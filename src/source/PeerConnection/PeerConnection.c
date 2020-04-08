#define LOG_CLASS "PeerConnection"

#include "../Include_i.h"

static volatile ATOMIC_BOOL gKvsWebRtcInitialized = (SIZE_T) FALSE;

STATUS allocateSrtp(PKvsPeerConnection pKvsPeerConnection)
{
    DtlsKeyingMaterial dtlsKeyingMaterial;
    STATUS retStatus = STATUS_SUCCESS;
    BOOL locked = FALSE;

    MEMSET(&dtlsKeyingMaterial, 0, SIZEOF(DtlsKeyingMaterial));

    CHK(pKvsPeerConnection != NULL, STATUS_SUCCESS);
    CHK_STATUS(dtlsSessionVerifyRemoteCertificateFingerprint(pKvsPeerConnection->pDtlsSession, pKvsPeerConnection->remoteCertificateFingerprint));
    CHK_STATUS(dtlsSessionPopulateKeyingMaterial(pKvsPeerConnection->pDtlsSession, &dtlsKeyingMaterial));

    MUTEX_LOCK(pKvsPeerConnection->pSrtpSessionLock);
    locked = TRUE;

    CHK_STATUS(initSrtpSession(
            pKvsPeerConnection->dtlsIsServer ? dtlsKeyingMaterial.clientWriteKey : dtlsKeyingMaterial.serverWriteKey,
            pKvsPeerConnection->dtlsIsServer ? dtlsKeyingMaterial.serverWriteKey : dtlsKeyingMaterial.clientWriteKey,
            dtlsKeyingMaterial.srtpProfile,
            &(pKvsPeerConnection->pSrtpSession)
    ));

CleanUp:
    if (locked) {
        MUTEX_UNLOCK(pKvsPeerConnection->pSrtpSessionLock);
    }

    if (STATUS_FAILED(retStatus)) {
        DLOGW("dtlsSessionPopulateKeyingMaterial failed with 0x%08x", retStatus);
    }


    return retStatus;
}

STATUS allocateSctpSortDataChannelsDataCallback(UINT64 customData, PHashEntry pHashEntry)
{
    STATUS retStatus = STATUS_SUCCESS;
    PAllocateSctpSortDataChannelsData data = (PAllocateSctpSortDataChannelsData) customData;
    PKvsDataChannel pKvsDataChannel = (PKvsDataChannel) pHashEntry->value;

    CHK(customData != 0, STATUS_NULL_ARG);

    pKvsDataChannel->channelId = data->currentDataChannelId;
    CHK_STATUS(hashTablePut(data->pKvsPeerConnection->pDataChannels, pKvsDataChannel->channelId, (UINT64) pKvsDataChannel));

    data->currentDataChannelId += 2;

CleanUp:
    return retStatus;
}

STATUS allocateSctp(PKvsPeerConnection pKvsPeerConnection)
{
    STATUS retStatus = STATUS_SUCCESS;
    SctpSessionCallbacks sctpSessionCallbacks;
    AllocateSctpSortDataChannelsData data;
    UINT32 currentDataChannelId = 0;
    PKvsDataChannel pKvsDataChannel = NULL;

    CHK(pKvsPeerConnection != NULL, STATUS_NULL_ARG);
    currentDataChannelId = (pKvsPeerConnection->dtlsIsServer) ? 1 : 0;

    // Re-sort DataChannel hashmap using proper streamIds if we are offerer or answerer
    data.currentDataChannelId = currentDataChannelId;
    data.pKvsPeerConnection = pKvsPeerConnection;
    data.unkeyedDataChannels = pKvsPeerConnection->pDataChannels;
    CHK_STATUS(hashTableCreateWithParams(CODEC_HASH_TABLE_BUCKET_COUNT, CODEC_HASH_TABLE_BUCKET_LENGTH, &pKvsPeerConnection->pDataChannels));
    CHK_STATUS(hashTableIterateEntries(data.unkeyedDataChannels, (UINT64) &data, allocateSctpSortDataChannelsDataCallback));

    // Free unkeyed DataChannels
    CHK_LOG_ERR_NV(hashTableClear(data.unkeyedDataChannels));
    CHK_LOG_ERR_NV(hashTableFree(data.unkeyedDataChannels));

    // Create the SCTP Session
    sctpSessionCallbacks.outboundPacketFunc = onSctpSessionOutboundPacket;
    sctpSessionCallbacks.dataChannelMessageFunc = onSctpSessionDataChannelMessage;
    sctpSessionCallbacks.dataChannelOpenFunc = onSctpSessionDataChannelOpen;
    sctpSessionCallbacks.customData = (UINT64) pKvsPeerConnection;
    CHK_STATUS(createSctpSession(&sctpSessionCallbacks, &(pKvsPeerConnection->pSctpSession)));

    for (; currentDataChannelId < data.currentDataChannelId; currentDataChannelId += 2) {
        CHK_STATUS(hashTableGet(pKvsPeerConnection->pDataChannels, currentDataChannelId, (PUINT64) &pKvsDataChannel));
        CHK(pKvsDataChannel != NULL, STATUS_INTERNAL_ERROR);
        sctpSessionWriteDcep(pKvsPeerConnection->pSctpSession, currentDataChannelId, pKvsDataChannel->dataChannel.name, STRLEN(pKvsDataChannel->dataChannel.name), NULL);

        if (pKvsDataChannel->onOpen != NULL) {
            pKvsDataChannel->onOpen(pKvsDataChannel->onOpenCustomData);
        }
    }

CleanUp:
    return retStatus;
}

VOID onInboundPacket(UINT64 customData, PBYTE buff, UINT32 buffLen)
{
    STATUS retStatus = STATUS_SUCCESS;
    PKvsPeerConnection pKvsPeerConnection = (PKvsPeerConnection) customData;
    BOOL isDtlsConnected = FALSE;
    INT32 signedBuffLen = buffLen;

    CHK(signedBuffLen > 2 && pKvsPeerConnection != NULL, STATUS_SUCCESS);

    /*
     demux each packet off of its first byte
     https://tools.ietf.org/html/rfc5764#section-5.1.2
                 +----------------+
                  | 127 < B < 192 -+--> forward to RTP
                  |                |
      packet -->  |  19 < B < 64  -+--> forward to DTLS
                  |                |
                  |       B < 2   -+--> forward to STUN
                  +----------------+
    */
    if (buff[0] > 19 && buff[0] < 64) {
        dtlsSessionProcessPacket(pKvsPeerConnection->pDtlsSession, buff, &signedBuffLen);

        if (signedBuffLen > 0) {
            CHK_STATUS(putSctpPacket(pKvsPeerConnection->pSctpSession, buff, signedBuffLen));
        }

        CHK_STATUS(dtlsSessionIsInitFinished(pKvsPeerConnection->pDtlsSession, &isDtlsConnected));
        if (isDtlsConnected) {
            if (pKvsPeerConnection->pSrtpSession == NULL) {
                CHK_STATUS(allocateSrtp(pKvsPeerConnection));
            }

            if (pKvsPeerConnection->pSctpSession == NULL) {
                CHK_STATUS(allocateSctp(pKvsPeerConnection));
            }
        }

    } else if ((buff[0] > 127 && buff[0] < 192) && (pKvsPeerConnection->pSrtpSession != NULL)) {
        if (buff[1] >= 192 && buff[1] <= 223) {
            if (STATUS_FAILED(retStatus = decryptSrtcpPacket(pKvsPeerConnection->pSrtpSession, buff, &signedBuffLen))) {
                DLOGW("decryptSrtcpPacket failed with 0x%08x", retStatus);
                CHK(FALSE, STATUS_SUCCESS);
            }

            CHK_STATUS(onRtcpPacket(pKvsPeerConnection, buff, signedBuffLen));
        } else {
            if (STATUS_FAILED(retStatus = decryptSrtpPacket(pKvsPeerConnection->pSrtpSession, buff, &signedBuffLen))) {
                DLOGW("decryptSrtpPacket failed with 0x%08x", retStatus);
                CHK(FALSE, STATUS_SUCCESS);
            }

            CHK_STATUS(sendPacketToRtpReceiver(pKvsPeerConnection, buff, signedBuffLen));
        }
    }

CleanUp:

    CHK_LOG_ERR_NV(retStatus);
}

STATUS sendPacketToRtpReceiver(PKvsPeerConnection pKvsPeerConnection, PBYTE pBuffer, UINT32 bufferLen)
{
    STATUS retStatus = STATUS_SUCCESS;
    PDoubleListNode pCurNode = NULL;
    PKvsRtpTransceiver pTransceiver;
    UINT64 item;
    UINT32 ssrc;
    PRtpPacket pRtpPacket;
    PBYTE pPayload = NULL;
    BOOL ownedByJitterBuffer = FALSE;

    CHK(pKvsPeerConnection != NULL && pBuffer != NULL, STATUS_NULL_ARG);
    CHK(bufferLen >= MIN_HEADER_LENGTH, STATUS_INVALID_ARG);

    ssrc = getInt32(*(PUINT32) (pBuffer + SSRC_OFFSET));

    CHK_STATUS(doubleListGetHeadNode(pKvsPeerConnection->pTransceievers, &pCurNode));
    while(pCurNode != NULL) {
        CHK_STATUS(doubleListGetNodeData(pCurNode, &item));
        pTransceiver = (PKvsRtpTransceiver) item;

        if (pTransceiver->jitterBufferSsrc == ssrc) {
            CHK(NULL != (pPayload = (PBYTE) MEMALLOC(bufferLen)), STATUS_NOT_ENOUGH_MEMORY);
            MEMCPY(pPayload, pBuffer, bufferLen);
            ownedByJitterBuffer = FALSE;
            CHK_STATUS(createRtpPacketFromBytes(pPayload, bufferLen, &pRtpPacket));
            CHK_STATUS(jitterBufferPush(pTransceiver->pJitterBuffer, pRtpPacket));
            ownedByJitterBuffer = TRUE;
            CHK(FALSE, STATUS_SUCCESS);
        }
        pCurNode = pCurNode->pNext;
    }

    DLOGW("No transceiver to handle inbound ssrc %u", ssrc);


CleanUp:
    if (!ownedByJitterBuffer) {
        MEMFREE(pPayload);
        CHK_LOG_ERR_NV(retStatus);
    }
    return retStatus;
}

STATUS onFrameReadyFunc(UINT64 customData, UINT16 startIndex, UINT16 endIndex, UINT32 frameSize)
{
    STATUS retStatus = STATUS_SUCCESS;
    PKvsRtpTransceiver pTransceiver = (PKvsRtpTransceiver) customData;
    PRtpPacket pPacket = NULL;
    Frame frame;
    UINT32 filledSize = 0;

    CHK(pTransceiver != NULL, STATUS_NULL_ARG);

    pPacket = pTransceiver->pJitterBuffer->pktBuffer[startIndex];
    CHK(pPacket != NULL, STATUS_NULL_ARG);

    if (frameSize > pTransceiver->peerFrameBufferSize) {
        MEMFREE(pTransceiver->peerFrameBuffer);
        pTransceiver->peerFrameBufferSize = (UINT32) (frameSize * PEER_FRAME_BUFFER_SIZE_INCREMENT_FACTOR);
        pTransceiver->peerFrameBuffer = (PBYTE) MEMALLOC(pTransceiver->peerFrameBufferSize);
        CHK(pTransceiver->peerFrameBuffer != NULL, STATUS_NOT_ENOUGH_MEMORY);
    }

    CHK_STATUS(jitterBufferFillFrameData(pTransceiver->pJitterBuffer, pTransceiver->peerFrameBuffer,
                                         frameSize, &filledSize, startIndex, endIndex));
    CHK(frameSize == filledSize, STATUS_INVALID_ARG_LEN);

    frame.version = FRAME_CURRENT_VERSION;
    frame.decodingTs = pPacket->header.timestamp * HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
    frame.presentationTs = frame.decodingTs;
    frame.frameData = pTransceiver->peerFrameBuffer;
    frame.size = frameSize;
    frame.duration = 0;
    // TODO: Fill frame flag and track id and index if we need to, currently those are not used by RtcRtpTransceiver
    if (pTransceiver->onFrame != NULL) {
        pTransceiver->onFrame(pTransceiver->onFrameCustomData, &frame);
    }

CleanUp:
    return retStatus;
}

STATUS onFrameDroppedFunc(UINT64 customData, UINT32 timestamp)
{
    UNUSED_PARAM(customData);
    DLOGW("Frame with timestamp %ld is dropped!", timestamp);
    return STATUS_SUCCESS;
}

VOID onIceConnectionStateChange(UINT64 customData, UINT64 connectionState)
{
    STATUS retStatus = STATUS_SUCCESS;
    PKvsPeerConnection pKvsPeerConnection = (PKvsPeerConnection) customData;
    BOOL locked = FALSE, reportConnectionStateChange = TRUE;
    RTC_PEER_CONNECTION_STATE newConnectionState = RTC_PEER_CONNECTION_STATE_NEW;

    CHK(pKvsPeerConnection != NULL, STATUS_NULL_ARG);
    CHK(pKvsPeerConnection->onConnectionStateChange != NULL, retStatus);

    MUTEX_LOCK(pKvsPeerConnection->peerConnectionObjLock);
    locked = TRUE;

    switch (connectionState) {
        case ICE_AGENT_STATE_NEW:
            // ice agent internal state. nothing to report
            reportConnectionStateChange = FALSE;
            break;

        case ICE_AGENT_STATE_GATHERING:
            // explicit fall-through
        case ICE_AGENT_STATE_WAITING_REMOTE_CREDENTIAL:
            newConnectionState = RTC_PEER_CONNECTION_STATE_NEW;
            break;

        case ICE_AGENT_STATE_CHECK_CONNECTION:
            newConnectionState = RTC_PEER_CONNECTION_STATE_CONNECTING;
            break;

        case ICE_AGENT_STATE_CONNECTED:
            // explicit fall-through
        case ICE_AGENT_STATE_NOMINATING:
            // explicit fall-through
        case ICE_AGENT_STATE_READY:
            newConnectionState = RTC_PEER_CONNECTION_STATE_CONNECTED;
            break;

        case ICE_AGENT_STATE_DISCONNECTED:
            newConnectionState = RTC_PEER_CONNECTION_STATE_DISCONNECTED;
            break;

        case ICE_AGENT_STATE_FAILED:
            newConnectionState = RTC_PEER_CONNECTION_STATE_FAILED;
            break;

        default:
            DLOGW("Unknown ice agent state %" PRIu64, connectionState);
            break;
    }

    if (reportConnectionStateChange && newConnectionState != pKvsPeerConnection->previousConnectionState) {
        pKvsPeerConnection->previousConnectionState = newConnectionState;
        pKvsPeerConnection->onConnectionStateChange(pKvsPeerConnection->onConnectionStateChangeCustomData,
                                                    newConnectionState);
    }

CleanUp:

    if (locked) {
        MUTEX_UNLOCK(pKvsPeerConnection->peerConnectionObjLock);
    }

    CHK_LOG_ERR_NV(retStatus);
}

VOID onNewIceLocalCandidate(UINT64 customData, PCHAR candidateSdpStr)
{
    STATUS retStatus = STATUS_SUCCESS;
    PKvsPeerConnection pKvsPeerConnection = (PKvsPeerConnection) customData;
    BOOL locked = FALSE;
    CHAR jsonStrBuffer[MAX_ICE_CANDIDATE_JSON_LEN];
    INT32 strCompleteLen = 0;
    PCHAR pIceCandidateStr = NULL;

    CHK(pKvsPeerConnection != NULL, STATUS_NULL_ARG);
    CHK(candidateSdpStr == NULL || STRLEN(candidateSdpStr) < MAX_SDP_ATTRIBUTE_VALUE_LENGTH, STATUS_INVALID_ARG);
    CHK(pKvsPeerConnection->onIceCandidate != NULL, retStatus); // do nothing if onIceCandidate is not implemented

    MUTEX_LOCK(pKvsPeerConnection->peerConnectionObjLock);
    locked = TRUE;

    if (candidateSdpStr != NULL) {
        strCompleteLen = SNPRINTF(jsonStrBuffer, ARRAY_SIZE(jsonStrBuffer), ICE_CANDIDATE_JSON_TEMPLATE, candidateSdpStr);
        CHK(strCompleteLen > 0, STATUS_INTERNAL_ERROR);
        CHK(strCompleteLen < (INT32) ARRAY_SIZE(jsonStrBuffer), STATUS_BUFFER_TOO_SMALL);
        pIceCandidateStr = jsonStrBuffer;
    }

    pKvsPeerConnection->onIceCandidate(pKvsPeerConnection->onIceCandidateCustomData,
                                                          pIceCandidateStr);

CleanUp:

    CHK_LOG_ERR_NV(retStatus);

    if (locked) {
        MUTEX_UNLOCK(pKvsPeerConnection->peerConnectionObjLock);
    }
}

VOID onSctpSessionOutboundPacket(UINT64 customData, PBYTE pPacket, UINT32 packetLen)
{
    STATUS retStatus = STATUS_SUCCESS;
    PKvsPeerConnection pKvsPeerConnection = NULL;
    if (customData == 0) {
        return;
    }

    pKvsPeerConnection = (PKvsPeerConnection) customData;
    CHK_STATUS(dtlsSessionPutApplicationData(pKvsPeerConnection->pDtlsSession, pPacket, packetLen));

CleanUp:
    if (STATUS_FAILED(retStatus)) {
        DLOGW("onSctpSessionOutboundPacket failed with 0x%08x", retStatus);
    }
}

VOID onSctpSessionDataChannelMessage(UINT64 customData, UINT32 channelId, BOOL isBinary, PBYTE pMessage, UINT32 pMessageLen)
{
    STATUS retStatus = STATUS_SUCCESS;
    PKvsPeerConnection pKvsPeerConnection = (PKvsPeerConnection) customData;
    PKvsDataChannel pKvsDataChannel = NULL;

    CHK(pKvsPeerConnection != NULL, STATUS_INTERNAL_ERROR);

    CHK_STATUS(hashTableGet(pKvsPeerConnection->pDataChannels, channelId, (PUINT64) &pKvsDataChannel));
    CHK(pKvsDataChannel != NULL && pKvsDataChannel->onMessage != NULL, STATUS_INTERNAL_ERROR);

    pKvsDataChannel->onMessage(pKvsDataChannel->onMessageCustomData, isBinary, pMessage, pMessageLen);

CleanUp:
    if (STATUS_FAILED(retStatus)) {
        DLOGW("onSctpSessionDataChannelMessage failed with 0x%08x", retStatus);
    }
}

VOID onSctpSessionDataChannelOpen(UINT64 customData, UINT32 channelId, PBYTE pName, UINT32 pNameLen)
{
    STATUS retStatus = STATUS_SUCCESS;
    PKvsPeerConnection pKvsPeerConnection = (PKvsPeerConnection) customData;
    PKvsDataChannel pKvsDataChannel = NULL;

    CHK(pKvsPeerConnection != NULL && pKvsPeerConnection->onDataChannel != NULL, STATUS_NULL_ARG);

    pKvsDataChannel = (PKvsDataChannel) MEMCALLOC(1, SIZEOF(KvsDataChannel));
    CHK(pKvsDataChannel != NULL, STATUS_NOT_ENOUGH_MEMORY);

    STRNCPY(pKvsDataChannel->dataChannel.name, (PCHAR) pName, pNameLen);
    pKvsDataChannel->pRtcPeerConnection = (PRtcPeerConnection) pKvsPeerConnection;
    pKvsDataChannel->channelId = channelId;

    CHK_STATUS(hashTablePut(pKvsPeerConnection->pDataChannels, channelId, (UINT64) pKvsDataChannel));

    pKvsPeerConnection->onDataChannel(pKvsPeerConnection->onDataChannelCustomData, &(pKvsDataChannel->dataChannel));

CleanUp:

    CHK_LOG_ERR_NV(retStatus);
}

VOID onDtlsOutboundPacket(UINT64 customData, PBYTE pBuffer, UINT32 bufferLen)
{
    PKvsPeerConnection pKvsPeerConnection = NULL;
    if (customData == 0) {
        return;
    }

    pKvsPeerConnection = (PKvsPeerConnection) customData;
    iceAgentSendPacket(pKvsPeerConnection->pIceAgent, pBuffer, bufferLen);
}

/* Generate a printable string that does not
 * need to be escaped when encoding in JSON
 */
STATUS generateJSONSafeString(PCHAR pDst, UINT32 len)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 i = 0;
    BYTE randBuffer[MAX_RAND_BUFFER_SIZE_FOR_NAME] = {0};

    CHK(pDst != NULL, STATUS_NULL_ARG);
    CHK(RAND_bytes(randBuffer, MIN(len, MAX_RAND_BUFFER_SIZE_FOR_NAME)), STATUS_INTERNAL_ERROR );

    for (i = 0; i < len; i++) {
        pDst[i] = VALID_CHAR_SET_FOR_JSON[randBuffer[i % MAX_RAND_BUFFER_SIZE_FOR_NAME] % (ARRAY_SIZE(VALID_CHAR_SET_FOR_JSON) - 1)];
    }

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS createPeerConnection(PRtcConfiguration pConfiguration, PRtcPeerConnection *ppPeerConnection)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PKvsPeerConnection pKvsPeerConnection = NULL;
    IceAgentCallbacks iceAgentCallbacks;
    DtlsSessionCallbacks dtlsSessionCallbacks;
    UINT32 logLevel = LOG_LEVEL_DEBUG;
    PCHAR logLevelStr = NULL;
    PConnectionListener pConnectionListener = NULL;

    CHK(pConfiguration != NULL && ppPeerConnection != NULL, STATUS_NULL_ARG);

    MEMSET(&iceAgentCallbacks, 0, SIZEOF(IceAgentCallbacks));
    MEMSET(&dtlsSessionCallbacks, 0, SIZEOF(DtlsSessionCallbacks));

    pKvsPeerConnection = (PKvsPeerConnection) MEMCALLOC(1, SIZEOF(KvsPeerConnection));
    CHK(pKvsPeerConnection != NULL, STATUS_NOT_ENOUGH_MEMORY);

    CHK_STATUS(timerQueueCreate(&pKvsPeerConnection->timerQueueHandle));

    pKvsPeerConnection->peerConnection.version = PEER_CONNECTION_CURRENT_VERSION;
    CHK_STATUS(generateJSONSafeString(pKvsPeerConnection->localIceUfrag, LOCAL_ICE_UFRAG_LEN));
    CHK_STATUS(generateJSONSafeString(pKvsPeerConnection->localIcePwd, LOCAL_ICE_PWD_LEN));
    CHK_STATUS(generateJSONSafeString(pKvsPeerConnection->localCNAME, LOCAL_CNAME_LEN));

    dtlsSessionCallbacks.customData = (UINT64) pKvsPeerConnection;
    dtlsSessionCallbacks.outboundPacketFn = onDtlsOutboundPacket;
    CHK_STATUS(createDtlsSession(&dtlsSessionCallbacks, pKvsPeerConnection->timerQueueHandle,
            pConfiguration->kvsRtcConfiguration.generatedCertificateBits,
	    pConfiguration->kvsRtcConfiguration.generateRSACertificate,
            pConfiguration->certificates, &pKvsPeerConnection->pDtlsSession));

    CHK_STATUS(hashTableCreateWithParams(CODEC_HASH_TABLE_BUCKET_COUNT, CODEC_HASH_TABLE_BUCKET_LENGTH, &pKvsPeerConnection->pCodecTable));
    CHK_STATUS(hashTableCreateWithParams(CODEC_HASH_TABLE_BUCKET_COUNT, CODEC_HASH_TABLE_BUCKET_LENGTH, &pKvsPeerConnection->pDataChannels));
    CHK_STATUS(hashTableCreateWithParams(RTX_HASH_TABLE_BUCKET_COUNT, RTX_HASH_TABLE_BUCKET_LENGTH, &pKvsPeerConnection->pRtxTable));
    CHK_STATUS(doubleListCreate(&(pKvsPeerConnection->pTransceievers)));

    if ((logLevelStr = GETENV(DEBUG_LOG_LEVEL_ENV_VAR)) != NULL) {
        CHK_STATUS(STRTOUI32(logLevelStr, NULL, 10, &logLevel));
        logLevel = MIN(MAX(logLevel, LOG_LEVEL_VERBOSE), LOG_LEVEL_SILENT);
    }

    SET_LOGGER_LOG_LEVEL(logLevel);

    pKvsPeerConnection->pSrtpSessionLock = MUTEX_CREATE(TRUE);
    pKvsPeerConnection->peerConnectionObjLock = MUTEX_CREATE(FALSE);
    pKvsPeerConnection->previousConnectionState = RTC_PEER_CONNECTION_STATE_NONE;
    pKvsPeerConnection->MTU = pConfiguration->kvsRtcConfiguration.maximumTransmissionUnit == 0 ? DEFAULT_MTU_SIZE : pConfiguration->kvsRtcConfiguration.maximumTransmissionUnit;

    iceAgentCallbacks.customData = (UINT64) pKvsPeerConnection;
    iceAgentCallbacks.inboundPacketFn = onInboundPacket;
    iceAgentCallbacks.connectionStateChangedFn = onIceConnectionStateChange;
    iceAgentCallbacks.newLocalCandidateFn = onNewIceLocalCandidate;
    CHK_STATUS(createConnectionListener(&pConnectionListener));
    // IceAgent will own the lifecycle of pConnectionListener;
    CHK_STATUS(createIceAgent(pKvsPeerConnection->localIceUfrag, pKvsPeerConnection->localIcePwd,
                              iceAgentCallbacks.customData, &iceAgentCallbacks, pConfiguration,
                              pKvsPeerConnection->timerQueueHandle, pConnectionListener, &pKvsPeerConnection->pIceAgent));

    *ppPeerConnection = (PRtcPeerConnection) pKvsPeerConnection;

CleanUp:

    CHK_LOG_ERR_NV(retStatus);

    if (STATUS_FAILED(retStatus)) {
        freePeerConnection((PRtcPeerConnection*) &pKvsPeerConnection);
    }

    LEAVES();
    return retStatus;
}

STATUS freeHashEntry(UINT64 customData, PHashEntry pHashEntry)
{
    UNUSED_PARAM(customData);
    MEMFREE((PVOID) pHashEntry->value);
    return STATUS_SUCCESS;
}

/*
 * NOT thread-safe
 */
STATUS freePeerConnection(PRtcPeerConnection *ppPeerConnection)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PKvsPeerConnection pKvsPeerConnection;
    PDoubleListNode pCurNode = NULL;
    UINT64 item = 0;

    CHK(ppPeerConnection != NULL, STATUS_NULL_ARG);

    pKvsPeerConnection = (PKvsPeerConnection) *ppPeerConnection;

    CHK(pKvsPeerConnection != NULL, retStatus);

    // free timer queue first to remove liveness provided by timer
    if (IS_VALID_TIMER_QUEUE_HANDLE(pKvsPeerConnection->timerQueueHandle)) {
        timerQueueShutdown(pKvsPeerConnection->timerQueueHandle);
    }

    // free structs that have their own thread. sctp has threads created by sctp library. iceAgent has the
    // connectionListener thread
    CHK_LOG_ERR_NV(freeSctpSession(&pKvsPeerConnection->pSctpSession));
    CHK_LOG_ERR_NV(freeIceAgent(&pKvsPeerConnection->pIceAgent));

    // free transceivers
    CHK_LOG_ERR_NV(doubleListGetHeadNode(pKvsPeerConnection->pTransceievers, &pCurNode));
    while(pCurNode != NULL) {
        CHK_LOG_ERR_NV(doubleListGetNodeData(pCurNode, &item));
        CHK_LOG_ERR_NV(freeKvsRtpTransceiver((PKvsRtpTransceiver *) &item));

        pCurNode = pCurNode->pNext;
    }

    // Free DataChannels
    CHK_LOG_ERR_NV(hashTableIterateEntries(pKvsPeerConnection->pDataChannels, 0, freeHashEntry));
    CHK_LOG_ERR_NV(hashTableFree(pKvsPeerConnection->pDataChannels));

    // free rest of structs
    CHK_LOG_ERR_NV(freeSrtpSession(&pKvsPeerConnection->pSrtpSession));
    CHK_LOG_ERR_NV(freeDtlsSession(&pKvsPeerConnection->pDtlsSession));
    CHK_LOG_ERR_NV(doubleListFree(pKvsPeerConnection->pTransceievers));
    CHK_LOG_ERR_NV(hashTableFree(pKvsPeerConnection->pCodecTable));
    CHK_LOG_ERR_NV(hashTableFree(pKvsPeerConnection->pRtxTable));
    if (IS_VALID_MUTEX_VALUE(pKvsPeerConnection->pSrtpSessionLock)) {
        MUTEX_FREE(pKvsPeerConnection->pSrtpSessionLock);
    }

    if (IS_VALID_MUTEX_VALUE(pKvsPeerConnection->peerConnectionObjLock)) {
        MUTEX_FREE(pKvsPeerConnection->peerConnectionObjLock);
    }

    if (IS_VALID_TIMER_QUEUE_HANDLE(pKvsPeerConnection->timerQueueHandle)) {
        timerQueueFree(&pKvsPeerConnection->timerQueueHandle);
    }

    SAFE_MEMFREE(pKvsPeerConnection);

    *ppPeerConnection = NULL;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS peerConnectionOnIceCandidate(PRtcPeerConnection pRtcPeerConnection, UINT64 customData, RtcOnIceCandidate rtcOnIceCandidate)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PKvsPeerConnection pKvsPeerConnection = (PKvsPeerConnection) pRtcPeerConnection;
    BOOL locked = FALSE;

    CHK(pKvsPeerConnection != NULL && rtcOnIceCandidate != NULL, STATUS_NULL_ARG);

    MUTEX_LOCK(pKvsPeerConnection->peerConnectionObjLock);
    locked = TRUE;

    pKvsPeerConnection->onIceCandidate = rtcOnIceCandidate;
    pKvsPeerConnection->onIceCandidateCustomData = customData;

CleanUp:

    if (locked) {
        MUTEX_UNLOCK(pKvsPeerConnection->peerConnectionObjLock);
    }

    LEAVES();
    return retStatus;
}

STATUS peerConnectionOnDataChannel(PRtcPeerConnection pRtcPeerConnection, UINT64 customData, RtcOnDataChannel rtcOnDataChannel)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PKvsPeerConnection pKvsPeerConnection = (PKvsPeerConnection) pRtcPeerConnection;
    BOOL locked = FALSE;

    CHK(pKvsPeerConnection != NULL && rtcOnDataChannel != NULL, STATUS_NULL_ARG);

    MUTEX_LOCK(pKvsPeerConnection->peerConnectionObjLock);
    locked = TRUE;

    pKvsPeerConnection->onDataChannel = rtcOnDataChannel;
    pKvsPeerConnection->onDataChannelCustomData = customData;

CleanUp:

    if (locked) {
        MUTEX_UNLOCK(pKvsPeerConnection->peerConnectionObjLock);
    }

    LEAVES();
    return retStatus;
}

STATUS peerConnectionOnConnectionStateChange(PRtcPeerConnection pRtcPeerConnection, UINT64 customData,
                                             RtcOnConnectionStateChange rtcOnConnectionStateChange)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PKvsPeerConnection pKvsPeerConnection = (PKvsPeerConnection) pRtcPeerConnection;
    BOOL locked = FALSE;

    CHK(pKvsPeerConnection != NULL && rtcOnConnectionStateChange != NULL, STATUS_NULL_ARG);

    MUTEX_LOCK(pKvsPeerConnection->peerConnectionObjLock);
    locked = TRUE;

    pKvsPeerConnection->onConnectionStateChange = rtcOnConnectionStateChange;
    pKvsPeerConnection->onConnectionStateChangeCustomData = customData;

CleanUp:

    if (locked) {
        MUTEX_UNLOCK(pKvsPeerConnection->peerConnectionObjLock);
    }

    LEAVES();
    return retStatus;
}

STATUS peerConnectionGetCurrentLocalDescription(PRtcPeerConnection pRtcPeerConnection, PRtcSessionDescriptionInit pRtcSessionDescriptionInit)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    SessionDescription sessionDescription;
    UINT32 deserializeLen = 0;
    PKvsPeerConnection pKvsPeerConnection = (PKvsPeerConnection) pRtcPeerConnection;

    CHK(pRtcPeerConnection != NULL && pRtcSessionDescriptionInit != NULL, STATUS_NULL_ARG);
    // do nothing if remote session description hasn't been received
    CHK(pKvsPeerConnection->remoteSessionDescription.sessionName[0] != '\0', retStatus);

    MEMSET(&sessionDescription, 0x00, SIZEOF(SessionDescription));

    CHK_STATUS(populateSessionDescription(pKvsPeerConnection, &(pKvsPeerConnection->remoteSessionDescription), &sessionDescription));
    CHK_STATUS(deserializeSessionDescription(&sessionDescription, NULL, &deserializeLen));
    CHK(deserializeLen <= MAX_SESSION_DESCRIPTION_INIT_SDP_LEN, STATUS_NOT_ENOUGH_MEMORY);

    CHK_STATUS(deserializeSessionDescription(&sessionDescription, pRtcSessionDescriptionInit->sdp, &deserializeLen));

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS setRemoteDescription(PRtcPeerConnection pPeerConnection, PRtcSessionDescriptionInit pSessionDescriptionInit)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PCHAR remoteIceUfrag = NULL, remoteIcePwd = NULL;
    UINT32 i, j;

    CHK(pPeerConnection != NULL, STATUS_NULL_ARG);
    PKvsPeerConnection pKvsPeerConnection = (PKvsPeerConnection) pPeerConnection;
    PSessionDescription pSessionDescription = &(pKvsPeerConnection->remoteSessionDescription);

    CHK(pSessionDescriptionInit != NULL, STATUS_NULL_ARG);

    MEMSET(pSessionDescription, 0x00, SIZEOF(SessionDescription));
    pKvsPeerConnection->dtlsIsServer = FALSE;

    CHK_STATUS(serializeSessionDescription(pSessionDescription, pSessionDescriptionInit->sdp));

    for (i = 0; i < pSessionDescription->sessionAttributesCount; i++) {
        if (STRCMP(pSessionDescription->sdpAttributes[i].attributeName, "fingerprint") == 0) {
            STRNCPY(pKvsPeerConnection->remoteCertificateFingerprint, pSessionDescription->sdpAttributes[i].attributeValue + 8, CERTIFICATE_FINGERPRINT_LENGTH);
        } else if (pKvsPeerConnection->isOffer && STRCMP(pSessionDescription->sdpAttributes[i].attributeName, "setup") == 0) {
            pKvsPeerConnection->dtlsIsServer = STRCMP(pSessionDescription->sdpAttributes[i].attributeValue, "active") == 0;
        }
    }

    for (i = 0; i < pSessionDescription->mediaCount; i++) {
        if (STRNCMP(pSessionDescription->mediaDescriptions[i].mediaName, "application", SIZEOF("application") - 1) == 0) {
            pKvsPeerConnection->sctpIsEnabled = TRUE;
        }

        for (j = 0; j < pSessionDescription->mediaDescriptions[i].mediaAttributesCount; j++) {
            if (STRCMP(pSessionDescription->mediaDescriptions[i].sdpAttributes[j].attributeName, "ice-ufrag") == 0) {
                remoteIceUfrag = pSessionDescription->mediaDescriptions[i].sdpAttributes[j].attributeValue;
            } else if (STRCMP(pSessionDescription->mediaDescriptions[i].sdpAttributes[j].attributeName, "ice-pwd") == 0) {
                remoteIcePwd = pSessionDescription->mediaDescriptions[i].sdpAttributes[j].attributeValue;
            } else if (STRCMP(pSessionDescription->mediaDescriptions[i].sdpAttributes[j].attributeName, "candidate") == 0) {
                // Ignore the return value, we have candidates we don't support yet like TURN
                iceAgentAddRemoteCandidate(pKvsPeerConnection->pIceAgent, pSessionDescription->mediaDescriptions[i].sdpAttributes[j].attributeValue);
            } else if (STRCMP(pSessionDescription->mediaDescriptions[i].sdpAttributes[j].attributeName, "fingerprint") == 0) {
                STRNCPY(pKvsPeerConnection->remoteCertificateFingerprint, pSessionDescription->mediaDescriptions[i].sdpAttributes[j].attributeValue + 8, CERTIFICATE_FINGERPRINT_LENGTH);
            } else if (pKvsPeerConnection->isOffer && STRCMP(pSessionDescription->mediaDescriptions[i].sdpAttributes[j].attributeName, "setup") == 0) {
                pKvsPeerConnection->dtlsIsServer = STRCMP(pSessionDescription->mediaDescriptions[i].sdpAttributes[j].attributeValue, "active") == 0;
            }
        }
    }

    CHK(remoteIceUfrag != NULL && remoteIcePwd != NULL, STATUS_SESSION_DESCRIPTION_MISSING_ICE_VALUES);
    CHK(pKvsPeerConnection->remoteCertificateFingerprint[0] != '\0', STATUS_SESSION_DESCRIPTION_MISSING_CERTIFICATE_FINGERPRINT);

    CHK_STATUS(dtlsSessionStart(pKvsPeerConnection->pDtlsSession, pKvsPeerConnection->dtlsIsServer));
    CHK_STATUS(iceAgentStartAgent(pKvsPeerConnection->pIceAgent, remoteIceUfrag, remoteIcePwd, pKvsPeerConnection->isOffer));
    if (!pKvsPeerConnection->isOffer) {
        CHK_STATUS(setPayloadTypesFromOffer(pKvsPeerConnection->pCodecTable, pKvsPeerConnection->pRtxTable, pSessionDescription));
    }
    CHK_STATUS(setTransceiverPayloadTypes(pKvsPeerConnection->pCodecTable, pKvsPeerConnection->pRtxTable, pKvsPeerConnection->pTransceievers));
    CHK_STATUS(setReceiversSsrc(pSessionDescription, pKvsPeerConnection->pTransceievers));

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS createOffer(PRtcPeerConnection pPeerConnection, PRtcSessionDescriptionInit pSessionDescriptionInit)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    SessionDescription sessionDescription;
    UINT32 deserializeLen = 0;

    PKvsPeerConnection pKvsPeerConnection = (PKvsPeerConnection) pPeerConnection;

    CHK(pKvsPeerConnection != NULL && pSessionDescriptionInit != NULL, STATUS_NULL_ARG);

    MEMSET(&sessionDescription, 0x00, SIZEOF(SessionDescription));
    pSessionDescriptionInit->type = SDP_TYPE_OFFER;
    pKvsPeerConnection->isOffer = TRUE;

    pKvsPeerConnection->sctpIsEnabled = TRUE;
    CHK_STATUS(setPayloadTypesForOffer(pKvsPeerConnection->pCodecTable));

    CHK_STATUS(populateSessionDescription(pKvsPeerConnection, &(pKvsPeerConnection->remoteSessionDescription), &sessionDescription));
    CHK_STATUS(deserializeSessionDescription(&sessionDescription, NULL, &deserializeLen));
    CHK(deserializeLen <= MAX_SESSION_DESCRIPTION_INIT_SDP_LEN, STATUS_NOT_ENOUGH_MEMORY);

    CHK_STATUS(deserializeSessionDescription(&sessionDescription, pSessionDescriptionInit->sdp, &deserializeLen));

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS createAnswer(PRtcPeerConnection pPeerConnection, PRtcSessionDescriptionInit pSessionDescriptionInit)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    PKvsPeerConnection pKvsPeerConnection = (PKvsPeerConnection) pPeerConnection;

    CHK(pKvsPeerConnection != NULL && pSessionDescriptionInit != NULL, STATUS_NULL_ARG);
    CHK(pKvsPeerConnection->remoteSessionDescription.sessionName[0] != '\0', STATUS_PEERCONNECTION_CREATE_ANSWER_WITHOUT_REMOTE_DESCRIPTION);

    pSessionDescriptionInit->type = SDP_TYPE_ANSWER;

    CHK_STATUS(peerConnectionGetCurrentLocalDescription(pPeerConnection, pSessionDescriptionInit));

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS setLocalDescription(PRtcPeerConnection pPeerConnection, PRtcSessionDescriptionInit pSessionDescriptionInit)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PKvsPeerConnection pKvsPeerConnection = (PKvsPeerConnection) pPeerConnection;

    CHK(pKvsPeerConnection != NULL && pSessionDescriptionInit != NULL, STATUS_NULL_ARG);

    CHK_STATUS(iceAgentStartGathering(pKvsPeerConnection->pIceAgent));

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS addTransceiver(PRtcPeerConnection pPeerConnection, PRtcMediaStreamTrack pRtcMediaStreamTrack, PRtcRtpTransceiverInit pRtcRtpTransceiverInit, PRtcRtpTransceiver *ppRtcRtpTransceiver)
{
    UNUSED_PARAM(pRtcRtpTransceiverInit);
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PKvsRtpTransceiver pKvsRtpTransceiver = NULL;
    PKvsPeerConnection pKvsPeerConnection = (PKvsPeerConnection) pPeerConnection;
    PJitterBuffer pJitterBuffer = NULL;
    DepayRtpPayloadFunc depayFunc;
    UINT32 clockRate = 0;
    UINT32 ssrc = (UINT32) RAND(), rtxSsrc = (UINT32) RAND();
    RTC_RTP_TRANSCEIVER_DIRECTION direction = RTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV;
    if(pRtcRtpTransceiverInit != NULL) {
        direction = pRtcRtpTransceiverInit->direction;
    }

    CHK(pKvsPeerConnection != NULL, STATUS_NULL_ARG);

    switch (pRtcMediaStreamTrack->codec) {
        case RTC_CODEC_OPUS:
            depayFunc = depayOpusFromRtpPayload;
            clockRate = OPUS_CLOCKRATE;
            break;

        case RTC_CODEC_MULAW:
        case RTC_CODEC_ALAW:
            depayFunc = depayG711FromRtpPayload;
            clockRate = PCM_CLOCKRATE;
            break;

        case RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE:
            depayFunc = depayH264FromRtpPayload;
            clockRate = VIDEO_CLOCKRATE;
            break;

        case RTC_CODEC_VP8:
            depayFunc = depayVP8FromRtpPayload;
            clockRate = VIDEO_CLOCKRATE;
            break;

        default:
            CHK(FALSE, STATUS_NOT_IMPLEMENTED);
    }

    //TODO: Add ssrc duplicate detection here not only relying on RAND()
    CHK_STATUS(createKvsRtpTransceiver(direction, pKvsPeerConnection, ssrc,
                        rtxSsrc, pRtcMediaStreamTrack, NULL, pRtcMediaStreamTrack->codec, &pKvsRtpTransceiver));
    CHK_STATUS(createJitterBuffer(onFrameReadyFunc, onFrameDroppedFunc, depayFunc, DEFAULT_JITTER_BUFFER_MAX_LATENCY,
                                  clockRate, (UINT64) pKvsRtpTransceiver, &pJitterBuffer));
    CHK_STATUS(kvsRtpTransceiverSetJitterBuffer(pKvsRtpTransceiver, pJitterBuffer));

    // after pKvsRtpTransceiver is successfully created, jitterBuffer will be freed by pKvsRtpTransceiver.
    pJitterBuffer = NULL;

    CHK_STATUS(doubleListInsertItemHead(pKvsPeerConnection->pTransceievers, (UINT64) pKvsRtpTransceiver));
    *ppRtcRtpTransceiver = (PRtcRtpTransceiver) pKvsRtpTransceiver;
    pKvsRtpTransceiver = NULL;

CleanUp:

    if (pJitterBuffer != NULL) {
        freeJitterBuffer(&pJitterBuffer);
    }

    if (pKvsRtpTransceiver != NULL) {
        freeKvsRtpTransceiver(&pKvsRtpTransceiver);
    }

    LEAVES();
    return retStatus;
}

STATUS addSupportedCodec(PRtcPeerConnection pPeerConnection, RTC_CODEC rtcCodec)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PKvsPeerConnection pKvsPeerConnection = (PKvsPeerConnection) pPeerConnection;

    CHK(pKvsPeerConnection != NULL, STATUS_NULL_ARG);

    CHK_STATUS(hashTablePut(pKvsPeerConnection->pCodecTable, rtcCodec, 0));

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS addIceCandidate(PRtcPeerConnection pPeerConnection, PCHAR pIceCandidate)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PKvsPeerConnection pKvsPeerConnection = (PKvsPeerConnection) pPeerConnection;

    CHK(pKvsPeerConnection != NULL && pIceCandidate != NULL, STATUS_NULL_ARG);

    iceAgentAddRemoteCandidate(pKvsPeerConnection->pIceAgent, pIceCandidate);

CleanUp:

    LEAVES();
    return retStatus;

}

STATUS initKvsWebRtc(VOID)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    CHK(!ATOMIC_LOAD_BOOL(&gKvsWebRtcInitialized), retStatus);

    SRAND(GETTIME());

    CHK(srtp_init() == srtp_err_status_ok, STATUS_SRTP_INIT_FAILED);

    // init endianness handling
    initializeEndianness();

    OpenSSL_add_ssl_algorithms();
    SSL_load_error_strings();
    // SSL_library_init() always returns "1", so it is safe to discard the return value.
    UNUSED_PARAM(SSL_library_init());

    CHK_STATUS(initSctpSession());

    ATOMIC_STORE_BOOL(&gKvsWebRtcInitialized, TRUE);

CleanUp:

    LEAVES();
    return retStatus;

}

STATUS deinitKvsWebRtc(VOID)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    CHK(ATOMIC_LOAD_BOOL(&gKvsWebRtcInitialized), retStatus);

    deinitSctpSession();

    srtp_shutdown();

    ATOMIC_STORE_BOOL(&gKvsWebRtcInitialized, FALSE);

CleanUp:

    LEAVES();
    return retStatus;

}
