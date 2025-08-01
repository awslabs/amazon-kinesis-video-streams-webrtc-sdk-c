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

#include "esp_work_queue.h"
#include <com/amazonaws/kinesis/video/webrtcclient/Include.h>

static const char *TAG = "kvs_signaling";

// Forward declarations for internal KVS functions (not exposed in public header)
STATUS createKvsSignalingClient(PKvsSignalingConfig pConfig, PVOID *ppSignalingClient);
STATUS connectKvsSignalingClient(PVOID pSignalingClient);
STATUS disconnectKvsSignalingClient(PVOID pSignalingClient);
STATUS sendKvsSignalingMessage(PVOID pSignalingClient, esp_webrtc_signaling_message_t *pMessage);
STATUS freeKvsSignalingClient(PVOID pSignalingClient);
STATUS setKvsSignalingCallbacks(PVOID pSignalingClient,
                              PVOID customData,
                              STATUS (*onMessageReceived)(UINT64, esp_webrtc_signaling_message_t*),
                              STATUS (*onStateChanged)(UINT64, SIGNALING_CLIENT_STATE),
                              STATUS (*onError)(UINT64, STATUS, PCHAR, UINT32));
STATUS setKvsSignalingRoleType(PVOID pSignalingClient, SIGNALING_CHANNEL_ROLE_TYPE roleType);
STATUS getKvsSignalingIceServers(PVOID pSignalingClient, PUINT32 pIceConfigCount, PRtcConfiguration pRtcConfiguration);

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
    STATUS (*onMessageReceived)(UINT64, esp_webrtc_signaling_message_t*);
    STATUS (*onStateChanged)(UINT64, SIGNALING_CLIENT_STATE);
    STATUS (*onError)(UINT64, STATUS, PCHAR, UINT32);

    // Adapter data for callback conversion (if needed)
    CallbackAdapterData *pCallbackAdapterData;

    // Configuration
    KvsSignalingConfig config;

    // Metrics
    SignalingClientMetrics metrics;
} KvsSignalingClientData;

/**
 * @brief Convert SIGNALING_CLIENT_STATE to webrtc_signaling_client_state_t
 */
static webrtc_signaling_client_state_t convertKvsToWebrtcState(SIGNALING_CLIENT_STATE kvsState)
{
    switch (kvsState) {
        case SIGNALING_CLIENT_STATE_NEW:
            return WEBRTC_SIGNALING_CLIENT_STATE_NEW;
        case SIGNALING_CLIENT_STATE_CONNECTING:
            return WEBRTC_SIGNALING_CLIENT_STATE_CONNECTING;
        case SIGNALING_CLIENT_STATE_CONNECTED:
            return WEBRTC_SIGNALING_CLIENT_STATE_CONNECTED;
        case SIGNALING_CLIENT_STATE_DISCONNECTED:
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
    if (pClientData->onStateChanged != NULL) {
        return pClientData->onStateChanged((UINT64)pClientData->customData, state);
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
    if (pClientData->onError != NULL) {
        return pClientData->onError((UINT64)pClientData->customData, status, errorMsg, errorMsgLen);
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
    if (pClientData->onMessageReceived != NULL) {
        // Convert KVS message format to standardized WebRTC message format
        retStatus = convertKvsToWebRtcMessage(pReceivedSignalingMessage, &webRtcMessage);
        CHK_STATUS(retStatus);

        CallbackAdapterData *pAdapterData = (CallbackAdapterData *) pClientData->customData;
        // Call the user callback with the standardized message format
        retStatus = pClientData->onMessageReceived((UINT64) pAdapterData->originalCustomData, &webRtcMessage);
    }

CleanUp:
    return retStatus;
}

/**
 * @brief Initialize KVS signaling client
 */
STATUS createKvsSignalingClient(PKvsSignalingConfig pConfig, PVOID *ppSignalingClient)
{
    STATUS retStatus = STATUS_SUCCESS;
    KvsSignalingClientData *pClientData = NULL;

    CHK(pConfig != NULL && ppSignalingClient != NULL, STATUS_NULL_ARG);

    // Allocate client data
    pClientData = (KvsSignalingClientData *)MEMCALLOC(1, SIZEOF(KvsSignalingClientData));
    CHK(pClientData != NULL, STATUS_NOT_ENOUGH_MEMORY);

    // Copy configuration
    MEMCPY(&pClientData->config, pConfig, SIZEOF(KvsSignalingConfig));

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
    STRCPY(pClientData->clientInfo.clientId, SAMPLE_MASTER_CLIENT_ID);

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
    PAwsCredentials pCredentials = NULL;
    BOOL credentialsAcquired = FALSE;
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
    // Free the credentials if we got them
    if (pCredentials != NULL) {
        // freeAwsCredentials(&pCredentials);
    }

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
                              STATUS (*onMessageReceived)(UINT64, esp_webrtc_signaling_message_t*),
                              STATUS (*onStateChanged)(UINT64, SIGNALING_CLIENT_STATE),
                              STATUS (*onError)(UINT64, STATUS, PCHAR, UINT32))
{
    STATUS retStatus = STATUS_SUCCESS;
    KvsSignalingClientData *pClientData = (KvsSignalingClientData *)pSignalingClient;

    CHK(pClientData != NULL, STATUS_NULL_ARG);

    // Store the callbacks
    pClientData->customData = customData;
    pClientData->onMessageReceived = onMessageReceived;
    pClientData->onStateChanged = onStateChanged;
    pClientData->onError = onError;

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
STATUS getKvsSignalingIceServers(PVOID pSignalingClient, PUINT32 pIceConfigCount, PRtcConfiguration pRtcConfiguration)
{
    STATUS retStatus = STATUS_SUCCESS;
    KvsSignalingClientData *pClientData = (KvsSignalingClientData *)pSignalingClient;
    UINT32 i, j, iceConfigCount, uriCount = 0;
    PIceConfigInfo pIceConfigInfo;
    PCHAR pKinesisVideoStunUrlPostFix;

    CHK(pClientData != NULL && pIceConfigCount != NULL && pRtcConfiguration != NULL, STATUS_NULL_ARG);

    // Check if signaling client handle is valid - if not, fall back to default STUN servers
    if (pClientData->signalingClientHandle == INVALID_SIGNALING_CLIENT_HANDLE_VALUE) {
        ESP_LOGW(TAG, "Signaling client not yet connected, using fallback STUN servers");

        // Use fallback STUN server
        SNPRINTF(pRtcConfiguration->iceServers[0].urls, MAX_ICE_CONFIG_URI_LEN,
                 "stun:stun.l.google.com:19302");

        // Make sure credentials are empty for STUN
        pRtcConfiguration->iceServers[0].username[0] = '\0';
        pRtcConfiguration->iceServers[0].credential[0] = '\0';

        *pIceConfigCount = 1;
        ESP_LOGI(TAG, "Using fallback STUN server: %s", pRtcConfiguration->iceServers[0].urls);
        CHK(FALSE, STATUS_SUCCESS);
    }

    // Set the STUN server
    pKinesisVideoStunUrlPostFix = KINESIS_VIDEO_STUN_URL_POSTFIX;
    // If region is in CN, add CN region uri postfix
    if (STRSTR(pClientData->channelInfo.pRegion, "cn-")) {
        pKinesisVideoStunUrlPostFix = KINESIS_VIDEO_STUN_URL_POSTFIX_CN;
    }

    // Make sure the username and credential are empty for STUN
    pRtcConfiguration->iceServers[0].username[0] = '\0';
    pRtcConfiguration->iceServers[0].credential[0] = '\0';

    // Use the KVS STUN server URL
    SNPRINTF(pRtcConfiguration->iceServers[0].urls, MAX_ICE_CONFIG_URI_LEN, KINESIS_VIDEO_STUN_URL,
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
             * if configuration.iceServers[uriCount + 1].urls is "turn:ip:port?transport=udp" then ICE will try TURN over UDP
             * if configuration.iceServers[uriCount + 1].urls is "turn:ip:port?transport=tcp" then ICE will try TURN over TCP/TLS
             * if configuration.iceServers[uriCount + 1].urls is "turns:ip:port?transport=udp", it's currently ignored because sdk dont do TURN
             * over DTLS yet. if configuration.iceServers[uriCount + 1].urls is "turns:ip:port?transport=tcp" then ICE will try TURN over TCP/TLS
             * if configuration.iceServers[uriCount + 1].urls is "turn:ip:port" then ICE will try both TURN over UDP and TCP/TLS
             *
             * It's recommended to not pass too many TURN iceServers to configuration because it will slow down ice gathering in non-trickle mode.
             */

            STRNCPY(pRtcConfiguration->iceServers[uriCount + 1].urls, pIceConfigInfo->uris[j], MAX_ICE_CONFIG_URI_LEN);
            STRNCPY(pRtcConfiguration->iceServers[uriCount + 1].credential, pIceConfigInfo->password, MAX_ICE_CONFIG_CREDENTIAL_LEN);
            STRNCPY(pRtcConfiguration->iceServers[uriCount + 1].username, pIceConfigInfo->userName, MAX_ICE_CONFIG_USER_NAME_LEN);

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
 * @brief Set the role type for the signaling client
 */
STATUS setKvsSignalingRoleType(PVOID pSignalingClient, SIGNALING_CHANNEL_ROLE_TYPE roleType)
{
    STATUS retStatus = STATUS_SUCCESS;
    KvsSignalingClientData *pClientData = (KvsSignalingClientData *)pSignalingClient;

    CHK(pClientData != NULL, STATUS_NULL_ARG);

    // Update the channel role type
    pClientData->channelInfo.channelRoleType = roleType;

    // Update the client ID based on role type
    if (roleType == SIGNALING_CHANNEL_ROLE_TYPE_MASTER) {
        STRCPY(pClientData->clientInfo.clientId, SAMPLE_MASTER_CLIENT_ID);
    } else {
        STRCPY(pClientData->clientInfo.clientId, SAMPLE_VIEWER_CLIENT_ID);
    }

CleanUp:
    return retStatus;
}

/**
 * @brief Wrapper function to match the portable interface signature
 */
static WEBRTC_STATUS kvsInitWrapper(void *pSignalingConfig, void **ppSignalingClient)
{
    STATUS retStatus = createKvsSignalingClient((PKvsSignalingConfig)pSignalingConfig, ppSignalingClient);
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
 * @brief Wrapper for sendMessage to convert return types
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
 * @brief Wrapper for setCallbacks to convert types
 */
static WEBRTC_STATUS kvsSetCallbacksWrapper(void *pSignalingClient,
                                            uint64_t customData,
                                            WEBRTC_STATUS (*onMessageReceived)(uint64_t, esp_webrtc_signaling_message_t*),
                                            WEBRTC_STATUS (*onStateChanged)(uint64_t, webrtc_signaling_client_state_t),
                                            WEBRTC_STATUS (*onError)(uint64_t, WEBRTC_STATUS, char*, uint32_t))
{
    STATUS retStatus = STATUS_SUCCESS;
    KvsSignalingClientData *pClientData = (KvsSignalingClientData *)pSignalingClient;
    CallbackAdapterData *pAdapterData = NULL;

    CHK(pClientData != NULL, STATUS_NULL_ARG);

    // Allocate adapter data if we have callbacks that need conversion
    if (onStateChanged != NULL || onError != NULL) {
        pAdapterData = (CallbackAdapterData *)MEMCALLOC(1, SIZEOF(CallbackAdapterData));
        CHK(pAdapterData != NULL, STATUS_NOT_ENOUGH_MEMORY);

        pAdapterData->originalOnStateChanged = onStateChanged;
        pAdapterData->originalOnError = onError;
        pAdapterData->originalCustomData = (uint64_t)customData;

        // Store adapter data in client data for cleanup
        pClientData->pCallbackAdapterData = pAdapterData;
    }

    retStatus = setKvsSignalingCallbacks(pSignalingClient,
                                       (PVOID)pAdapterData,
                                       onMessageReceived,
                                       onStateChanged ? adapterStateChangedCallback : NULL,
                                       onError ? adapterErrorCallback : NULL);

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
 * @brief Wrapper for setRoleType to convert types
 */
static WEBRTC_STATUS kvsSetRoleTypeWrapper(void *pSignalingClient, webrtc_signaling_channel_role_type_t roleType)
{
    // Convert portable role type to KVS role type
    SIGNALING_CHANNEL_ROLE_TYPE kvsRoleType;
    switch (roleType) {
        case WEBRTC_SIGNALING_CHANNEL_ROLE_TYPE_MASTER:
            kvsRoleType = SIGNALING_CHANNEL_ROLE_TYPE_MASTER;
            break;
        case WEBRTC_SIGNALING_CHANNEL_ROLE_TYPE_VIEWER:
            kvsRoleType = SIGNALING_CHANNEL_ROLE_TYPE_VIEWER;
            break;
        default:
            ESP_LOGW(TAG, "Unknown role type: %d, defaulting to MASTER", roleType);
            kvsRoleType = SIGNALING_CHANNEL_ROLE_TYPE_MASTER;
            break;
    }

    STATUS retStatus = setKvsSignalingRoleType(pSignalingClient, kvsRoleType);
    return (retStatus == STATUS_SUCCESS) ? WEBRTC_STATUS_SUCCESS : WEBRTC_STATUS_INTERNAL_ERROR;
}

/**
 * @brief Wrapper for getIceServers to convert types
 */
static WEBRTC_STATUS kvsGetIceServersWrapper(void *pSignalingClient, uint32_t *pIceConfigCount, void *pRtcConfiguration)
{
    PUINT32 kvsIceConfigCount = (PUINT32)pIceConfigCount;
    STATUS retStatus = getKvsSignalingIceServers(pSignalingClient, kvsIceConfigCount, (PRtcConfiguration)pRtcConfiguration);
    return (retStatus == STATUS_SUCCESS) ? WEBRTC_STATUS_SUCCESS : WEBRTC_STATUS_INTERNAL_ERROR;
}

/**
 * @brief Get the KVS signaling client interface
 */
WebRtcSignalingClientInterface* getKvsSignalingClientInterface(void)
{
    static WebRtcSignalingClientInterface kvsSignalingInterface = {
        .init = kvsInitWrapper,
        .connect = kvsConnectWrapper,
        .disconnect = kvsDisconnectWrapper,
        .sendMessage = kvsSendMessageWrapper,
        .free = kvsFreeWrapper,
        .setCallbacks = kvsSetCallbacksWrapper,
        .setRoleType = kvsSetRoleTypeWrapper,
        .getIceServers = kvsGetIceServersWrapper
    };

    return &kvsSignalingInterface;
}
