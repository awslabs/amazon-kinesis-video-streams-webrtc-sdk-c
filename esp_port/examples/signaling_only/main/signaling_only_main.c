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

#include "signaling_serializer.h"
#include "webrtc_bridge.h"
#include "esp_work_queue.h"
#include "app_webrtc.h"
#include "network_coprocessor.h"
#include "esp_webrtc_time.h"

static const char *TAG = "signaling_only";

// WiFi event group
// static EventGroupHandle_t s_wifi_event_group;
// static char wifi_ip[72];

// #define WIFI_CONNECTED_BIT BIT0
// #define WIFI_FAIL_BIT      BIT1

extern int sleep_command_register_cli();

int app_common_queryServer_get_by_idx(int index, uint8_t **data, int *len, bool *have_more)
{
    return -1;
}

/**
 * @brief Handle bridged signaling received via webrtc_bridge
 *
 * Signaling messages received via webrtc_bridge need to be sent to the signaling client
 * Messages are deserialized, converted to SignalingMessage and sent to the signaling client
 *
 * @note The messages to be sent via bridge (to streaming_only component) are serialized and
 * sent by app_webrtc
 *
 * @param data The data to handle
 * @param len The length of the data
 */
static void handle_bridged_message(const void* data, int len)
{
    signaling_msg_t signalingMsg = {0};
    esp_err_t ret = deserialize_signaling_message(data, len, &signalingMsg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to deserialize signaling message from bridge");
        return;
    }

    ESP_LOGI(TAG, "Received signaling message type: %d", signalingMsg.messageType);

    webrtcAppSignalingMessageReceived(&signalingMsg);

    if (signalingMsg.payload != NULL) {
        // If the payload is not NULL, means ownership is with caller
        free(signalingMsg.payload);
    }
}

// function type: app_webrtc_send_msg_cb_t is defined in app_webrtc.h
static int send_message_callback(signaling_msg_t *signalingMessage)
{
    size_t serializedMsgLen = 0;
    char *serializedMsg = serialize_signaling_message(signalingMessage, &serializedMsgLen);
    webrtc_bridge_send_message(serializedMsg, serializedMsgLen);

    // Do not free the serialized message, it is freed by the webrtc bridge
    return 0;
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
        case APP_WEBRTC_EVENT_ERROR:
            /* fall-through */
        case APP_WEBRTC_EVENT_SIGNALING_ERROR:
            ESP_LOGE(TAG, "[KVS Event] Error Event %d: Code %d, Message: %s",
                     (int) event_data->event_id, (int) event_data->status_code, event_data->message);
            break;
        default:
            ESP_LOGI(TAG, "[KVS Event] Unhandled Event ID: %d", (int) event_data->event_id);
            break;
    }
}

void app_main(void)
{
    esp_err_t ret;
    STATUS status;

    // Initialize NVS
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "ESP32 WebRTC Signaling Example");

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

    // Initialize signaling serializer
    signaling_serializer_init();

    // Register the bridge message handler!
    // We get the messages from the bridge here, deserialize and send them to the KVS SDK
    webrtc_bridge_register_handler(handle_bridged_message);

    // Register the message sending callback!
    // Messages from the KVS SDK are received in this callback, serialized and sent to the bridge
    webrtcAppRegisterSendMessageCallback(send_message_callback);

    // Start webrtc bridge
    webrtc_bridge_start();

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

    // Register the event callback *before* init to catch all events
    if (app_webrtc_register_event_callback(app_webrtc_event_handler, NULL) != 0) {
        ESP_LOGE(TAG, "Failed to register KVS event callback.");
    }

    WebRtcAppConfig webrtcConfig = {0};
    // Configure WebRTC app
    webrtcConfig.pChannelName = CONFIG_AWS_KVS_CHANNEL_NAME;
    webrtcConfig.roleType = SIGNALING_CHANNEL_ROLE_TYPE_MASTER;
    webrtcConfig.mode = APP_WEBRTC_SIGNALING_ONLY_MODE;

#ifdef CONFIG_IOT_CORE_ENABLE_CREDENTIALS
    // Configure IoT Core credentials
    webrtcConfig.useIotCredentials = TRUE;
    webrtcConfig.iotCoreCredentialEndpoint = CONFIG_AWS_IOT_CORE_CREDENTIAL_ENDPOINT;
    webrtcConfig.iotCoreCert = CONFIG_AWS_IOT_CORE_CERT;
    webrtcConfig.iotCorePrivateKey = CONFIG_AWS_IOT_CORE_PRIVATE_KEY;
    webrtcConfig.iotCoreRoleAlias = CONFIG_AWS_IOT_CORE_ROLE_ALIAS;
    webrtcConfig.iotCoreThingName = CONFIG_AWS_IOT_CORE_THING_NAME;
#else
    // Configure direct AWS credentials
    webrtcConfig.useIotCredentials = FALSE;
    webrtcConfig.awsAccessKey = CONFIG_AWS_ACCESS_KEY_ID;
    webrtcConfig.awsSecretKey = CONFIG_AWS_SECRET_ACCESS_KEY;
    webrtcConfig.awsSessionToken = CONFIG_AWS_SESSION_TOKEN;
#endif

    // Set common AWS options
    webrtcConfig.awsRegion = CONFIG_AWS_DEFAULT_REGION;
    webrtcConfig.caCertPath = "/spiffs/certs/cacert.pem";
    webrtcConfig.logLevel = 1;

    ESP_LOGI(TAG, "Initializing WebRTC application");

    // Initialize WebRTC application
    status = webrtcAppInit(&webrtcConfig);
    if (status != STATUS_SUCCESS) {
        ESP_LOGE(TAG, "Failed to initialize WebRTC application: 0x%08" PRIx32, status);
        return;
    }

    ESP_LOGI(TAG, "Running WebRTC application");

    // Run WebRTC application
    status = webrtcAppRun();
    if (status != STATUS_SUCCESS) {
        ESP_LOGE(TAG, "WebRTC application failed: 0x%08" PRIx32, status);
    } else {
        ESP_LOGI(TAG, "WebRTC application started successfully");
    }
}
