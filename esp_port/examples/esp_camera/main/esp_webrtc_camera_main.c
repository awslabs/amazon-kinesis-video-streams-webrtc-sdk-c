/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"

#include "esp_cli.h"

#include "app_webrtc.h"
#include "media_stream.h"
#include "apprtc_signaling.h"
#include "wifi_cli.h"
#include "webrtc_cli.h"
#include "app_webrtc_if.h"
#include "kvs_peer_connection.h"

static const char *TAG = "esp_webrtc_camera";

// WiFi event group
static EventGroupHandle_t s_wifi_event_group;
static char wifi_ip[72];

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static void event_handler(void* arg, esp_event_base_t event_base,
                         int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;

        memset(wifi_ip, 0, sizeof(wifi_ip)/sizeof(wifi_ip[0]));
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        memcpy(wifi_ip, &event->ip_info.ip, 4);
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_ESP_WIFI_SSID,
            .password = CONFIG_ESP_WIFI_PASSWORD,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Waiting for WiFi connection");
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            pdMS_TO_TICKS(20000));

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to WiFi");
    } else {
        ESP_LOGE(TAG, "Failed to connect to WiFi");
    }
}

/**
 * @brief Handle WebRTC events from the WebRTC SDK
 *
 * This callback is registered with the WebRTC SDK and receives events
 * such as peer connection state changes, ICE gathering completion, etc.
 */
static void app_webrtc_event_handler(app_webrtc_event_data_t *event_data, void *user_ctx)
{
    if (event_data == NULL) {
        return;
    }

    switch (event_data->event_id) {
        case APP_WEBRTC_EVENT_INITIALIZED:
            ESP_LOGI(TAG, "[WebRTC Event] WebRTC Initialized.");
            break;
        case APP_WEBRTC_EVENT_DEINITIALIZING:
            ESP_LOGI(TAG, "[WebRTC Event] WebRTC Deinitialized.");
            break;
        case APP_WEBRTC_EVENT_SIGNALING_CONNECTING:
            ESP_LOGI(TAG, "[WebRTC Event] Signaling Connecting.");
            break;
        case APP_WEBRTC_EVENT_SIGNALING_CONNECTED:
            ESP_LOGI(TAG, "[WebRTC Event] Signaling Connected.");
            break;
        case APP_WEBRTC_EVENT_SIGNALING_DISCONNECTED:
            ESP_LOGI(TAG, "[WebRTC Event] Signaling Disconnected.");
            break;
        case APP_WEBRTC_EVENT_PEER_CONNECTION_REQUESTED:
            ESP_LOGI(TAG, "[WebRTC Event] Peer Connection Requested.");
            break;
        case APP_WEBRTC_EVENT_RECEIVED_OFFER:
            ESP_LOGI(TAG, "[WebRTC Event] Received Offer.");
            break;
        case APP_WEBRTC_EVENT_SENT_ANSWER:
            ESP_LOGI(TAG, "[WebRTC Event] Sent Answer.");
            break;
        case APP_WEBRTC_EVENT_ICE_GATHERING_COMPLETE:
            ESP_LOGI(TAG, "[WebRTC Event] ICE Gathering Complete.");
            break;
        case APP_WEBRTC_EVENT_PEER_CONNECTED:
            ESP_LOGI(TAG, "[WebRTC Event] Peer Connected: %s", event_data->peer_id);
            break;
        case APP_WEBRTC_EVENT_PEER_DISCONNECTED:
            ESP_LOGI(TAG, "[WebRTC Event] Peer Disconnected: %s", event_data->peer_id);
            break;
        case APP_WEBRTC_EVENT_STREAMING_STARTED:
            ESP_LOGI(TAG, "[WebRTC Event] Streaming Started for Peer: %s", event_data->peer_id);
            break;
        case APP_WEBRTC_EVENT_STREAMING_STOPPED:
            ESP_LOGI(TAG, "[WebRTC Event] Streaming Stopped for Peer: %s", event_data->peer_id);
            break;
        case APP_WEBRTC_EVENT_ERROR:
        case APP_WEBRTC_EVENT_SIGNALING_ERROR:
        case APP_WEBRTC_EVENT_PEER_CONNECTION_FAILED:
            ESP_LOGE(TAG, "[WebRTC Event] Error Event %d: Code %d, Message: %s",
                     (int) event_data->event_id, (int) event_data->status_code, event_data->message);
            break;
        default:
            ESP_LOGI(TAG, "[WebRTC Event] Unhandled Event ID: %d", (int) event_data->event_id);
            break;
    }
}

void app_main(void)
{
    esp_err_t ret;
    WEBRTC_STATUS status = WEBRTC_STATUS_SUCCESS;

    // Initialize NVS
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "ESP32 WebRTC Camera Example");

    esp_cli_start();

    // Initialize WiFi
    wifi_init_sta();
    wifi_register_cli();
    webrtc_register_cli();

    // Register the WebRTC event callback to receive events from the WebRTC SDK
    if (app_webrtc_register_event_callback(app_webrtc_event_handler, NULL) != 0) {
        ESP_LOGE(TAG, "Failed to register WebRTC event callback");
    }

    // Get the media capture interfaces
    media_stream_video_capture_t *video_capture = media_stream_get_video_capture_if();
    media_stream_audio_capture_t *audio_capture = media_stream_get_audio_capture_if();
    media_stream_video_player_t *video_player = media_stream_get_video_player_if();
    media_stream_audio_player_t *audio_player = media_stream_get_audio_player_if();

#ifdef CONFIG_ESP_P4_CORE_BOARD
    audio_capture = NULL;
    video_player = NULL;
    audio_player = NULL;
#endif

    if (video_capture == NULL) {
        ESP_LOGW(TAG, "Video capture not available - continuing without video capture");
    }
    if (video_player == NULL) {
        ESP_LOGW(TAG, "Video player not available - continuing without video player");
    }

    if (audio_capture == NULL) {
        ESP_LOGW(TAG, "Audio capture not available - continuing without audio capture");
    }

    if (audio_player == NULL) {
        ESP_LOGW(TAG, "Audio player not available - continuing without audio player");
    }

    // Configure AppRTC signaling
    apprtc_signaling_config_t apprtc_config = {
        .serverUrl = NULL,  // Use default AppRTC server
        .roomId = NULL,     // Will be set based on role type
        .autoConnect = false,
        .connectionTimeout = 30000,
        .logLevel = 3
    };

    // Configure WebRTC app with our new simplified API
    app_webrtc_config_t app_webrtc_config = APP_WEBRTC_CONFIG_DEFAULT();

    // Essential configuration - signaling interface
    app_webrtc_config.signaling_client_if = apprtc_signaling_client_if_get();
    app_webrtc_config.signaling_cfg = &apprtc_config;

    // Peer connection interface
    app_webrtc_config.peer_connection_if = kvs_peer_connection_if_get();
    app_webrtc_config.implementation_config = NULL;

    // Media interfaces for bi-directional streaming
    app_webrtc_config.video_capture = video_capture;
    app_webrtc_config.audio_capture = audio_capture;
    app_webrtc_config.video_player = video_player;
    app_webrtc_config.audio_player = audio_player;

    ESP_LOGI(TAG, "Initializing WebRTC application with simplified API:");
    ESP_LOGI(TAG, "  - Media type: auto-detected (audio+video from interfaces)");
    ESP_LOGI(TAG, "  - AppRTC signaling: browser-compatible");
    ESP_LOGI(TAG, "  - Streaming: bi-directional (can send and receive)");

    // Initialize WebRTC application with simplified API
    status = app_webrtc_init(&app_webrtc_config);
    if (status != WEBRTC_STATUS_SUCCESS) {
        ESP_LOGE(TAG, "Failed to initialize WebRTC application: 0x%08" PRIx32, (uint32_t) status);
        return;
    }

    // Advanced configuration: Set role type based on configuration
#if CONFIG_APPRTC_ROLE_TYPE == 0
    // This mode can be used when you want to connect to the existing room.
    // Will then receive the offer from the other peer and send the answer.
    app_webrtc_set_role(WEBRTC_CHANNEL_ROLE_TYPE_MASTER);
    ESP_LOGI(TAG, "Configured as MASTER role using advanced API");
#else
    // In this mode, the application will send the offer and wait for the answer.
    app_webrtc_set_role(WEBRTC_CHANNEL_ROLE_TYPE_VIEWER);
    ESP_LOGI(TAG, "Configured as VIEWER role using advanced API");
#endif

    // Enable media reception for bi-directional streaming
    app_webrtc_enable_media_reception(true);

    // Start the WebRTC application (this will handle signaling connection)
    status = app_webrtc_run();
    if (status != WEBRTC_STATUS_SUCCESS) {
        ESP_LOGE(TAG, "Failed to start WebRTC application: 0x%08" PRIx32, (uint32_t) status);
        app_webrtc_terminate();
        return;
    }

#if CONFIG_APPRTC_AUTO_CONNECT
#if CONFIG_APPRTC_ROLE_TYPE == 0
    // For MASTER role, the WebRTC app will automatically create and connect to a new room
    ESP_LOGI(TAG, "MASTER role - will create a new room automatically");
#else
    // For VIEWER role, configure room ID if specified
#if CONFIG_APPRTC_USE_FIXED_ROOM
    // Set the room ID to join in the AppRTC configuration
    apprtc_config.roomId = CONFIG_APPRTC_ROOM_ID;
    ESP_LOGI(TAG, "VIEWER role - will join fixed room: %s", CONFIG_APPRTC_ROOM_ID);
#else
    // For VIEWER role without fixed room, will create a new room
    ESP_LOGI(TAG, "VIEWER role - will create a new room");
#endif
#endif
#else
    // Manual connection mode - user must use CLI commands
    ESP_LOGI(TAG, "------------------------------------------------------------");
    ESP_LOGI(TAG, "| WebRTC connection ready. Use CLI commands to connect:    |");
    ESP_LOGI(TAG, "| - To create a new room:  join-room new                   |");
    ESP_LOGI(TAG, "| - To join existing room: join-room <room_id>             |");
    ESP_LOGI(TAG, "| - To check current room: get-room                        |");
    ESP_LOGI(TAG, "| - To disconnect:         disconnect                      |");
    ESP_LOGI(TAG, "------------------------------------------------------------");
#endif

    ESP_LOGI(TAG, "------------------------------------------------------------");
    ESP_LOGI(TAG, "| WebRTC application initialized and ready!                |");
    ESP_LOGI(TAG, "| The signaling will connect automatically                 |");
    ESP_LOGI(TAG, "| Room URL will be displayed once connection is established|");
    ESP_LOGI(TAG, "------------------------------------------------------------");
}
