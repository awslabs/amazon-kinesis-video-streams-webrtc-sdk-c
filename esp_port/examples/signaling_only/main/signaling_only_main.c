#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
// #include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"

#include "esp_cli.h"
#include "wifi_cli.h"

#include "app_storage.h"
#include "esp_work_queue.h"
#include "kvs_signaling.h"
#include "network_coprocessor.h"
#include "esp_webrtc_time.h"
#include "app_webrtc.h"
#include "signaling_bridge_adapter.h"

static const char *TAG = "signaling_only";

extern int sleep_command_register_cli();

// Global configuration - keep same structure as before for IoT Core compatibility
static kvs_signaling_config_t g_kvsSignalingConfig = {0};

/**
 * @brief Handle WebRTC application events
 */
static void app_webrtc_event_handler(app_webrtc_event_data_t *event_data, void *user_ctx)
{
    ESP_LOGI(TAG, "🎯 WebRTC Event: %d, Status: 0x%08" PRIx32 ", Peer: %s, Message: %s",
             event_data->event_id, event_data->status_code,
             event_data->peer_id ? event_data->peer_id : "NULL",
             event_data->message ? event_data->message : "NULL");

    switch (event_data->event_id) {
        case APP_WEBRTC_EVENT_SIGNALING_CONNECTED:
            ESP_LOGI(TAG, "Signaling connected successfully");
            // Note: ICE servers are now transferred on-demand when streaming device requests them
            break;
        case APP_WEBRTC_EVENT_SIGNALING_GET_ICE:
            ESP_LOGI(TAG, "ICE servers fetched from AWS and ready for requests");
            break;
        case APP_WEBRTC_EVENT_SIGNALING_DISCONNECTED:
            ESP_LOGI(TAG, "Signaling disconnected");
            break;
        case APP_WEBRTC_EVENT_RECEIVED_OFFER:
            ESP_LOGI(TAG, "Received offer from peer %s", event_data->peer_id);
            // Note: ICE servers are now provided on-demand when requested by streaming device
            break;
        case APP_WEBRTC_EVENT_SENT_ANSWER:
            ESP_LOGI(TAG, "Sent answer to peer %s", event_data->peer_id);
            break;
        case APP_WEBRTC_EVENT_RECEIVED_ICE_CANDIDATE:
            ESP_LOGI(TAG, "Received ICE candidate from peer %s", event_data->peer_id);
            break;
        case APP_WEBRTC_EVENT_SENT_ICE_CANDIDATE:
            ESP_LOGI(TAG, "Sent ICE candidate to peer %s", event_data->peer_id);
            break;
        case APP_WEBRTC_EVENT_ERROR:
        case APP_WEBRTC_EVENT_SIGNALING_ERROR:
            ESP_LOGE(TAG, "WebRTC error occurred: %s", event_data->message ? event_data->message : "Unknown error");
            break;
        default:
            ESP_LOGI(TAG, "🔍 Unhandled WebRTC event: %d", event_data->event_id);
            break;
    }
}

volatile static bool ip_event_got_ip = false;
static void event_handler_ip(void* arg, esp_event_base_t event_base,
                             int32_t event_id, void* event_data)
{
	if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ESP_LOGI(TAG, "IP event got IP");
        ip_event_got_ip = true;
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

    ESP_LOGI(TAG, "ESP32 WebRTC Signaling-Only Example (Using App WebRTC State Machine)");

    esp_cli_start();
    wifi_register_cli(); // for wifi-set command

#if CONFIG_IDF_TARGET_ESP32C6
    /* Initialize network co-processor */
    network_coprocessor_init();
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler_ip,
                                                        NULL,
                                                        NULL));
#endif

    app_storage_init();

    // Initialize signaling serializer (needed for bridge communication)
    signaling_serializer_init();

    int count = 0;
    while (!ip_event_got_ip) {
        vTaskDelay(pdMS_TO_TICKS(100));
        count++;
        if (count == 10) {
            ESP_LOGI(TAG, "Waiting for Got IP event...");
            count = 0;
        }
    }

    // Perform the time sync
    esp_webrtc_time_sntp_time_sync_and_wait();

    if (esp_work_queue_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize work queue");
        return;
    }

    if (esp_work_queue_start() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start work queue");
        return;
    }

    ESP_LOGI(TAG, "Setting up WebRTC application with KVS signaling and app_webrtc state machine");

    g_kvsSignalingConfig.pChannelName = CONFIG_AWS_KVS_CHANNEL_NAME;
    g_kvsSignalingConfig.useIotCredentials = false; // Use static credentials for now
    g_kvsSignalingConfig.awsRegion = CONFIG_AWS_DEFAULT_REGION;
    g_kvsSignalingConfig.caCertPath = "/spiffs/certs/cacert.pem";

#ifdef CONFIG_IOT_CORE_ENABLE_CREDENTIALS
    ESP_LOGI(TAG, "Using IoT Core credentials");
    g_kvsSignalingConfig.useIotCredentials = true;
    g_kvsSignalingConfig.iotCoreCredentialEndpoint = CONFIG_AWS_IOT_CORE_CREDENTIAL_ENDPOINT;
    g_kvsSignalingConfig.iotCoreCert = CONFIG_AWS_IOT_CORE_CERT;
    g_kvsSignalingConfig.iotCorePrivateKey = CONFIG_AWS_IOT_CORE_PRIVATE_KEY;
    g_kvsSignalingConfig.iotCoreRoleAlias = CONFIG_AWS_IOT_CORE_ROLE_ALIAS;
    g_kvsSignalingConfig.iotCoreThingName = CONFIG_AWS_IOT_CORE_THING_NAME;
#else
    ESP_LOGI(TAG, "Using static AWS credentials");
    g_kvsSignalingConfig.awsAccessKey = CONFIG_AWS_ACCESS_KEY_ID;
    g_kvsSignalingConfig.awsSecretKey = CONFIG_AWS_SECRET_ACCESS_KEY;
    g_kvsSignalingConfig.awsSessionToken = CONFIG_AWS_SESSION_TOKEN;
#endif

    // Register WebRTC event callback
    app_webrtc_register_event_callback(app_webrtc_event_handler, NULL);

    // Configure and initialize signaling bridge adapter (handles all bridge communication)
    signaling_bridge_adapter_config_t adapter_config = {
        .user_ctx = NULL
    };

    WEBRTC_STATUS adapter_status = signaling_bridge_adapter_init(&adapter_config);
    if (adapter_status != WEBRTC_STATUS_SUCCESS) {
        ESP_LOGE(TAG, "Failed to initialize signaling bridge adapter: 0x%08" PRIx32, adapter_status);
        return;
    }

    // Start the signaling bridge adapter (starts bridge and handles all communication)
    adapter_status = signaling_bridge_adapter_start();
    if (adapter_status != WEBRTC_STATUS_SUCCESS) {
        ESP_LOGE(TAG, "Failed to start signaling bridge adapter: 0x%08" PRIx32, adapter_status);
        return;
    }

    // Configure WebRTC with our new simplified API - signaling-only mode
    app_webrtc_config_t app_webrtc_config = APP_WEBRTC_CONFIG_DEFAULT();

    // Essential configuration - what you MUST provide
    app_webrtc_config.signaling_client_if = kvs_signaling_client_if_get();
    app_webrtc_config.signaling_cfg = &g_kvsSignalingConfig;

    // No media interfaces = signaling-only mode (auto-detected!)
    // video_capture, audio_capture, video_player, audio_player all default to NULL

    ESP_LOGI(TAG, "Initializing WebRTC in signaling-only mode with simplified API:");
    ESP_LOGI(TAG, "  - Mode: auto-detected as signaling-only (no media interfaces)");
    ESP_LOGI(TAG, "  - Role: MASTER (default - manages connections)");
    ESP_LOGI(TAG, "  - Memory: optimized (media components disabled automatically)");
    ESP_LOGI(TAG, "  - Split mode: ready for bridge communication");
    ESP_LOGI(TAG, "  - Channel: %s", g_kvsSignalingConfig.pChannelName);

    status = app_webrtc_init(&app_webrtc_config);
    if (status != WEBRTC_STATUS_SUCCESS) {
        ESP_LOGE(TAG, "Failed to initialize WebRTC application: 0x%08" PRIx32, (uint32_t) status);
        return;
    }

    ESP_LOGI(TAG, "Starting signaling-only WebRTC application");
    ESP_LOGI(TAG, "Ready to forward signaling messages to/from streaming device via bridge");

    status = app_webrtc_run();
    if (status != WEBRTC_STATUS_SUCCESS) {
        ESP_LOGE(TAG, "WebRTC application failed: 0x%08" PRIx32, (uint32_t) status);
        app_webrtc_terminate();
    } else {
        ESP_LOGI(TAG, "WebRTC application started successfully");
    }

    ESP_LOGI(TAG, "Signaling-only example using app_webrtc state machine finished");
}
