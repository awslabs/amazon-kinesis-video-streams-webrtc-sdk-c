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
#include "app_storage.h"
#include "flash_wrapper.h"

#include "signaling_serializer.h"
#include "webrtc_bridge.h"
#include "esp_work_queue.h"
#include "app_webrtc.h"
// #include "network_coprocessor.h"
#include "esp_webrtc_time.h"

static const char *TAG = "signaling_only";

// WiFi event group
// static EventGroupHandle_t s_wifi_event_group;
// static char wifi_ip[72];

// #define WIFI_CONNECTED_BIT BIT0
// #define WIFI_FAIL_BIT      BIT1

extern void network_coprocessor_init();
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
    STATUS retStatus = STATUS_SUCCESS;
    signaling_msg_t signalingMessage = {0};
    esp_err_t ret = deserialize_signaling_message(data, len, &signalingMessage);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to deserialize signaling message from bridge");
        return;
    }

    ESP_LOGI(TAG, "Received message from bridge of type: %d", signalingMessage.messageType);

    // Get the sample configuration
    PSampleConfiguration pSampleConfiguration = NULL;
    CHK_STATUS(webrtcAppGetSampleConfiguration(&pSampleConfiguration));
    CHK(pSampleConfiguration != NULL, STATUS_NULL_ARG);

    // Validate that we have a valid signaling client
    if (pSampleConfiguration == NULL || pSampleConfiguration->signalingClientHandle == INVALID_SIGNALING_CLIENT_HANDLE_VALUE) {
        ESP_LOGE(TAG, "Signaling client not initialized");
        goto CleanUp;
    }

    // Prepare the message for KVS
    SignalingMessage signalingAwsMessage;
    signalingAwsMessage.version = SIGNALING_MESSAGE_CURRENT_VERSION;

    // Map the message types
    switch (signalingMessage.messageType) {
        case SIGNALING_MSG_TYPE_ANSWER:
            signalingAwsMessage.messageType = SIGNALING_MESSAGE_TYPE_ANSWER;
            break;
        case SIGNALING_MSG_TYPE_ICE_CANDIDATE:
            signalingAwsMessage.messageType = SIGNALING_MESSAGE_TYPE_ICE_CANDIDATE;
            break;
        default:
            ESP_LOGW(TAG, "Unknown message type from bridge: %d", signalingMessage.messageType);
            goto CleanUp;
    }

    STRNCPY(signalingAwsMessage.peerClientId, signalingMessage.peerClientId, MAX_SIGNALING_CLIENT_ID_LEN);
    STRNCPY(signalingAwsMessage.correlationId, signalingMessage.correlationId, MAX_CORRELATION_ID_LEN);
    // memcpy(signalingAwsMessage.payload, signalingMessage.payload, signalingMessage.payloadLen);
#ifdef DYNAMIC_SIGNALING_PAYLOAD
    signalingAwsMessage.payload = signalingMessage.payload;
#else
    MEMCPY(signalingAwsMessage.payload, signalingMessage.payload, signalingMessage.payloadLen);
#endif
    signalingAwsMessage.payloadLen = signalingMessage.payloadLen;

    // Send the message to KVS via the signaling client
    retStatus = signalingClientSendMessageSync(pSampleConfiguration->signalingClientHandle, &signalingAwsMessage);
    if (retStatus != STATUS_SUCCESS) {
        ESP_LOGE(TAG, "signalingClientSendMessageSync failed: 0x%08" PRIx32, retStatus);
    } else {
        ESP_LOGI(TAG, "Successfully sent message to KVS: %s message",
            signalingMessage.messageType == SIGNALING_MSG_TYPE_ANSWER ? "ANSWER" :
            signalingMessage.messageType == SIGNALING_MSG_TYPE_ICE_CANDIDATE ? "ICE_CANDIDATE" : "OTHER");
    }

CleanUp:
    // Free the payload if it was allocated
    // if (signalingMessage.payload != NULL) {
    //     SAFE_MEMFREE(signalingMessage.payload);
    // }
}

// static void event_handler(void* arg, esp_event_base_t event_base,
//                          int32_t event_id, void* event_data)
// {
//     if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
//         esp_wifi_connect();
//     } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
//         esp_wifi_connect();
//     } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
//         ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
//         memset(wifi_ip, 0, sizeof(wifi_ip)/sizeof(wifi_ip[0]));
//         ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
//         memcpy(wifi_ip, &event->ip_info.ip, 4);
//         xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
//     }
// }

// char* esp_get_ip(void)
// {
//     return wifi_ip;
// }

// static void wifi_init_sta(void)
// {
//     s_wifi_event_group = xEventGroupCreate();

//     ESP_ERROR_CHECK(esp_netif_init());
//     ESP_ERROR_CHECK(esp_event_loop_create_default());
//     esp_netif_create_default_wifi_sta();

//     wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
//     ESP_ERROR_CHECK(esp_wifi_init(&cfg));

//     ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
//     ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

//     wifi_config_t wifi_config = {
//         .sta = {
//             .ssid = "ESP_India2.4",
//             .password = "Esp@3101",
//         },
//     };

//     ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
//     ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
//     ESP_ERROR_CHECK(esp_wifi_start());

//     ESP_LOGI(TAG, "Waiting for WiFi connection");
//     EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
//             WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
//             pdFALSE,
//             pdFALSE,
//             pdMS_TO_TICKS(20000));

//     if (bits & WIFI_CONNECTED_BIT) {
//         ESP_LOGI(TAG, "Connected to WiFi");
//     } else {
//         ESP_LOGE(TAG, "Failed to connect to WiFi");
//     }
// }

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

    // Register the bridge message handler
    webrtc_bridge_register_handler(handle_bridged_message);

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
        goto CleanUp;
    }

    ESP_LOGI(TAG, "Running WebRTC application");

    // Run WebRTC application
    status = webrtcAppRun();
    if (status != STATUS_SUCCESS) {
        ESP_LOGE(TAG, "WebRTC application failed: 0x%08" PRIx32, status);
        goto CleanUp;
    }

    ESP_LOGI(TAG, "WebRTC session terminated");

CleanUp:

    // Terminate WebRTC application
    webrtcAppTerminate();
    ESP_LOGI(TAG, "Cleanup done");
}
