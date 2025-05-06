#include <string.h>
#include <inttypes.h>
#include <stdint.h>
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

#include "app_media.h"
#include "signaling_serializer.h"
#include "webrtc_bridge.h"
#include "esp_work_queue.h"
#include "app_webrtc.h"
#include "signalling_remote.h"

// External function declarations
extern STATUS signalingMessageReceived(UINT64 customData, PReceivedSignalingMessage pReceivedSignalingMessage);

static const char *TAG = "streaming_only";

// WiFi event group
static EventGroupHandle_t s_wifi_event_group;
static char wifi_ip[72];

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

/**
 * @brief Handle bridged signaling received  via webrtc_bridge
 *
 * Signaling messages received via webrtc_bridge need to be converted to
 * ReceivedSignalingMessage and passed to signalingMessageReceived function
 *
 * @note The messages to be sent via bridge (to signaling_only component) are serialized and
 * sent by app_webrtc
 *
 * @param data The data to handle
 * @param len The length of the data
 */
static void handle_bridged_message(const void* data, int len)
{
    STATUS retStatus = STATUS_SUCCESS;
    signaling_msg_t signalingMsg = {0};
    ReceivedSignalingMessage receivedSignalingMessage = {0};
    // WebRTC configuration and session
    PSampleConfiguration pSampleConfiguration = NULL;
    CHK_STATUS(webrtcAppGetSampleConfiguration(&pSampleConfiguration));
    CHK(pSampleConfiguration != NULL, STATUS_NULL_ARG);

    // Deserialize the incoming message
    esp_err_t ret = deserialize_signaling_message(data, len, &signalingMsg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to deserialize signaling message");
        return;
    }

    ESP_LOGI(TAG, "Received signaling message type: %d", signalingMsg.messageType);

    // Convert from SignalingMsg to ReceivedSignalingMessage
    receivedSignalingMessage.signalingMessage.version = SIGNALING_MESSAGE_CURRENT_VERSION;

    // Convert message type
    switch (signalingMsg.messageType) {
        case SIGNALING_MSG_TYPE_OFFER:
            receivedSignalingMessage.signalingMessage.messageType = SIGNALING_MESSAGE_TYPE_OFFER;
            break;
        case SIGNALING_MSG_TYPE_ICE_CANDIDATE:
            receivedSignalingMessage.signalingMessage.messageType = SIGNALING_MESSAGE_TYPE_ICE_CANDIDATE;
            break;
        case SIGNALING_MSG_TYPE_ANSWER:
            receivedSignalingMessage.signalingMessage.messageType = SIGNALING_MESSAGE_TYPE_ANSWER;
            break;
        default:
            ESP_LOGW(TAG, "Unknown message type: %d", signalingMsg.messageType);
            goto CleanUp;
    }

    // Copy the rest of the fields
    STRNCPY(receivedSignalingMessage.signalingMessage.peerClientId, signalingMsg.peerClientId, MAX_SIGNALING_CLIENT_ID_LEN);
    STRNCPY(receivedSignalingMessage.signalingMessage.correlationId, signalingMsg.correlationId, MAX_CORRELATION_ID_LEN);

    if (signalingMsg.payload != NULL && signalingMsg.payloadLen > 0) {
#if DYNAMIC_SIGNALING_PAYLOAD
        receivedSignalingMessage.signalingMessage.payload = signalingMsg.payload;
#else
        MEMCPY(receivedSignalingMessage.signalingMessage.payload, signalingMsg.payload, signalingMsg.payloadLen);
        receivedSignalingMessage.signalingMessage.payload[signalingMsg.payloadLen] = '\0';
        SAFE_MEMFREE(signalingMsg.payload);
#endif
        receivedSignalingMessage.signalingMessage.payloadLen = signalingMsg.payloadLen;
    } else {
        // Handle empty payload
#if DYNAMIC_SIGNALING_PAYLOAD
        receivedSignalingMessage.signalingMessage.payload = NULL;
#else
        receivedSignalingMessage.signalingMessage.payload[0] = '\0';
#endif
        receivedSignalingMessage.signalingMessage.payloadLen = 0;
    }

    // Call the signaling message handler from sample_config.c
    // This will handle all aspects of signaling, including:
    // - Creating the streaming session when needed
    // - Managing session lifecycle
    // - Processing offers, answers, and ICE candidates
    retStatus = signalingMessageReceived((UINT64)pSampleConfiguration, &receivedSignalingMessage);
    if (retStatus != STATUS_SUCCESS) {
        // Check if it's the ESP-IDF specific error (non-fatal)
        if (retStatus == 0x40100002) {
            ESP_LOGW(TAG, "signalingMessageReceived returned ESP-IDF event handler error (0x40100002) - continuing anyway");
        } else {
            ESP_LOGE(TAG, "signalingMessageReceived failed with status: 0x%08" PRIx32, retStatus);
        }
    }

CleanUp:
    // Nothing!!
}

// WiFi event handler
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

    //esp_netif_action_start(sta_netif, NULL, 0, NULL);
    //esp_netif_action_connected(sta_netif, 0, 0, 0);

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
        case APP_WEBRTC_EVENT_PEER_CONNECTION_FAILED:
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

    ESP_LOGI(TAG, "ESP32 WebRTC Streaming Example");

    esp_cli_start();

    // Initialize WiFi
    wifi_init_sta();

    app_storage_init();

    // Initialize signaling serializer
    signaling_serializer_init();

    // Initialize work queue
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

    // Configure WebRTC
    WebRtcAppConfig webrtcConfig = {0};
    webrtcConfig.roleType = SIGNALING_CHANNEL_ROLE_TYPE_VIEWER;
    webrtcConfig.mode = APP_WEBRTC_STREAMING_ONLY_MODE;
    webrtcConfig.pChannelName = NULL; // NULL for streaming-only mode

    // Set common options
    webrtcConfig.trickleIce = TRUE;
    webrtcConfig.useTurn = TRUE;
    webrtcConfig.logLevel = LOG_LEVEL_DEBUG;

    // Configure media
    webrtcConfig.audioCodec = RTC_CODEC_OPUS;
    webrtcConfig.videoCodec = RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE;
    webrtcConfig.mediaType = SAMPLE_STREAMING_AUDIO_VIDEO;

    // Set media source callbacks
#ifdef USE_FILE_SOURCE
    webrtcConfig.audioSourceCallback = sendFileAudioPackets;
    webrtcConfig.videoSourceCallback = sendFileVideoPackets;
#else
    webrtcConfig.audioSourceCallback = sendMediaStreamAudioPackets;
    webrtcConfig.videoSourceCallback = sendMediaStreamVideoPackets;
#endif
    webrtcConfig.receiveAudioVideoCallback = sampleReceiveAudioVideoFrame;

    ESP_LOGI(TAG, "Initializing WebRTC application");

    // Initialize WebRTC application
    status = webrtcAppInit(&webrtcConfig);
    if (status != STATUS_SUCCESS) {
        ESP_LOGE(TAG, "Failed to initialize WebRTC application: 0x%08" PRIx32, status);
        goto CleanUp;
    }

    // Register the bridge message handler
    webrtc_bridge_register_handler(handle_bridged_message);

    // Start webrtc bridge
    webrtc_bridge_start();

    ESP_LOGI(TAG, "Streaming example initialized, waiting for signaling messages");

    // Run WebRTC application
    status = webrtcAppRun();
    if (status != STATUS_SUCCESS) {
        ESP_LOGE(TAG, "WebRTC application failed: 0x%08" PRIx32, status);
        goto CleanUp;
    }

CleanUp:
    // Do not terminate the WebRTC application in streaming-only mode
    // Only streaming sessions are created and destroyed internally
    // Terminate WebRTC application
    // webrtcAppTerminate();
}

PVOID sampleReceiveAudioVideoFrame(PVOID args)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleStreamingSession pSampleStreamingSession = (PSampleStreamingSession) args;
    CHK_ERR(pSampleStreamingSession != NULL, STATUS_NULL_ARG, "[KVS Master] Streaming session is NULL");
    CHK_STATUS(transceiverOnFrame(pSampleStreamingSession->pVideoRtcRtpTransceiver, (UINT64) pSampleStreamingSession, appMediaVideoFrameHandler));
    CHK_STATUS(transceiverOnFrame(pSampleStreamingSession->pAudioRtcRtpTransceiver, (UINT64) pSampleStreamingSession, appMediaAudioFrameHandler));

CleanUp:

    return (PVOID) (ULONG_PTR) retStatus;
}
