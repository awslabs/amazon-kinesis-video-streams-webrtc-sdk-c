/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "kvs_signaling.h"
#include "webrtc_mem_utils.h"
#include "app_webrtc.h"
#include "fileio.h"
#ifdef CONFIG_IOT_CORE_ENABLE_CREDENTIALS
#include "iot_credential_provider.h"
#endif

#ifdef CONFIG_USE_ESP_WEBSOCKET_CLIENT
#include "SignalingESP.h"
#endif

#include "esp_work_queue.h"
#include <com/amazonaws/kinesis/video/webrtcclient/Include.h>

// Include KVS signaling headers for refresh_ice_configuration and PSignalingClient
#include "Signaling.h"

static const char *TAG = "kvs_signaling";

// Work queue task for refreshing ICE configuration in background
static void kvs_refresh_ice_task(void *arg)
{
    PSignalingClient pSignalingClient = (PSignalingClient)arg;
    if (pSignalingClient != NULL) {
        ESP_LOGI(TAG, "Background ICE refresh task started");
        STATUS status = refresh_ice_configuration(pSignalingClient);
        if (STATUS_FAILED(status)) {
            ESP_LOGE(TAG, "Background ICE refresh failed: 0x%08x", status);
        } else {
            ESP_LOGI(TAG, "Background ICE refresh completed successfully");
        }
    }
}

// Forward declarations for internal KVS functions (not exposed in public header)
STATUS createKvsSignalingClient(kvs_signaling_config_t *pConfig, PVOID *ppSignalingClient);
STATUS connectKvsSignalingClient(PVOID pSignalingClient);
STATUS disconnectKvsSignalingClient(PVOID pSignalingClient);
STATUS sendKvsSignalingMessage(PVOID pSignalingClient, esp_webrtc_signaling_message_t *pMessage);
STATUS freeKvsSignalingClient(PVOID pSignalingClient);
STATUS setKvsSignalingCallbacks(PVOID pSignalingClient,
                              PVOID customData,
                              STATUS (*on_msg_received)(UINT64, esp_webrtc_signaling_message_t*),
                              STATUS (*on_state_changed)(UINT64, SIGNALING_CLIENT_STATE),
                              STATUS (*on_error)(UINT64, STATUS, PCHAR, UINT32));
STATUS setKvsSignalingRoleType(PVOID pSignalingClient, SIGNALING_CHANNEL_ROLE_TYPE role_type);
STATUS getKvsSignalingIceServers(PVOID pSignalingClient, PUINT32 pIceConfigCount, RtcIceServer iceServers[]);
STATUS kvsSignalingQueryServerGetByIdx(PVOID pSignalingClient, int index, bool useTurn, uint8_t **data, int *len, bool *have_more);
static WEBRTC_STATUS kvsIsIceRefreshNeededWrapper(void *pSignalingClient, bool *refreshNeeded);

/**
 * @brief Adapter data structure for callback conversion
 */
 typedef struct {
    uint64_t originalCustomData;
    WEBRTC_STATUS (*originalOnStateChanged)(uint64_t, webrtc_signaling_client_state_t);
    WEBRTC_STATUS (*originalOnError)(uint64_t, WEBRTC_STATUS, char*, uint32_t);
} CallbackAdapterData;

/**
 * @brief Structure for KVS signaling client data
 */
typedef struct {
    SIGNALING_CLIENT_HANDLE signalingClientHandle;
    ChannelInfo channelInfo;
    SignalingClientInfo clientInfo;
    SignalingClientCallbacks signalingClientCallbacks;
    PAwsCredentialProvider pCredentialProvider;
    BOOL initialized;
    BOOL connected;
    MUTEX signalingSendMessageLock;

    // Callback data
    PVOID customData;
    STATUS (*on_msg_received)(UINT64, esp_webrtc_signaling_message_t*);
    STATUS (*on_state_changed)(UINT64, SIGNALING_CLIENT_STATE);
    STATUS (*on_error)(UINT64, STATUS, PCHAR, UINT32);

    // Adapter data for callback conversion (if needed)
    CallbackAdapterData *pCallbackAdapterData;

    // Configuration
    kvs_signaling_config_t config;

    // Metrics
    SignalingClientMetrics metrics;
} KvsSignalingClientData;

/**
 * @brief Convert SIGNALING_CLIENT_STATE to webrtc_signaling_client_state_t
 *
 * This conversion matches the logic from app_webrtc.c to properly handle
 * all intermediate KVS signaling states.
 */
static webrtc_signaling_client_state_t convertKvsToWebrtcState(SIGNALING_CLIENT_STATE kvsState)
{
    switch (kvsState) {
        case SIGNALING_CLIENT_STATE_NEW:
            return WEBRTC_SIGNALING_CLIENT_STATE_NEW;
        case SIGNALING_CLIENT_STATE_CONNECTING:
        case SIGNALING_CLIENT_STATE_GET_CREDENTIALS:
        case SIGNALING_CLIENT_STATE_DESCRIBE:
        case SIGNALING_CLIENT_STATE_CREATE:
        case SIGNALING_CLIENT_STATE_GET_ENDPOINT:
        case SIGNALING_CLIENT_STATE_GET_ICE_CONFIG:
        case SIGNALING_CLIENT_STATE_READY:
            return WEBRTC_SIGNALING_CLIENT_STATE_CONNECTING;
        case SIGNALING_CLIENT_STATE_CONNECTED:
        case SIGNALING_CLIENT_STATE_JOIN_SESSION_CONNECTED:
            return WEBRTC_SIGNALING_CLIENT_STATE_CONNECTED;
        case SIGNALING_CLIENT_STATE_DISCONNECTED:
        case SIGNALING_CLIENT_STATE_DELETE:
        case SIGNALING_CLIENT_STATE_DELETED:
            return WEBRTC_SIGNALING_CLIENT_STATE_DISCONNECTED;
        default:
            return WEBRTC_SIGNALING_CLIENT_STATE_FAILED;
    }
}

/**
 * @brief Convert STATUS to WEBRTC_STATUS
 */
static WEBRTC_STATUS convertStatusToWebrtcStatus(STATUS retStatus)
{
    return (retStatus == STATUS_SUCCESS) ? WEBRTC_STATUS_SUCCESS : WEBRTC_STATUS_INTERNAL_ERROR;
}

/**
 * @brief Adapter function for state change callback
 */
static STATUS adapterStateChangedCallback(UINT64 customData, SIGNALING_CLIENT_STATE state)
{
    CallbackAdapterData *pAdapterData = (CallbackAdapterData *) customData;

    if (pAdapterData == NULL || pAdapterData->originalOnStateChanged == NULL) {
        return STATUS_SUCCESS;
    }

    webrtc_signaling_client_state_t webrtcState = convertKvsToWebrtcState(state);
    WEBRTC_STATUS result = pAdapterData->originalOnStateChanged(pAdapterData->originalCustomData, webrtcState);

    return (result == WEBRTC_STATUS_SUCCESS) ? STATUS_SUCCESS : STATUS_INTERNAL_ERROR;
}

/**
 * @brief Adapter function for error callback
 */
static STATUS adapterErrorCallback(UINT64 customData, STATUS errorStatus, PCHAR errorMessage, UINT32 subErrorCode)
{
    CallbackAdapterData *pAdapterData = (CallbackAdapterData *) customData;

    if (pAdapterData == NULL || pAdapterData->originalOnError == NULL) {
        return STATUS_SUCCESS;
    }

    // Preserve the original status code instead of converting to generic WEBRTC_STATUS_INTERNAL_ERROR
    // This ensures signaling reconnection status codes are passed through correctly
    WEBRTC_STATUS result = pAdapterData->originalOnError(pAdapterData->originalCustomData, (WEBRTC_STATUS)errorStatus, errorMessage, subErrorCode);

    return (result == WEBRTC_STATUS_SUCCESS) ? STATUS_SUCCESS : STATUS_INTERNAL_ERROR;
}

// Forward declarations
STATUS createCredentialProvider(KvsSignalingClientData *pClientData);
STATUS extractRegionFromCredentialToken(const char* token, char* region, size_t region_size);

/**
 * @brief KVS signaling state change callback
 */
static STATUS kvsStateChangedCallback(UINT64 customData, SIGNALING_CLIENT_STATE state)
{
    KvsSignalingClientData *pClientData = (KvsSignalingClientData *) customData;

    if (pClientData == NULL) {
        return STATUS_NULL_ARG;
    }

    ESP_LOGI(TAG, "KVS signaling state changed to %d", state);

    // Update internal state
    if (state == SIGNALING_CLIENT_STATE_CONNECTED) {
        pClientData->connected = TRUE;
    } else if (state == SIGNALING_CLIENT_STATE_DISCONNECTED) {
        pClientData->connected = FALSE;
    }

    // Call user callback if set
    if (pClientData->on_state_changed != NULL) {
        ESP_LOGI(TAG, "Calling user on_state_changed callback");
        return pClientData->on_state_changed((UINT64)pClientData->customData, state);
    } else {
        ESP_LOGW(TAG, "No on_state_changed callback set - events will not be propagated!");
    }

    return STATUS_SUCCESS;
}

/**
 * @brief KVS signaling error callback
 */
static STATUS kvsErrorCallback(UINT64 customData, STATUS status, PCHAR errorMsg, UINT32 errorMsgLen)
{
    KvsSignalingClientData *pClientData = (KvsSignalingClientData *) customData;

    if (pClientData == NULL) {
        return STATUS_NULL_ARG;
    }

    ESP_LOGW(TAG, "KVS signaling error: 0x%08x - %.*s", status, errorMsgLen, errorMsg);

    // Call user callback if set
    if (pClientData->on_error != NULL) {
        return pClientData->on_error((UINT64)pClientData->customData, status, errorMsg, errorMsgLen);
    }

    return STATUS_SUCCESS;
}

/**
 * @brief Convert KVS SDK message format to standardized WebRTC signaling message format
 */
static STATUS convertKvsToWebRtcMessage(PReceivedSignalingMessage pKvsMessage, esp_webrtc_signaling_message_t* pWebRtcMessage)
{
    if (pKvsMessage == NULL || pWebRtcMessage == NULL) {
        return STATUS_NULL_ARG;
    }

    // Clear the output structure
    memset(pWebRtcMessage, 0, sizeof(esp_webrtc_signaling_message_t));

    // Convert message type
    switch (pKvsMessage->signalingMessage.messageType) {
        case SIGNALING_MESSAGE_TYPE_OFFER:
            pWebRtcMessage->message_type = ESP_SIGNALING_MESSAGE_TYPE_OFFER;
            break;
        case SIGNALING_MESSAGE_TYPE_ANSWER:
            pWebRtcMessage->message_type = ESP_SIGNALING_MESSAGE_TYPE_ANSWER;
            break;
        case SIGNALING_MESSAGE_TYPE_ICE_CANDIDATE:
            pWebRtcMessage->message_type = ESP_SIGNALING_MESSAGE_TYPE_ICE_CANDIDATE;
            break;
        default:
            ESP_LOGW(TAG, "Unknown KVS message type: %d", pKvsMessage->signalingMessage.messageType);
            return STATUS_INVALID_ARG;
    }

    // Copy basic fields
    pWebRtcMessage->version = pKvsMessage->signalingMessage.version;

    // Copy correlation ID and peer client ID (with bounds checking)
    strncpy(pWebRtcMessage->correlation_id, pKvsMessage->signalingMessage.correlationId, MAX_CORRELATION_ID_LEN);
    pWebRtcMessage->correlation_id[MAX_CORRELATION_ID_LEN] = '\0';

    strncpy(pWebRtcMessage->peer_client_id, pKvsMessage->signalingMessage.peerClientId, MAX_SIGNALING_CLIENT_ID_LEN);
    pWebRtcMessage->peer_client_id[MAX_SIGNALING_CLIENT_ID_LEN] = '\0';

    // Copy payload (note: this is a shallow copy - the payload memory is shared)
    pWebRtcMessage->payload = pKvsMessage->signalingMessage.payload;
    pWebRtcMessage->payload_len = pKvsMessage->signalingMessage.payloadLen;

    return STATUS_SUCCESS;
}

/**
 * @brief Internal callback that converts KVS message format to standardized format
 */
static STATUS kvsMessageReceivedCallback(UINT64 customData, PReceivedSignalingMessage pReceivedSignalingMessage)
{
    STATUS retStatus = STATUS_SUCCESS;
    KvsSignalingClientData *pClientData = (KvsSignalingClientData *)(UINT64)customData;
    esp_webrtc_signaling_message_t webRtcMessage = {0};

    if (pClientData == NULL || pReceivedSignalingMessage == NULL) {
        retStatus = STATUS_NULL_ARG;
        CHK(FALSE, retStatus);
    }

    // Call user callback if set
    if (pClientData->on_msg_received != NULL) {
        // Convert KVS message format to standardized WebRTC message format
        retStatus = convertKvsToWebRtcMessage(pReceivedSignalingMessage, &webRtcMessage);
        CHK_STATUS(retStatus);

        CallbackAdapterData *pAdapterData = (CallbackAdapterData *) pClientData->customData;
        // Call the user callback with the standardized message format
        retStatus = pClientData->on_msg_received((UINT64) pAdapterData->originalCustomData, &webRtcMessage);
    }

CleanUp:
    return retStatus;
}

/**
 * @brief Initialize KVS signaling client
 */
STATUS createKvsSignalingClient(kvs_signaling_config_t *pConfig, PVOID *ppSignalingClient)
{
    STATUS retStatus = STATUS_SUCCESS;
    KvsSignalingClientData *pClientData = NULL;

    CHK(pConfig != NULL && ppSignalingClient != NULL, STATUS_NULL_ARG);

    // Allocate client data
    pClientData = (KvsSignalingClientData *)MEMCALLOC(1, SIZEOF(KvsSignalingClientData));
    CHK(pClientData != NULL, STATUS_NOT_ENOUGH_MEMORY);

    // Copy configuration
    MEMCPY(&pClientData->config, pConfig, SIZEOF(kvs_signaling_config_t));

    // Initialize mutex
    pClientData->signalingSendMessageLock = MUTEX_CREATE(FALSE);
    CHK(IS_VALID_MUTEX_VALUE(pClientData->signalingSendMessageLock), STATUS_INVALID_OPERATION);

    // Set up channel info - role type will be set by WebRTC app
    pClientData->channelInfo.version = CHANNEL_INFO_CURRENT_VERSION;
    pClientData->channelInfo.pChannelName = pConfig->pChannelName;
    pClientData->channelInfo.pKmsKeyId = NULL;
    pClientData->channelInfo.tagCount = 0;
    pClientData->channelInfo.pTags = NULL;
    pClientData->channelInfo.channelType = SIGNALING_CHANNEL_TYPE_SINGLE_MASTER;
    pClientData->channelInfo.channelRoleType = SIGNALING_CHANNEL_ROLE_TYPE_MASTER; // Default, will be updated
    pClientData->channelInfo.cachingPolicy = SIGNALING_API_CALL_CACHE_TYPE_NONE;
    pClientData->channelInfo.cachingPeriod = SIGNALING_API_CALL_CACHE_TTL_SENTINEL_VALUE;
    pClientData->channelInfo.asyncIceServerConfig = TRUE;
    pClientData->channelInfo.retry = TRUE;
    pClientData->channelInfo.reconnect = TRUE;
    pClientData->channelInfo.pCertPath = pConfig->caCertPath;
    pClientData->channelInfo.messageTtl = 0; // Default is 60 seconds
    pClientData->channelInfo.pRegion = pConfig->awsRegion;

    // Set up client info with default log level
    pClientData->clientInfo.version = SIGNALING_CLIENT_INFO_CURRENT_VERSION;
    pClientData->clientInfo.loggingLevel = LOG_LEVEL_DEBUG; // Default log level
    pClientData->clientInfo.cacheFilePath = NULL; // Use the default path
    pClientData->clientInfo.signalingClientCreationMaxRetryAttempts = CREATE_SIGNALING_CLIENT_RETRY_ATTEMPTS_SENTINEL_VALUE;

    // Set default client ID (will be updated based on role type later)
    STRCPY(pClientData->clientInfo.clientId, DEFAULT_MASTER_CLIENT_ID);

    // Set up callbacks
    pClientData->signalingClientCallbacks.version = SIGNALING_CLIENT_CALLBACKS_CURRENT_VERSION;
    pClientData->signalingClientCallbacks.customData = (UINT64) pClientData;
    pClientData->signalingClientCallbacks.errorReportFn = kvsErrorCallback;
    pClientData->signalingClientCallbacks.stateChangeFn = kvsStateChangedCallback;
    pClientData->signalingClientCallbacks.messageReceivedFn = kvsMessageReceivedCallback;

    // Initialize metrics
    pClientData->metrics.version = SIGNALING_CLIENT_METRICS_CURRENT_VERSION;

    // Create credential provider
    CHK_STATUS(createCredentialProvider(pClientData));

    *ppSignalingClient = pClientData;

CleanUp:
    if (STATUS_FAILED(retStatus) && pClientData != NULL) {
        freeKvsSignalingClient(pClientData);
        pClientData = NULL;
    }

    return retStatus;
}

/**
 * @brief Create credential provider for KVS signaling
 */
STATUS createCredentialProvider(KvsSignalingClientData *pClientData)
{
    STATUS retStatus = STATUS_SUCCESS;
    PCHAR pIotCoreCredentialEndPoint = NULL;
    PCHAR pIotCoreCert = NULL;
    PCHAR pIotCorePrivateKey = NULL;
    PCHAR pIotCoreRoleAlias = NULL;
    PCHAR pIotCoreThingName = NULL;
    PCHAR pAccessKey = NULL;
    PCHAR pSecretKey = NULL;
    PCHAR pSessionToken = NULL;

    CHK(pClientData != NULL, STATUS_NULL_ARG);

    ESP_LOGI(TAG, "Creating credential provider with region: %s", pClientData->config.awsRegion);
    ESP_LOGI(TAG, "Credential type: %s", pClientData->config.useIotCredentials ? "IoT Core" : "Static");

    // Create credential provider based on type
    if (pClientData->config.useIotCredentials) {
        // Use IoT Core credentials from the options
        pIotCoreCredentialEndPoint = pClientData->config.iotCoreCredentialEndpoint;
        pIotCoreCert = pClientData->config.iotCoreCert;
        pIotCorePrivateKey = pClientData->config.iotCorePrivateKey;
        pIotCoreRoleAlias = pClientData->config.iotCoreRoleAlias;
        pIotCoreThingName = pClientData->config.iotCoreThingName;

        // Validate required fields
        CHK_ERR(pIotCoreCredentialEndPoint != NULL && pIotCoreCredentialEndPoint[0] != '\0', STATUS_INVALID_OPERATION,
                "IoT Core credential endpoint must be set");
        CHK_ERR(pIotCoreCert != NULL && pIotCoreCert[0] != '\0', STATUS_INVALID_OPERATION,
                "IoT Core certificate must be set");
        CHK_ERR(pIotCorePrivateKey != NULL && pIotCorePrivateKey[0] != '\0', STATUS_INVALID_OPERATION,
                "IoT Core private key must be set");
        CHK_ERR(pIotCoreRoleAlias != NULL && pIotCoreRoleAlias[0] != '\0', STATUS_INVALID_OPERATION,
                "IoT Core role alias must be set");
        CHK_ERR(pIotCoreThingName != NULL && pIotCoreThingName[0] != '\0', STATUS_INVALID_OPERATION,
                "IoT Core thing name must be set");

        ESP_LOGI(TAG, "Creating IoT credential provider with endpoint: %s", pIotCoreCredentialEndPoint);
        ESP_LOGI(TAG, "IoT Core thing name: %s, role alias: %s", pIotCoreThingName, pIotCoreRoleAlias);
        ESP_LOGI(TAG, "Certificate path: %s", pIotCoreCert);
        ESP_LOGI(TAG, "Private key path: %s", pIotCorePrivateKey);
        ESP_LOGI(TAG, "CA cert path: %s", pClientData->channelInfo.pCertPath);

        // Try to read the certificate file to verify it exists and is accessible
        BOOL cert_exists = FALSE;
        if (fileExists(pIotCoreCert, &cert_exists) != STATUS_SUCCESS || !cert_exists) {
            ESP_LOGE(TAG, "Failed to open certificate file: %s", pIotCoreCert);
            CHK(FALSE, STATUS_INVALID_OPERATION);
        } else {
            ESP_LOGI(TAG, "Successfully verified certificate file exists and is readable");
        }

        // Try to read the private key file to verify it exists and is accessible
        BOOL key_exists = FALSE;
        if (fileExists(pIotCorePrivateKey, &key_exists) != STATUS_SUCCESS || !key_exists) {
            ESP_LOGE(TAG, "Failed to open private key file: %s", pIotCorePrivateKey);
            CHK(FALSE, STATUS_INVALID_OPERATION);
        } else {
            ESP_LOGI(TAG, "Successfully verified private key file exists and is readable");
        }

        // Try to read the CA cert file to verify it exists and is accessible
        if (pClientData->channelInfo.pCertPath != NULL) {
            BOOL ca_exists = FALSE;
            if (fileExists(pClientData->channelInfo.pCertPath, &ca_exists) != STATUS_SUCCESS || !ca_exists) {
                ESP_LOGE(TAG, "Failed to open CA cert file: %s", pClientData->channelInfo.pCertPath);
                CHK(FALSE, STATUS_INVALID_OPERATION);
            } else {
                ESP_LOGI(TAG, "Successfully verified CA cert file exists and is readable");
            }
        }

#ifdef CONFIG_IOT_CORE_ENABLE_CREDENTIALS
        retStatus = createIotCredentialProvider(
            pIotCoreCredentialEndPoint,
            pClientData->config.awsRegion,
            pIotCoreCert,
            pIotCorePrivateKey,
            pClientData->channelInfo.pCertPath,
            pIotCoreRoleAlias,
            pIotCoreThingName,
            &pClientData->pCredentialProvider);
#else
        // IoT Core credentials not supported in this build
        ESP_LOGE(TAG, "IoT Core credentials not supported in this build");
        CHK(FALSE, STATUS_NOT_IMPLEMENTED);
#endif

#ifdef CONFIG_IOT_CORE_ENABLE_CREDENTIALS
        if (STATUS_FAILED(retStatus)) {
            ESP_LOGE(TAG, "Failed to create credential provider: 0x%08x", retStatus);
            if ((retStatus >> 24) == 0x52) {
                ESP_LOGE(TAG, "This appears to be a networking error. Check network connectivity.");
            } else if ((retStatus >> 24) == 0x50) {
                ESP_LOGE(TAG, "This appears to be a platform error. Check certificate and key files.");
            } else if ((retStatus >> 24) == 0x58) {
                ESP_LOGE(TAG, "This appears to be a client error. Check IoT Core configuration.");
            }
            CHK(FALSE, retStatus);
        }

        ESP_LOGI(TAG, "IoT credential provider created successfully");
#endif
    } else {
        // Use direct AWS credentials from the options
        pAccessKey = pClientData->config.awsAccessKey;
        pSecretKey = pClientData->config.awsSecretKey;
        pSessionToken = pClientData->config.awsSessionToken;

        // Validate required fields
        CHK_ERR(pAccessKey != NULL && pAccessKey[0] != '\0', STATUS_INVALID_OPERATION,
                "AWS access key must be set");
        CHK_ERR(pSecretKey != NULL && pSecretKey[0] != '\0', STATUS_INVALID_OPERATION,
                "AWS secret key must be set");

        ESP_LOGI(TAG, "Creating static credential provider with access key ID: %.*s...", 4, pAccessKey);
        retStatus = createStaticCredentialProvider(
            pAccessKey,
            0,
            pSecretKey,
            0,
            pSessionToken,
            0,
            MAX_UINT64,
            &pClientData->pCredentialProvider);

        if (STATUS_FAILED(retStatus)) {
            ESP_LOGE(TAG, "Failed to create static credential provider: 0x%08x", retStatus);
            CHK(FALSE, retStatus);
        }

        ESP_LOGI(TAG, "Static credential provider created successfully");
    }

CleanUp:
    return retStatus;
}

/**
 * @brief Connect KVS signaling client
 */
STATUS connectKvsSignalingClient(PVOID pSignalingClient)
{
    STATUS retStatus = STATUS_SUCCESS;
    KvsSignalingClientData *pClientData = (KvsSignalingClientData *)pSignalingClient;

    CHK(pClientData != NULL, STATUS_NULL_ARG);

    // Create KVS signaling client if not already created
    if (pClientData->signalingClientHandle == INVALID_SIGNALING_CLIENT_HANDLE_VALUE) {
        CHK_STATUS(createSignalingClientSync(
            &pClientData->clientInfo,
            &pClientData->channelInfo,
            &pClientData->signalingClientCallbacks,
            pClientData->pCredentialProvider,
            &pClientData->signalingClientHandle));
    }

    // Fetch the signaling client
    CHK_STATUS(signalingClientFetchSync(pClientData->signalingClientHandle));

    // Connect the signaling client
    CHK_STATUS(signalingClientConnectSync(pClientData->signalingClientHandle));

    // Get metrics
    CHK_STATUS(signalingClientGetMetrics(pClientData->signalingClientHandle, &pClientData->metrics));

    // Log metrics
    ESP_LOGI(TAG, "[Signaling Get token] %" PRIu64 " ms", pClientData->metrics.signalingClientStats.getTokenCallTime);
    ESP_LOGI(TAG, "[Signaling Describe] %" PRIu64 " ms", pClientData->metrics.signalingClientStats.describeCallTime);
    ESP_LOGI(TAG, "[Signaling Get endpoint] %" PRIu64 " ms", pClientData->metrics.signalingClientStats.getEndpointCallTime);
    ESP_LOGI(TAG, "[Signaling Get ICE config] %" PRIu64 " ms", pClientData->metrics.signalingClientStats.getIceConfigCallTime);
    ESP_LOGI(TAG, "[Signaling Connect] %" PRIu64 " ms", pClientData->metrics.signalingClientStats.connectCallTime);

    pClientData->initialized = TRUE;

CleanUp:
    return retStatus;
}

/**
 * @brief Disconnect KVS signaling client
 */
STATUS disconnectKvsSignalingClient(PVOID pSignalingClient)
{
    STATUS retStatus = STATUS_SUCCESS;
    KvsSignalingClientData *pClientData = (KvsSignalingClientData *)pSignalingClient;

    CHK(pClientData != NULL, STATUS_NULL_ARG);
    CHK(pClientData->signalingClientHandle != INVALID_SIGNALING_CLIENT_HANDLE_VALUE, STATUS_INVALID_OPERATION);

    // Disconnect the signaling client
    CHK_STATUS(signalingClientDisconnectSync(pClientData->signalingClientHandle));

CleanUp:
    return retStatus;
}

/**
 * @brief Send message through KVS signaling client
 */
STATUS sendKvsSignalingMessage(PVOID pSignalingClient, esp_webrtc_signaling_message_t *pMessage)
{
    STATUS retStatus = STATUS_SUCCESS;
    KvsSignalingClientData *pClientData = (KvsSignalingClientData *)pSignalingClient;
    SignalingMessage signalingMessage;
    BOOL locked = FALSE;

    CHK(pClientData != NULL && pMessage != NULL, STATUS_NULL_ARG);
    CHK(pClientData->signalingClientHandle != INVALID_SIGNALING_CLIENT_HANDLE_VALUE, STATUS_INVALID_OPERATION);

    // Convert the message to KVS format
    signalingMessage.version = pMessage->version;
    signalingMessage.messageType = (SIGNALING_MESSAGE_TYPE)pMessage->message_type;
    STRNCPY(signalingMessage.peerClientId, pMessage->peer_client_id, MAX_SIGNALING_CLIENT_ID_LEN);
    signalingMessage.peerClientId[MAX_SIGNALING_CLIENT_ID_LEN] = '\0';
    STRNCPY(signalingMessage.correlationId, pMessage->correlation_id, MAX_CORRELATION_ID_LEN);
    signalingMessage.correlationId[MAX_CORRELATION_ID_LEN] = '\0';
    signalingMessage.payload = pMessage->payload;
    signalingMessage.payloadLen = pMessage->payload_len;

    // Send the message with thread safety
    MUTEX_LOCK(pClientData->signalingSendMessageLock);
    locked = TRUE;
    CHK_STATUS(signalingClientSendMessageSync(pClientData->signalingClientHandle, &signalingMessage));
    MUTEX_UNLOCK(pClientData->signalingSendMessageLock);
    locked = FALSE;

    // Update metrics for answer messages
    if (pMessage->message_type == ESP_SIGNALING_MESSAGE_TYPE_ANSWER) {
        // Get updated metrics
        CHK_STATUS(signalingClientGetMetrics(pClientData->signalingClientHandle, &pClientData->metrics));
    }

CleanUp:
    if (locked) {
        MUTEX_UNLOCK(pClientData->signalingSendMessageLock);
    }

    return retStatus;
}

/**
 * @brief Set callbacks for KVS signaling client
 */
STATUS setKvsSignalingCallbacks(PVOID pSignalingClient,
                              PVOID customData,
                              STATUS (*on_msg_received)(UINT64, esp_webrtc_signaling_message_t*),
                              STATUS (*on_state_changed)(UINT64, SIGNALING_CLIENT_STATE),
                              STATUS (*on_error)(UINT64, STATUS, PCHAR, UINT32))
{
    STATUS retStatus = STATUS_SUCCESS;
    KvsSignalingClientData *pClientData = (KvsSignalingClientData *)pSignalingClient;

    CHK(pClientData != NULL, STATUS_NULL_ARG);

    // Store the callbacks
    pClientData->customData = customData;
    pClientData->on_msg_received = on_msg_received;
    pClientData->on_state_changed = on_state_changed;
    pClientData->on_error = on_error;

CleanUp:
    return retStatus;
}

/**
 * @brief Free KVS signaling client
 */
STATUS freeKvsSignalingClient(PVOID pSignalingClient)
{
    STATUS retStatus = STATUS_SUCCESS;
    KvsSignalingClientData *pClientData = (KvsSignalingClientData *)pSignalingClient;

    CHK(pClientData != NULL, STATUS_NULL_ARG);

    // Free the signaling client if initialized
    if (pClientData->signalingClientHandle != INVALID_SIGNALING_CLIENT_HANDLE_VALUE) {
        freeSignalingClient(&pClientData->signalingClientHandle);
    }

    // Free the credential provider
    if (pClientData->pCredentialProvider != NULL) {
        if (pClientData->config.useIotCredentials) {
            freeIotCredentialProvider(&pClientData->pCredentialProvider);
        } else {
            freeStaticCredentialProvider(&pClientData->pCredentialProvider);
        }
    }

    // Free the mutex
    if (IS_VALID_MUTEX_VALUE(pClientData->signalingSendMessageLock)) {
        MUTEX_FREE(pClientData->signalingSendMessageLock);
    }

    // Free the callback adapter data if it was allocated
    if (pClientData->pCallbackAdapterData != NULL) {
        MEMFREE(pClientData->pCallbackAdapterData);
    }

    // Free the client data
    MEMFREE(pClientData);

CleanUp:
    return retStatus;
}

/**
 * @brief Get ICE server configuration from KVS signaling client
 */
STATUS getKvsSignalingIceServers(PVOID pSignalingClient, PUINT32 pIceConfigCount, RtcIceServer iceServers[])
{
    STATUS retStatus = STATUS_SUCCESS;
    KvsSignalingClientData *pClientData = (KvsSignalingClientData *)pSignalingClient;
    UINT32 i, j, iceConfigCount, uriCount = 0;
    PIceConfigInfo pIceConfigInfo;
    PCHAR pKinesisVideoStunUrlPostFix;

    CHK(pClientData != NULL && pIceConfigCount != NULL && iceServers != NULL, STATUS_NULL_ARG);

    // Check if signaling client handle is valid - if not, fall back to default STUN servers
    if (pClientData->signalingClientHandle == INVALID_SIGNALING_CLIENT_HANDLE_VALUE) {
        ESP_LOGW(TAG, "Signaling client not yet connected, using fallback STUN servers");

        // Use fallback STUN server
        SNPRINTF(iceServers[0].urls, MAX_ICE_CONFIG_URI_LEN,
                 "stun:stun.l.google.com:19302");

        // Make sure credentials are empty for STUN
        iceServers[0].username[0] = '\0';
        iceServers[0].credential[0] = '\0';

        *pIceConfigCount = 1;
        ESP_LOGI(TAG, "Using fallback STUN server: %s", iceServers[0].urls);
        CHK(FALSE, STATUS_SUCCESS);
    }

    // Set the STUN server
    pKinesisVideoStunUrlPostFix = KINESIS_VIDEO_STUN_URL_POSTFIX;
    // If region is in CN, add CN region uri postfix
    if (STRSTR(pClientData->channelInfo.pRegion, "cn-")) {
        pKinesisVideoStunUrlPostFix = KINESIS_VIDEO_STUN_URL_POSTFIX_CN;
    }

    // Make sure the username and credential are empty for STUN
    iceServers[0].username[0] = '\0';
    iceServers[0].credential[0] = '\0';

    // Use the KVS STUN server URL
    SNPRINTF(iceServers[0].urls, MAX_ICE_CONFIG_URI_LEN, KINESIS_VIDEO_STUN_URL,
             pClientData->channelInfo.pRegion, pKinesisVideoStunUrlPostFix);

    // Get the TURN servers from the configuration
    retStatus = signalingClientGetIceConfigInfoCount(pClientData->signalingClientHandle, &iceConfigCount);
    if (STATUS_FAILED(retStatus)) {
        ESP_LOGW(TAG, "Failed to get ice config count, proceeding anyway...");
        retStatus = STATUS_SUCCESS;
        iceConfigCount = 0;
    }

    /* signalingClientGetIceConfigInfoCount can return more than one turn server. Use only one to optimize
     * candidate gathering latency. But user can also choose to use more than 1 turn server. */
    UINT32 maxTurnServer = 1;  // This can be made configurable if needed
    for (uriCount = 0, i = 0; i < maxTurnServer && i < iceConfigCount; i++) {
        retStatus = signalingClientGetIceConfigInfo(pClientData->signalingClientHandle, i, &pIceConfigInfo);
        if (STATUS_FAILED(retStatus)) {
            ESP_LOGW(TAG, "Failed to get ice config, proceeding anyway...");
            retStatus = STATUS_SUCCESS;
            break;
        }
        for (j = 0; j < pIceConfigInfo->uriCount && (uriCount + 1) < MAX_ICE_SERVERS_COUNT; j++) {
            /*
             * if iceServers[uriCount + 1].urls is "turn:ip:port?transport=udp" then ICE will try TURN over UDP
             * if iceServers[uriCount + 1].urls is "turn:ip:port?transport=tcp" then ICE will try TURN over TCP/TLS
             * if iceServers[uriCount + 1].urls is "turns:ip:port?transport=udp", it's currently ignored because sdk dont do TURN
             * over DTLS yet. if iceServers[uriCount + 1].urls is "turns:ip:port?transport=tcp" then ICE will try TURN over TCP/TLS
             * if iceServers[uriCount + 1].urls is "turn:ip:port" then ICE will try both TURN over UDP and TCP/TLS
             *
             * It's recommended to not pass too many TURN iceServers because it will slow down ice gathering in non-trickle mode.
             */

            STRNCPY(iceServers[uriCount + 1].urls, pIceConfigInfo->uris[j], MAX_ICE_CONFIG_URI_LEN);
            STRNCPY(iceServers[uriCount + 1].credential, pIceConfigInfo->password, MAX_ICE_CONFIG_CREDENTIAL_LEN);
            STRNCPY(iceServers[uriCount + 1].username, pIceConfigInfo->userName, MAX_ICE_CONFIG_USER_NAME_LEN);

            uriCount++;
        }
    }

    // Add 1 for the STUN server
    *pIceConfigCount = uriCount + 1;
    ESP_LOGI(TAG, "Total ICE servers configured: %d", *pIceConfigCount);

CleanUp:
    return retStatus;
}

/**
 * @brief Wrapper for get_ice_server_by_idx to convert return types
 */
static WEBRTC_STATUS kvsQueryServerGetByIdxWrapper(void *pSignalingClient, int index, bool useTurn, uint8_t **data, int *len, bool *have_more)
{
    STATUS retStatus = kvsSignalingQueryServerGetByIdx(pSignalingClient, index, useTurn, data, len, have_more);
    return (retStatus == STATUS_SUCCESS) ? WEBRTC_STATUS_SUCCESS : WEBRTC_STATUS_INTERNAL_ERROR;
}

/**
 * @brief Query ICE server by index (following reference RPC pattern)
 *
 * This function implements the same pattern as app_signaling_queryServer_get_by_idx:
 * - Index 0: Returns STUN server immediately, triggers background TURN refresh if needed
 * - Index 1+: Returns TURN servers from cached ICE configuration
 */
STATUS kvsSignalingQueryServerGetByIdx(PVOID pSignalingClient, int index, bool useTurn, uint8_t **data, int *len, bool *have_more)
{
    STATUS retStatus = STATUS_SUCCESS;
    KvsSignalingClientData *pClientData = (KvsSignalingClientData *)pSignalingClient;
    static UINT32 uriCount = 0;
    static PIceConfigInfo pIceConfigInfo = NULL;
    static bool is_ice_refresh_in_progress = false;

    *len = 0;
    *have_more = false;
    ss_ice_server_t* pIceServer = NULL;

    CHK(pClientData != NULL && data != NULL && len != NULL && have_more != NULL, STATUS_NULL_ARG);

    ESP_LOGI(TAG, "kvsSignalingQueryServerGetByIdx: index=%d, useTurn=%s", index, useTurn ? "true" : "false");

    if (index == 0) {
        uriCount = 1; // Start with STUN server
        is_ice_refresh_in_progress = false;

        // Allocate ICE server structure
        pIceServer = (ss_ice_server_t*) MEMCALLOC(1, sizeof(ss_ice_server_t));
        CHK(pIceServer != NULL, STATUS_NOT_ENOUGH_MEMORY);
        *len = sizeof(ss_ice_server_t);

        // Set the STUN server (region-aware KVS STUN server)
        PCHAR pKinesisVideoStunUrlPostFix = KINESIS_VIDEO_STUN_URL_POSTFIX;
        // If region is in CN, add CN region uri postfix
        if (pClientData->channelInfo.pRegion != NULL && STRSTR(pClientData->channelInfo.pRegion, "cn-")) {
            pKinesisVideoStunUrlPostFix = KINESIS_VIDEO_STUN_URL_POSTFIX_CN;
        }

        SNPRINTF(pIceServer->urls, SS_MAX_ICE_CONFIG_URI_LEN, KINESIS_VIDEO_STUN_URL,
                 pClientData->channelInfo.pRegion, pKinesisVideoStunUrlPostFix);
        pIceServer->username[0] = '\0';    // STUN has no username
        pIceServer->credential[0] = '\0';  // STUN has no credential

        if (useTurn && pClientData->signalingClientHandle != INVALID_SIGNALING_CLIENT_HANDLE_VALUE) {
            // Use the standardized ICE refresh check mechanism
            bool refreshNeeded = false;
            WEBRTC_STATUS checkStatus = kvsIsIceRefreshNeededWrapper(pClientData, &refreshNeeded);

            if (checkStatus == WEBRTC_STATUS_SUCCESS && refreshNeeded) {
                is_ice_refresh_in_progress = true;
                ESP_LOGI(TAG, "ICE refresh needed - triggering background refresh");

                // Trigger background refresh using work queue
                esp_err_t result = esp_work_queue_add_task(&kvs_refresh_ice_task, (void*)pClientData->signalingClientHandle);
                if (result != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to queue background ICE refresh: %d", result);
                }
            } else if (checkStatus == WEBRTC_STATUS_SUCCESS) {
                ESP_LOGI(TAG, "ICE configuration is up to date");
            } else {
                ESP_LOGW(TAG, "ICE refresh check failed, proceeding anyway");
            }
        }

        ESP_LOGI(TAG, "Sending immediate STUN server: %s", pIceServer->urls);
        ESP_LOGI(TAG, "ICE refresh in progress: %s", is_ice_refresh_in_progress ? "true" : "false");
        *have_more = true; // Always ask for more to get TURN servers
        goto CleanUp;

    } else if (is_ice_refresh_in_progress) {
        if (useTurn && pClientData->signalingClientHandle != INVALID_SIGNALING_CLIENT_HANDLE_VALUE) {
            // Check if refresh completed using standardized mechanism
            bool refreshNeeded = false;
            WEBRTC_STATUS checkStatus = kvsIsIceRefreshNeededWrapper(pClientData, &refreshNeeded);

            if (checkStatus == WEBRTC_STATUS_SUCCESS && refreshNeeded) {
                ESP_LOGI(TAG, "ICE refresh still in progress - asking caller to wait");
                *have_more = true; // Ask caller to wait for refresh completion
                goto CleanUp;
            } else if (checkStatus == WEBRTC_STATUS_SUCCESS) {
                ESP_LOGI(TAG, "ICE refresh completed");
                is_ice_refresh_in_progress = false;
            } else {
                ESP_LOGW(TAG, "ICE refresh check failed, assuming completed");
                is_ice_refresh_in_progress = false;
            }
        }
    }

    if (index == 1) {
        if (useTurn && pClientData->signalingClientHandle != INVALID_SIGNALING_CLIENT_HANDLE_VALUE) {
            // Final check that ICE configuration is ready using standardized mechanism
            bool refreshNeeded = false;
            WEBRTC_STATUS checkStatus = kvsIsIceRefreshNeededWrapper(pClientData, &refreshNeeded);

            if (checkStatus != WEBRTC_STATUS_SUCCESS || refreshNeeded) {
                ESP_LOGE(TAG, "ICE configuration still not ready for TURN servers");
                *have_more = false;
                goto CleanUp;
            }

            // Get ICE configuration count
            UINT32 iceConfigCount = 0;
            STATUS status = signalingClientGetIceConfigInfoCount(pClientData->signalingClientHandle, &iceConfigCount);
            if (STATUS_FAILED(status)) {
                ESP_LOGE(TAG, "Failed to get ICE config count: 0x%08x", status);
                *have_more = false;
                goto CleanUp;
            }

            ESP_LOGI(TAG, "Retrieved %d ICE configurations", iceConfigCount);

            if (iceConfigCount > 0) {
                // Get the first ICE config info (contains TURN servers)
                PIceConfigInfo pIceConfigInfoPtr = NULL;
                status = signalingClientGetIceConfigInfo(pClientData->signalingClientHandle, 0, &pIceConfigInfoPtr);
                if (STATUS_FAILED(status) || pIceConfigInfoPtr == NULL) {
                    ESP_LOGE(TAG, "Failed to get ICE config info: 0x%08x", status);
                    *have_more = false;
                    goto CleanUp;
                }

                uriCount += pIceConfigInfoPtr->uriCount;
                ESP_LOGI(TAG, "Total URI count (STUN + TURN): %d", uriCount);

                // Store the ICE config for subsequent requests
                pIceConfigInfo = pIceConfigInfoPtr;
            }
        }
    }

    // Handle TURN server requests (index >= 1)
    if (index >= 1 && pIceConfigInfo != NULL) {
        UINT32 turnIndex = index - 1; // Convert to 0-based TURN index

        if (turnIndex < pIceConfigInfo->uriCount) {
            // Allocate and populate TURN server
            pIceServer = (ss_ice_server_t*)MEMCALLOC(1, sizeof(ss_ice_server_t));
            CHK(pIceServer != NULL, STATUS_NOT_ENOUGH_MEMORY);
            *len = sizeof(ss_ice_server_t);

            STRNCPY(pIceServer->urls, pIceConfigInfo->uris[turnIndex], SS_MAX_ICE_CONFIG_URI_LEN);
            STRNCPY(pIceServer->username, pIceConfigInfo->userName, SS_MAX_ICE_CONFIG_USER_NAME_LEN);
            STRNCPY(pIceServer->credential, pIceConfigInfo->password, SS_MAX_ICE_CONFIG_CREDENTIAL_LEN);

            *have_more = (index < uriCount - 1);

            ESP_LOGI(TAG, "Sending TURN server %d: %s (user: %s) [have_more: %s]",
                     turnIndex, pIceServer->urls, pIceServer->username, *have_more ? "true" : "false");
            goto CleanUp;
        }
    }

    ESP_LOGI(TAG, "No more ICE servers available for index %d", index);
    *have_more = false;

CleanUp:
    *data = (uint8_t*)pIceServer;
    return retStatus;
}

/**
 * @brief Set the role type for the signaling client
 */
STATUS setKvsSignalingRoleType(PVOID pSignalingClient, SIGNALING_CHANNEL_ROLE_TYPE role_type)
{
    STATUS retStatus = STATUS_SUCCESS;
    KvsSignalingClientData *pClientData = (KvsSignalingClientData *)pSignalingClient;

    CHK(pClientData != NULL, STATUS_NULL_ARG);

    // Update the channel role type
    pClientData->channelInfo.channelRoleType = role_type;

    // Update the client ID based on role type
    if (role_type == SIGNALING_CHANNEL_ROLE_TYPE_MASTER) {
        STRCPY(pClientData->clientInfo.clientId, DEFAULT_MASTER_CLIENT_ID);
    } else {
        STRCPY(pClientData->clientInfo.clientId, DEFAULT_VIEWER_CLIENT_ID);
    }

CleanUp:
    return retStatus;
}

/**
 * @brief Wrapper function to match the portable interface signature
 */
static WEBRTC_STATUS kvsInitWrapper(void *signaling_cfg, void **ppSignalingClient)
{
    STATUS retStatus = createKvsSignalingClient((kvs_signaling_config_t *) signaling_cfg, ppSignalingClient);
    // Convert KVS SDK STATUS to portable WEBRTC_STATUS
    return (retStatus == STATUS_SUCCESS) ? WEBRTC_STATUS_SUCCESS : WEBRTC_STATUS_INTERNAL_ERROR;
}

/**
 * @brief Wrapper for connect to convert return types
 */
static WEBRTC_STATUS kvsConnectWrapper(void *pSignalingClient)
{
    STATUS retStatus = connectKvsSignalingClient(pSignalingClient);
    return (retStatus == STATUS_SUCCESS) ? WEBRTC_STATUS_SUCCESS : WEBRTC_STATUS_INTERNAL_ERROR;
}

/**
 * @brief Wrapper for disconnect to convert return types
 */
static WEBRTC_STATUS kvsDisconnectWrapper(void *pSignalingClient)
{
    STATUS retStatus = disconnectKvsSignalingClient(pSignalingClient);
    return (retStatus == STATUS_SUCCESS) ? WEBRTC_STATUS_SUCCESS : WEBRTC_STATUS_INTERNAL_ERROR;
}

/**
 * @brief Wrapper for send_message to convert return types
 */
static WEBRTC_STATUS kvsSendMessageWrapper(void *pSignalingClient, esp_webrtc_signaling_message_t *pMessage)
{
    STATUS retStatus = sendKvsSignalingMessage(pSignalingClient, pMessage);
    return (retStatus == STATUS_SUCCESS) ? WEBRTC_STATUS_SUCCESS : WEBRTC_STATUS_INTERNAL_ERROR;
}

/**
 * @brief Wrapper for free to convert return types
 */
static WEBRTC_STATUS kvsFreeWrapper(void *pSignalingClient)
{
    STATUS retStatus = freeKvsSignalingClient(pSignalingClient);
    return (retStatus == STATUS_SUCCESS) ? WEBRTC_STATUS_SUCCESS : WEBRTC_STATUS_INTERNAL_ERROR;
}

/**
 * @brief Wrapper for set_callback to convert types
 */
static WEBRTC_STATUS kvsSetCallbacksWrapper(void *pSignalingClient,
                                            uint64_t customData,
                                            WEBRTC_STATUS (*on_msg_received)(uint64_t, esp_webrtc_signaling_message_t*),
                                            WEBRTC_STATUS (*on_state_changed)(uint64_t, webrtc_signaling_client_state_t),
                                            WEBRTC_STATUS (*on_error)(uint64_t, WEBRTC_STATUS, char*, uint32_t))
{
    STATUS retStatus = STATUS_SUCCESS;
    KvsSignalingClientData *pClientData = (KvsSignalingClientData *)pSignalingClient;
    CallbackAdapterData *pAdapterData = NULL;

    CHK(pClientData != NULL, STATUS_NULL_ARG);

    // Allocate adapter data if we have callbacks that need conversion
    if (on_state_changed != NULL || on_error != NULL) {
        pAdapterData = (CallbackAdapterData *)MEMCALLOC(1, SIZEOF(CallbackAdapterData));
        CHK(pAdapterData != NULL, STATUS_NOT_ENOUGH_MEMORY);

        pAdapterData->originalOnStateChanged = on_state_changed;
        pAdapterData->originalOnError = on_error;
        pAdapterData->originalCustomData = (uint64_t)customData;

        // Store adapter data in client data for cleanup
        pClientData->pCallbackAdapterData = pAdapterData;
    }

    retStatus = setKvsSignalingCallbacks(pSignalingClient,
                                       (PVOID)pAdapterData,
                                       on_msg_received,
                                       on_state_changed ? adapterStateChangedCallback : NULL,
                                       on_error ? adapterErrorCallback : NULL);

CleanUp:
    if (STATUS_FAILED(retStatus) && pAdapterData != NULL) {
        MEMFREE(pAdapterData);
        if (pClientData != NULL) {
            pClientData->pCallbackAdapterData = NULL;
        }
    }

    return (retStatus == STATUS_SUCCESS) ? WEBRTC_STATUS_SUCCESS : WEBRTC_STATUS_INTERNAL_ERROR;
}

/**
 * @brief Wrapper for set_role_type to convert types
 */
static WEBRTC_STATUS kvsSetRoleTypeWrapper(void *pSignalingClient, webrtc_signaling_channel_role_type_t role_type)
{
    // Convert portable role type to KVS role type
    SIGNALING_CHANNEL_ROLE_TYPE kvsRoleType;
    switch (role_type) {
        case WEBRTC_SIGNALING_CHANNEL_ROLE_TYPE_MASTER:
            kvsRoleType = SIGNALING_CHANNEL_ROLE_TYPE_MASTER;
            break;
        case WEBRTC_SIGNALING_CHANNEL_ROLE_TYPE_VIEWER:
            kvsRoleType = SIGNALING_CHANNEL_ROLE_TYPE_VIEWER;
            break;
        default:
            ESP_LOGW(TAG, "Unknown role type: %d, defaulting to MASTER", role_type);
            kvsRoleType = SIGNALING_CHANNEL_ROLE_TYPE_MASTER;
            break;
    }

    STATUS retStatus = setKvsSignalingRoleType(pSignalingClient, kvsRoleType);
    return (retStatus == STATUS_SUCCESS) ? WEBRTC_STATUS_SUCCESS : WEBRTC_STATUS_INTERNAL_ERROR;
}

/**
 * @brief Wrapper for get_ice_servers to convert types
 *
 * @param pSignalingClient - KVS signaling client instance
 * @param pIceConfigCount - IN/OUT - Number of ICE servers
 * @param pIceServersArray - OUT - Array of ICE servers (RtcIceServer format)
 */
static WEBRTC_STATUS kvsGetIceServersWrapper(void *pSignalingClient, uint32_t *pIceConfigCount, void *pIceServersArray)
{
    STATUS retStatus = STATUS_SUCCESS;
    RtcIceServer iceServers[MAX_ICE_SERVERS_COUNT] = {0};

    // Call the KVS function with the local ice servers array
    PUINT32 kvsIceConfigCount = (PUINT32)pIceConfigCount;
    retStatus = getKvsSignalingIceServers(pSignalingClient, kvsIceConfigCount, iceServers);

    uint32_t max_ice_config_count = SS_MAX_ICE_SERVERS_COUNT;
    if (max_ice_config_count > MAX_ICE_CONFIG_COUNT) {
        max_ice_config_count = MAX_ICE_CONFIG_COUNT;
    }

    if (STATUS_SUCCEEDED(retStatus) && *pIceConfigCount > 0) {
        if (*pIceConfigCount > max_ice_config_count) {
            *pIceConfigCount = max_ice_config_count; // Adjust the count to the max allowed
        }
        // Copy the iceServers array to the output
        MEMCPY(pIceServersArray, iceServers, sizeof(RtcIceServer) * (*pIceConfigCount));
        ESP_LOGI(TAG, "KVS get_ice_servers: copied %d servers to generic iceServers array", *pIceConfigCount);
    }

    return (retStatus == STATUS_SUCCESS) ? WEBRTC_STATUS_SUCCESS : WEBRTC_STATUS_INTERNAL_ERROR;
}

/**
 * @brief Wrapper for is_ice_refresh_needed to convert return types
 */
static WEBRTC_STATUS kvsIsIceRefreshNeededWrapper(void *pSignalingClient, bool *refreshNeeded)
{
    if (refreshNeeded == NULL) {
        return WEBRTC_STATUS_NULL_ARG;
    }

    KvsSignalingClientData *pClientData = (KvsSignalingClientData *)pSignalingClient;
    if (pClientData == NULL || pClientData->signalingClientHandle == INVALID_SIGNALING_CLIENT_HANDLE_VALUE) {
        // If no signaling client, assume refresh is needed
        *refreshNeeded = true;
        return WEBRTC_STATUS_SUCCESS;
    }

#ifdef CONFIG_USE_ESP_WEBSOCKET_CLIENT
    // Use ESP signaling specific function if available
    BOOL refreshResult = signaling_is_ice_config_refresh_needed((PSignalingClient)pClientData->signalingClientHandle);
    *refreshNeeded = (refreshResult == TRUE);
#else
    // Default implementation: Use KVS signaling APIs to check ICE config availability
    UINT32 iceConfigCount = 0;
    STATUS status = signalingClientGetIceConfigInfoCount(pClientData->signalingClientHandle, &iceConfigCount);

    if (STATUS_FAILED(status) || iceConfigCount == 0) {
        // If we can't get ICE config count or there are no configs, refresh is needed
        *refreshNeeded = true;
    } else {
        // If we have ICE configurations, assume they're valid for now
        // (More sophisticated expiration checking would require access to internal state)
        *refreshNeeded = false;
    }
#endif

    return WEBRTC_STATUS_SUCCESS;
}

static WEBRTC_STATUS kvsRefreshIceConfigurationWrapper(void *pSignalingClient)
{
    KvsSignalingClientData *pClientData = (KvsSignalingClientData *)pSignalingClient;
    if (pClientData == NULL || pClientData->signalingClientHandle == INVALID_SIGNALING_CLIENT_HANDLE_VALUE) {
        ESP_LOGE(TAG, "Invalid signaling client for ICE refresh");
        return WEBRTC_STATUS_INVALID_ARG;
    }

#ifdef CONFIG_USE_ESP_WEBSOCKET_CLIENT
    ESP_LOGI(TAG, "Triggering KVS ICE configuration refresh");

    // Trigger the background refresh using the KVS signaling client
    STATUS status = refresh_ice_configuration((PSignalingClient)pClientData->signalingClientHandle);
    if (STATUS_FAILED(status)) {
        ESP_LOGE(TAG, "Failed to refresh ICE configuration: 0x%08x", status);
        return WEBRTC_STATUS_INTERNAL_ERROR;
    }

    ESP_LOGI(TAG, "ICE configuration refresh triggered successfully");
#else
    ESP_LOGE(TAG, "ICE refresh is not supported in default websocket client mode, skipping...");
#endif

    return WEBRTC_STATUS_SUCCESS;
}

/**
 * @brief Get the KVS signaling client interface
 */
webrtc_signaling_client_if_t* kvs_signaling_client_if_get(void)
{
    static webrtc_signaling_client_if_t kvs_signaling_client_if = {
        .init = kvsInitWrapper,
        .connect = kvsConnectWrapper,
        .disconnect = kvsDisconnectWrapper,
        .send_message = kvsSendMessageWrapper,
        .free = kvsFreeWrapper,
        .set_callback = kvsSetCallbacksWrapper,
        .set_role_type = kvsSetRoleTypeWrapper,
        .get_ice_servers = kvsGetIceServersWrapper,
        .get_ice_server_by_idx = kvsQueryServerGetByIdxWrapper,
        .is_ice_refresh_needed = kvsIsIceRefreshNeededWrapper,
        .refresh_ice_configuration = kvsRefreshIceConfigurationWrapper
    };

    return &kvs_signaling_client_if;
}
