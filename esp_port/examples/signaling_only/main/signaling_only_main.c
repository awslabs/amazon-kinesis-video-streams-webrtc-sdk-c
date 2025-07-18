#include <stdio.h>
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
#include "wifi_cli.h"

#include "app_storage.h"
#include "flash_wrapper.h"

#include "webrtc_bridge.h"
#include "esp_work_queue.h"
#include "kvs_signaling.h"
#include "webrtc_signaling_if.h"
#include "network_coprocessor.h"
#include "esp_webrtc_time.h"
#include "app_webrtc.h"
#include "signaling_serializer.h"

static const char *TAG = "signaling_only";

extern int sleep_command_register_cli();

int app_common_queryServer_get_by_idx(int index, uint8_t **data, int *len, bool *have_more)
{
    return -1;
}

// Global configuration - keep same structure as before for IoT Core compatibility
static KvsSignalingConfig g_kvsSignalingConfig = {0};

/**
 * @brief Handle WebRTC application events
 */
static void app_webrtc_event_handler(app_webrtc_event_data_t *event_data, void *user_ctx)
{
    ESP_LOGI(TAG, "WebRTC Event: %d, Status: 0x%08" PRIx32 ", Peer: %s, Message: %s",
             event_data->event_id, event_data->status_code,
             event_data->peer_id, event_data->message);

    switch (event_data->event_id) {
        case APP_WEBRTC_EVENT_SIGNALING_CONNECTED:
            ESP_LOGI(TAG, "Signaling connected successfully");
            break;
        case APP_WEBRTC_EVENT_SIGNALING_DISCONNECTED:
            ESP_LOGI(TAG, "Signaling disconnected");
            break;
        case APP_WEBRTC_EVENT_RECEIVED_OFFER:
            ESP_LOGI(TAG, "Received offer from peer %s", event_data->peer_id);
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
            ESP_LOGE(TAG, "WebRTC error occurred: %s", event_data->message);
            break;
        default:
            break;
    }
}

/**
 * @brief Callback to send signaling messages to the streaming device via webrtc_bridge
 */
static int send_message_callback(signaling_msg_t *signalingMessage)
{
    ESP_LOGI(TAG, "Sending signaling message type %d to streaming device", signalingMessage->messageType);

    size_t serializedMsgLen = 0;
    char *serializedMsg = serialize_signaling_message(signalingMessage, &serializedMsgLen);
    if (serializedMsg == NULL) {
        ESP_LOGE(TAG, "Failed to serialize signaling message");
        return -1;
    }

    // ownership of serializedMsg is transferred to webrtc_bridge_send_message
    webrtc_bridge_send_message(serializedMsg, serializedMsgLen);

    ESP_LOGI(TAG, "Successfully sent message type %d via bridge", signalingMessage->messageType);
    return 0;
}

/**
 * @brief Handle signaling messages received from the streaming device via webrtc_bridge
 */
static void handle_bridged_message(const void* data, int len)
{
    ESP_LOGI(TAG, "Received bridged message from streaming device (%d bytes)", len);

    signaling_msg_t signalingMsg = {0};
    esp_err_t ret = deserialize_signaling_message(data, len, &signalingMsg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to deserialize signaling message");
        return;
    }

    ESP_LOGI(TAG, "Deserialized message type %d from peer %s",
             signalingMsg.messageType, signalingMsg.peerClientId);

    // Send the message from bridge to signaling server
    int result = webrtcAppSendMessageToSignalingServer(&signalingMsg);
    if (result != 0) {
        ESP_LOGE(TAG, "Failed to send message to signaling server: %d", result);
    } else {
        ESP_LOGI(TAG, "Successfully sent message to signaling server");
    }

    // Clean up
    if (signalingMsg.payload != NULL) {
        free(signalingMsg.payload);
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

    // Register the message sending callback for split mode (sends to streaming device)
    webrtcAppRegisterSendToBridgeCallback(send_message_callback);

    // Register the bridge message handler to receive messages from streaming device
    webrtc_bridge_register_handler(handle_bridged_message);

    // Start the WebRTC bridge
    webrtc_bridge_start();

    // Configure WebRTC application for signaling-only mode with split mode support
    WebRtcAppConfig webrtcConfig = WEBRTC_APP_CONFIG_DEFAULT();

    // Configure signaling with KVS - pass the SAME config that was working
    webrtcConfig.pSignalingClientInterface = getKvsSignalingClientInterface();
    webrtcConfig.pSignalingConfig = &g_kvsSignalingConfig;

    // WebRTC configuration
    webrtcConfig.roleType = WEBRTC_SIGNALING_CHANNEL_ROLE_TYPE_MASTER;
    webrtcConfig.trickleIce = TRUE;
    webrtcConfig.useTurn = TRUE;
    webrtcConfig.logLevel = 3; // INFO level
    webrtcConfig.signalingOnly = TRUE; // Disable streaming components to save memory

    // No media interfaces needed for signaling-only mode
    webrtcConfig.videoCapture = NULL;
    webrtcConfig.audioCapture = NULL;
    webrtcConfig.videoPlayer = NULL;
    webrtcConfig.audioPlayer = NULL;
    webrtcConfig.receiveMedia = FALSE;

    ESP_LOGI(TAG, "Initializing WebRTC application");
    status = webrtcAppInit(&webrtcConfig);
    if (status != WEBRTC_STATUS_SUCCESS) {
        ESP_LOGE(TAG, "Failed to initialize WebRTC application: 0x%08" PRIx32, status);
        return;
    }

    ESP_LOGI(TAG, "Starting WebRTC application with split mode support");
    ESP_LOGI(TAG, "Signaling device ready - will forward messages to/from streaming device via bridge");

    status = webrtcAppRun();
    if (status != WEBRTC_STATUS_SUCCESS) {
        ESP_LOGE(TAG, "WebRTC application failed: 0x%08" PRIx32, status);
        webrtcAppTerminate();
    } else {
        ESP_LOGI(TAG, "WebRTC application started successfully");
    }

    ESP_LOGI(TAG, "Signaling-only example using app_webrtc state machine finished");
}
