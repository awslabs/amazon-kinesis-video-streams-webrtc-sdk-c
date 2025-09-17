/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * WebRTC Classic Example - Simplified API Demo
 *
 * This example demonstrates the new simplified WebRTC API design that:
 * - Uses reasonable defaults (MASTER role, OPUS/H264 codecs, trickle ICE, etc.)
 * - Requires only essential configuration (signaling + media interfaces)
 * - Auto-detects signaling-only vs. media streaming based on provided interfaces
 * - Provides advanced configuration APIs for power users (commented examples)
 *
 * BEFORE (complex): 12+ config fields to set manually
 * AFTER (simple):   6 essential fields with smart defaults
 */

#include <string.h>
#include <pthread.h>
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
#include "kvs_signaling.h"
#include "kvs_peer_connection.h"
#include "esp_webrtc_time.h"
#include "esp_work_queue.h"
#include "media_stream.h"


static const char *TAG = "webrtc_main";

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
        // s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

char* esp_get_ip(void)
{
    return wifi_ip;
}

#ifdef CONFIG_HOST_USES_STATIC_NETIF
esp_netif_t *create_slave_sta_netif_with_static_ip(void)
{
    ESP_LOGI(TAG, "Create netif with static IP");
     /* Create "almost" default station, but with un-flagged DHCP client */
    esp_netif_inherent_config_t netif_cfg;
    memcpy(&netif_cfg, ESP_NETIF_BASE_DEFAULT_WIFI_STA, sizeof(netif_cfg));
    netif_cfg.flags &= ~ESP_NETIF_DHCP_CLIENT;
    esp_netif_config_t cfg_sta = {
        .base = &netif_cfg,
        .stack = ESP_NETIF_NETSTACK_DEFAULT_WIFI_STA,
    };
    esp_netif_t *sta_netif = esp_netif_new(&cfg_sta);
    assert(sta_netif);

    ESP_LOGI(TAG, "Creating slave sta netif with static IP");

    ESP_ERROR_CHECK(esp_netif_attach_wifi_station(sta_netif));
    ESP_ERROR_CHECK(esp_wifi_set_default_wifi_sta_handlers());

    /* stop dhcpc */
    ESP_ERROR_CHECK(esp_netif_dhcpc_stop(sta_netif));

    // esp_netif_action_start(sta_netif, NULL, 0, NULL);
    // esp_netif_action_connected(sta_netif, 0, 0, 0);

    //esp_netif_up(netif);

    return sta_netif;
}
#endif

static void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
#ifdef CONFIG_HOST_USES_STATIC_NETIF
    create_slave_sta_netif_with_static_ip();
#else
    esp_netif_create_default_wifi_sta();
#endif

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

static void app_webrtc_event_handler(app_webrtc_event_data_t *event_data, void *user_ctx)
{
    if (event_data == NULL) {
        return;
    }

    switch (event_data->event_id) {
        case APP_WEBRTC_EVENT_INITIALIZED:
            ESP_LOGI(TAG, "[KVS Event] WebRTC Initialized.");
            break;
        case APP_WEBRTC_EVENT_DEINITIALIZING:
            ESP_LOGI(TAG, "[KVS Event] WebRTC Deinitialized.");
            break;
        case APP_WEBRTC_EVENT_SIGNALING_CONNECTING:
            ESP_LOGI(TAG, "[KVS Event] Signaling Connecting.");
            break;
        case APP_WEBRTC_EVENT_SIGNALING_CONNECTED:
            ESP_LOGI(TAG, "[KVS Event] Signaling Connected.");
            break;
        case APP_WEBRTC_EVENT_SIGNALING_DISCONNECTED:
            ESP_LOGI(TAG, "[KVS Event] Signaling Disconnected.");
            break;
        case APP_WEBRTC_EVENT_SIGNALING_DESCRIBE:
            ESP_LOGI(TAG, "[KVS Event] Signaling Describe.");
            break;
        case APP_WEBRTC_EVENT_SIGNALING_GET_ENDPOINT:
            ESP_LOGI(TAG, "[KVS Event] Signaling Get Endpoint.");
            break;
        case APP_WEBRTC_EVENT_SIGNALING_GET_ICE:
            ESP_LOGI(TAG, "[KVS Event] Signaling Get ICE.");
            break;
        case APP_WEBRTC_EVENT_PEER_CONNECTION_REQUESTED:
            ESP_LOGI(TAG, "[KVS Event] Peer Connection Requested.");
            break;
        case APP_WEBRTC_EVENT_PEER_CONNECTED:
            ESP_LOGI(TAG, "[KVS Event] Peer Connected: %s", event_data->peer_id);
            break;
        case APP_WEBRTC_EVENT_PEER_DISCONNECTED:
            ESP_LOGI(TAG, "[KVS Event] Peer Disconnected: %s", event_data->peer_id);
            break;
        case APP_WEBRTC_EVENT_STREAMING_STARTED:
            ESP_LOGI(TAG, "[KVS Event] Streaming Started for Peer: %s", event_data->peer_id);
            break;
        case APP_WEBRTC_EVENT_STREAMING_STOPPED:
            ESP_LOGI(TAG, "[KVS Event] Streaming Stopped for Peer: %s", event_data->peer_id);
            break;
        case APP_WEBRTC_EVENT_RECEIVED_OFFER:
            ESP_LOGI(TAG, "[KVS Event] Received Offer.");
            break;
        case APP_WEBRTC_EVENT_SENT_ANSWER:
            ESP_LOGI(TAG, "[KVS Event] Sent Answer.");
            break;
        case APP_WEBRTC_EVENT_ERROR:
            /* fall-through */
        case APP_WEBRTC_EVENT_SIGNALING_ERROR:
            /* fall-through */
        case APP_WEBRTC_EVENT_PEER_CONNECTION_FAILED:
            ESP_LOGE(TAG, "[KVS Event] Error Event %d: Code %d, Message: %s",
                     (int) event_data->event_id, (int) event_data->status_code, event_data->message);
            break;
        // Add cases for other events if needed
        default:
            ESP_LOGI(TAG, "[KVS Event] Unhandled Event ID: %d", (int) event_data->event_id);
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

    ESP_LOGI(TAG, "ESP32 WebRTC Example");

    esp_cli_start();

    // Initialize WiFi
    wifi_init_sta();

    app_storage_init();

    // Perform the time sync
    esp_webrtc_time_sntp_time_sync_and_wait();

    // Register the event callback *before* init to catch all events
    if (app_webrtc_register_event_callback(app_webrtc_event_handler, NULL) != 0) {
        ESP_LOGE(TAG, "Failed to register KVS event callback.");
    }

    // Get the media capture interfaces for sending audio/video
    media_stream_video_capture_t *video_capture = media_stream_get_video_capture_if();
    media_stream_audio_capture_t *audio_capture = media_stream_get_audio_capture_if();
    media_stream_video_player_t *video_player = media_stream_get_video_player_if();
    media_stream_audio_player_t *audio_player = media_stream_get_audio_player_if();

    if (video_capture == NULL || audio_capture == NULL ||
        video_player == NULL || audio_player == NULL) {
        ESP_LOGE(TAG, "Failed to get media interfaces");
        return;
    }

    // Set up KVS signaling configuration (as opaque config)
    static kvs_signaling_config_t kvs_signaling_cfg = {0};

    // Channel configuration
    kvs_signaling_cfg.pChannelName = CONFIG_AWS_KVS_CHANNEL_NAME;

#ifdef CONFIG_IOT_CORE_ENABLE_CREDENTIALS
    // Configure IoT Core credentials
    kvs_signaling_cfg.useIotCredentials = true;
    kvs_signaling_cfg.iotCoreCredentialEndpoint = CONFIG_AWS_IOT_CORE_CREDENTIAL_ENDPOINT;
    kvs_signaling_cfg.iotCoreCert = CONFIG_AWS_IOT_CORE_CERT;
    kvs_signaling_cfg.iotCorePrivateKey = CONFIG_AWS_IOT_CORE_PRIVATE_KEY;
    kvs_signaling_cfg.iotCoreRoleAlias = CONFIG_AWS_IOT_CORE_ROLE_ALIAS;
    kvs_signaling_cfg.iotCoreThingName = CONFIG_AWS_IOT_CORE_THING_NAME;
#else
    // Configure direct AWS credentials
    kvs_signaling_cfg.useIotCredentials = false;
    kvs_signaling_cfg.awsAccessKey = CONFIG_AWS_ACCESS_KEY_ID;
    kvs_signaling_cfg.awsSecretKey = CONFIG_AWS_SECRET_ACCESS_KEY;
    kvs_signaling_cfg.awsSessionToken = CONFIG_AWS_SESSION_TOKEN;
#endif

    // Set common AWS options
    kvs_signaling_cfg.awsRegion = "us-east-1";
    kvs_signaling_cfg.caCertPath = "/spiffs/certs/cacert.pem";

    // Configure WebRTC app with our new simplified API
    app_webrtc_config_t app_webrtc_config = APP_WEBRTC_CONFIG_DEFAULT();

    // Essential configuration - what you MUST provide
    app_webrtc_config.signaling_client_if = kvs_signaling_client_if_get();
    app_webrtc_config.signaling_cfg = &kvs_signaling_cfg;

    // Peer connection configuration - for full WebRTC functionality
    app_webrtc_config.peer_connection_if = kvs_peer_connection_if_get();
    // peer_connection_cfg can be NULL for default settings

    /*
     * 🏗️ Architecture Examples:
     *
     * webrtc_classic (this):    kvs_signaling + kvs_webrtc       (Full KVS)
     * esp_camera:               apprtc_signaling + kvs_webrtc    (Custom signaling + KVS peer)
     * streaming_only:           bridge_signaling + kvs_webrtc    (Bridge signaling + KVS peer)
     * signaling_only:           kvs_signaling + null_peer        (KVS signaling only)
     *
     * For signaling_only example:
     *   app_webrtc_config.signaling_client_if = kvs_signaling_client_if_get();
     *   app_webrtc_config.peer_connection_if = null_peer_connection_if_get();
     */

    // Media interfaces for sending (optional - NULL for signaling-only)
    app_webrtc_config.video_capture = video_capture;
    app_webrtc_config.audio_capture = audio_capture;
    // app_webrtc_config.video_player = NULL;
    app_webrtc_config.audio_player = audio_player;

    ESP_LOGI(TAG, "Initializing WebRTC Classic Example - Full KVS Integration");
    ESP_LOGI(TAG, "  - Architecture: app_webrtc + kvs_webrtc + kvs_signaling");
    ESP_LOGI(TAG, "  - Signaling: AWS KVS WebSocket signaling");
    ESP_LOGI(TAG, "  - Peer Connections: Full KVS WebRTC peer connections");
    ESP_LOGI(TAG, "  - Role: MASTER (initiates connections)");
    ESP_LOGI(TAG, "  - Trickle ICE: enabled (faster connection setup)");
    ESP_LOGI(TAG, "  - TURN servers: enabled (better NAT traversal)");
    ESP_LOGI(TAG, "  - Audio codec: OPUS, Video codec: H264");
    ESP_LOGI(TAG, "  - Media reception: enabled for bidirectional streaming");

    // Initialize WebRTC application with simplified API
    status = app_webrtc_init(&app_webrtc_config);
    if (status != WEBRTC_STATUS_SUCCESS) {
        ESP_LOGE(TAG, "Failed to initialize WebRTC application: 0x%08" PRIx32, (uint32_t) status);
        return;
    }

    // Example: Advanced configuration APIs (optional)
    // Uncomment any of these if you need to change defaults:
    //
    // app_webrtc_set_role(WEBRTC_CHANNEL_ROLE_TYPE_VIEWER);  // Change to viewer role
    // app_webrtc_set_log_level(2);                                     // Enable DEBUG logging
    app_webrtc_enable_media_reception(true);                         // Enable receiving media
    // app_webrtc_set_ice_config(false, false);                         // Disable trickle ICE and TURN

    ESP_LOGI(TAG, "Running WebRTC application");

    // Run WebRTC application
    status = app_webrtc_run();
    if (status != WEBRTC_STATUS_SUCCESS) {
        ESP_LOGE(TAG, "WebRTC application failed: 0x%08" PRIx32, (uint32_t) status);
        app_webrtc_terminate();
    } else {
        ESP_LOGI(TAG, "WebRTC application started successfully");
    }
}
