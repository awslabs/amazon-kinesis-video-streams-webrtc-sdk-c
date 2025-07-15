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
#include "app_storage.h"

#include "app_webrtc.h"
#include "esp_webrtc_time.h"
#include "esp_work_queue.h"
#include "media_stream.h"
#include "apprtc_signaling.h"
#include "wifi_cli.h"
#include "webrtc_cli.h"
#include "signaling_serializer.h"
#include "signaling_conversion.h"

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
 * @brief Handle signaling messages from AppRTC and forward them to WebRTC SDK
 *
 * This callback is registered with apprtc_signaling and receives messages
 * from the AppRTC signaling server. It converts them to the format expected
 * by the WebRTC SDK and forwards them.
 */
static void handle_signaling_message(const char *message, size_t message_len, void *user_data)
{
    ESP_LOGI(TAG, "Received signaling message from AppRTC");

    // Forward the message to the signaling conversion module
    // The conversion module will parse the message and convert it to the format
    // expected by the WebRTC SDK, then forward it to webrtcAppSignalingMessageReceived
    apprtc_signaling_process_message(message, message_len);
}

/**
 * @brief Handle state changes in the AppRTC signaling connection
 *
 * This callback is registered with apprtc_signaling and receives state
 * change notifications from the AppRTC signaling connection.
 */
static void handle_signaling_state_change(int state, void *user_data)
{
    ESP_LOGI(TAG, "AppRTC signaling state changed to: %d", state);

    switch (state) {
        case APPRTC_SIGNALING_STATE_CONNECTED:
            ESP_LOGI(TAG, "Connected to AppRTC signaling server");
            ESP_LOGI(TAG, "Room ID: %s", apprtc_signaling_get_room_id());
            break;

        case APPRTC_SIGNALING_STATE_DISCONNECTED:
            ESP_LOGW(TAG, "Disconnected from AppRTC signaling server");
            break;

        case APPRTC_SIGNALING_STATE_ERROR:
            ESP_LOGE(TAG, "Error in AppRTC signaling");
            break;

        default:
            break;
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
    STATUS status = STATUS_SUCCESS;

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

    app_storage_init();

    esp_work_queue_config_t work_queue_config = ESP_WORK_QUEUE_CONFIG_DEFAULT();
    work_queue_config.stack_size = 60 * 1024;

    // Initialize work queue for background tasks
    if (esp_work_queue_init_with_config(&work_queue_config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize work queue");
        return;
    }

    if (esp_work_queue_start() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start work queue");
        return;
    }

    // Initialize signaling serializer for message format conversion
    signaling_serializer_init();

    // Initialize AppRTC signaling
    ret = apprtc_signaling_init(handle_signaling_message, handle_signaling_state_change, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize AppRTC signaling");
        return;
    }

    // Register the WebRTC event callback to receive events from the WebRTC SDK
    if (app_webrtc_register_event_callback(app_webrtc_event_handler, NULL) != 0) {
        ESP_LOGE(TAG, "Failed to register WebRTC event callback");
    }

    // Register the custom signaling message sender with the WebRTC SDK
    if (apprtc_signaling_register_with_webrtc() != 0) {
        ESP_LOGE(TAG, "Failed to register signaling message sender");
        return;
    }

    // Get the media capture interfaces
    media_stream_video_capture_t *video_capture = media_stream_get_video_capture_if();
    media_stream_audio_capture_t *audio_capture = media_stream_get_audio_capture_if();
    media_stream_video_player_t *video_player = media_stream_get_video_player_if();
    media_stream_audio_player_t *audio_player = media_stream_get_audio_player_if();

    if (video_capture == NULL || audio_capture == NULL ||
        video_player == NULL || audio_player == NULL) {
        ESP_LOGE(TAG, "Failed to get media interfaces");
        return;
    }

    // Configure WebRTC app
    WebRtcAppConfig webrtcConfig = WEBRTC_APP_CONFIG_DEFAULT();
    webrtcConfig.pChannelName = "AppRTCChannel";
    webrtcConfig.mode = APP_WEBRTC_STREAMING_ONLY_MODE;  // Use streaming-only mode with custom signaling

    // Set role type based on configuration
#if CONFIG_APPRTC_ROLE_TYPE == 0
    // This mode can be used when you want to connect to the existing room.
    // Will then receive the offer from the other peer and send the answer.
    webrtcConfig.roleType = SIGNALING_CHANNEL_ROLE_TYPE_MASTER;
    ESP_LOGI(TAG, "Configured as MASTER role");
#else
    // In this mode, the application will send the offer and wait for the answer.
    webrtcConfig.roleType = SIGNALING_CHANNEL_ROLE_TYPE_VIEWER;
    ESP_LOGI(TAG, "Configured as VIEWER role");
#endif

    // Pass the media capture interfaces
    webrtcConfig.videoCapture = video_capture;
    webrtcConfig.audioCapture = audio_capture;
    webrtcConfig.videoPlayer = video_player;
    webrtcConfig.audioPlayer = audio_player;
    webrtcConfig.mediaType = APP_WEBRTC_MEDIA_AUDIO_VIDEO;
    webrtcConfig.receiveMedia = TRUE;  // Enable media reception

    ESP_LOGI(TAG, "Initializing WebRTC application");

    // Initialize WebRTC application
    status = webrtcAppInit(&webrtcConfig);
    if (status != STATUS_SUCCESS) {
        ESP_LOGE(TAG, "Failed to initialize WebRTC application: 0x%08" PRIx32, status);
        return;
    }

#if CONFIG_APPRTC_AUTO_CONNECT
#if CONFIG_APPRTC_ROLE_TYPE == 0
    // MASTER always creates a new room
    ret = apprtc_signaling_connect(NULL);  // NULL to create a new room
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to connect to AppRTC signaling server: %s", esp_err_to_name(ret));
        webrtcAppTerminate();
        return;
    }

    // Show QR code or web URL for connecting to the room
    const char* room_id = apprtc_signaling_get_room_id();
    if (room_id) {
        ESP_LOGI(TAG, "------------------------------------------------------------");
        ESP_LOGI(TAG, "| WebRTC Room URL: https://webrtc.espressif.com/r/%s", room_id);
        ESP_LOGI(TAG, "| Scan the QR code or visit the URL to connect to this device");
        ESP_LOGI(TAG, "------------------------------------------------------------");
    } else {
        ESP_LOGW(TAG, "Room ID not available yet. Wait for signaling connection to complete.");
    }
#else
    // For VIEWER role, if fixed room ID is configured, join that room
#if CONFIG_APPRTC_USE_FIXED_ROOM
    ret = apprtc_signaling_connect(CONFIG_APPRTC_ROOM_ID);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to connect to room %s: %s", CONFIG_APPRTC_ROOM_ID, esp_err_to_name(ret));
        webrtcAppTerminate();
        return;
    }
    ESP_LOGI(TAG, "Joining fixed room: %s", CONFIG_APPRTC_ROOM_ID);
#else
    // For VIEWER role without fixed room, create a new room
    ret = apprtc_signaling_connect(NULL);  // NULL to create a new room
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create a new room: %s", esp_err_to_name(ret));
        webrtcAppTerminate();
        return;
    }

    // Show QR code or web URL for connecting to the room
    const char* room_id = apprtc_signaling_get_room_id();
    if (room_id) {
        ESP_LOGI(TAG, "------------------------------------------------------------");
        ESP_LOGI(TAG, "| WebRTC Room URL: https://webrtc.espressif.com/r/%s", room_id);
        ESP_LOGI(TAG, "| Scan the QR code or visit the URL to connect to this device");
        ESP_LOGI(TAG, "------------------------------------------------------------");
    } else {
        ESP_LOGW(TAG, "Room ID not available yet. Wait for signaling connection to complete.");
    }
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

    ESP_LOGI(TAG, "Running WebRTC application");

    // Run WebRTC application
    status = webrtcAppRun();
    if (status != STATUS_SUCCESS) {
        ESP_LOGE(TAG, "WebRTC application failed: 0x%08" PRIx32, status);
        apprtc_signaling_disconnect();
        webrtcAppTerminate();
    } else {
        ESP_LOGI(TAG, "WebRTC application started successfully");
    }
}
