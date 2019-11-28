#define LOG_CLASS "WebRtcSamples"
#include "Samples.h"

VOID onDataChannelMessage(UINT64 customData, BOOL isBinary, PBYTE pMessage, UINT32 pMessageLen)
{
    if (isBinary) {
        DLOGI("DataChannel Binary Message");
    } else {
        DLOGI("DataChannel String Message: %.*s\n", pMessageLen, pMessage);
    }
}

VOID onDataChannel(UINT64 customData, PRtcDataChannel pRtcDataChannel)
{
    DLOGI("New DataChannel has been opened %s \n", pRtcDataChannel->name);
    dataChannelOnMessage(pRtcDataChannel, customData, onDataChannelMessage);
}

VOID onConnectionStateChange(UINT64 customData, RTC_PEER_CONNECTION_STATE newState)
{
    PSampleConfiguration pSampleConfiguration = (PSampleConfiguration) customData;

    DLOGI("New connection state %u", newState);

    if (newState == RTC_PEER_CONNECTION_STATE_FAILED) {
        ATOMIC_STORE_BOOL(&pSampleConfiguration->terminateFlag, TRUE);
        CVAR_BROADCAST(pSampleConfiguration->cvar);
    }
}

STATUS viewerMessageReceived(UINT64 customData, PReceivedSignalingMessage pReceivedSignalingMessage)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleConfiguration pSampleConfiguration = (PSampleConfiguration) customData;

    CHK(pSampleConfiguration != NULL, STATUS_NULL_ARG);

    switch (pReceivedSignalingMessage->signalingMessage.messageType) {
        case SIGNALING_MESSAGE_TYPE_OFFER:
            DLOGE("Unexpected message SIGNALING_MESSAGE_TYPE_OFFER \n");
            break;
        case SIGNALING_MESSAGE_TYPE_ICE_CANDIDATE:
            CHK_STATUS(handleRemoteCandidate(pSampleConfiguration, &pReceivedSignalingMessage->signalingMessage));
            break;
        case SIGNALING_MESSAGE_TYPE_ANSWER:
            CHK_STATUS(handleAnswer(pSampleConfiguration, &pReceivedSignalingMessage->signalingMessage));
            break;
        default:
            DLOGW("Unknown message type %u", pReceivedSignalingMessage->signalingMessage.messageType);
    }

CleanUp:

    // Return success to continue
    return retStatus;
}

STATUS signalingClientStateChanged(UINT64 customData, SIGNALING_CLIENT_STATE state)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleConfiguration pSampleConfiguration = (PSampleConfiguration) customData;
    CHK(pSampleConfiguration != NULL, STATUS_NULL_ARG);

    DLOGV("Signaling client state changed to %d", state);

CleanUp:

    // Return success to continue
    return retStatus;
}

STATUS masterMessageReceived(UINT64 customData, PReceivedSignalingMessage pReceivedSignalingMessage)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleConfiguration pSampleConfiguration = (PSampleConfiguration) customData;

    CHK(pSampleConfiguration != NULL, STATUS_NULL_ARG);

    switch (pReceivedSignalingMessage->signalingMessage.messageType) {
        case SIGNALING_MESSAGE_TYPE_OFFER:
            CHK_STATUS(handleOffer(pSampleConfiguration, &pReceivedSignalingMessage->signalingMessage));
            break;
        case SIGNALING_MESSAGE_TYPE_ICE_CANDIDATE:
            CHK_STATUS(handleRemoteCandidate(pSampleConfiguration, &pReceivedSignalingMessage->signalingMessage));
            break;
        case SIGNALING_MESSAGE_TYPE_ANSWER:
            DLOGE("Unexpected message SIGNALING_MESSAGE_TYPE_ANSWER \n");
            break;
        default:
            DLOGW("Unknown message type %u", pReceivedSignalingMessage->signalingMessage.messageType);
    }

CleanUp:

    // Return success to continue
    return retStatus;
}

STATUS handleAnswer(PSampleConfiguration pSampleConfiguration, PSignalingMessage pSignalingMessage)
{
    STATUS retStatus = STATUS_SUCCESS;
    RtcSessionDescriptionInit answerSessionDescriptionInit;

    CHK_WARN(!ATOMIC_LOAD_BOOL(&pSampleConfiguration->terminateFlag), retStatus,
             "Dropping offer as peerConnection is shutting down");
    CHK_WARN(!ATOMIC_LOAD_BOOL(&pSampleConfiguration->peerConnectionStarted), retStatus,
             "Dropping offer as peerConnection already received answer");

    MEMSET(&answerSessionDescriptionInit, 0x00, SIZEOF(RtcSessionDescriptionInit));

    CHK_STATUS(deserializeSessionDescriptionInit(pSignalingMessage->payload, pSignalingMessage->payloadLen, &answerSessionDescriptionInit));
    CHK_STATUS(setRemoteDescription(pSampleConfiguration->pPeerConnection, &answerSessionDescriptionInit));
    ATOMIC_STORE_BOOL(&pSampleConfiguration->peerConnectionStarted, TRUE);

CleanUp:
    CHK_LOG_ERR(retStatus);

    return retStatus;
}

STATUS handleOffer(PSampleConfiguration pSampleConfiguration, PSignalingMessage pSignalingMessage)
{
    STATUS retStatus = STATUS_SUCCESS;
    BOOL locked = FALSE;

    CHK(pSampleConfiguration != NULL && pSignalingMessage != NULL, STATUS_NULL_ARG);
    CHK_WARN(!ATOMIC_LOAD_BOOL(&pSampleConfiguration->terminateFlag), retStatus,
             "Dropping offer as peerConnection is shutting down");
    CHK_WARN(!ATOMIC_LOAD_BOOL(&pSampleConfiguration->peerConnectionStarted), retStatus,
             "Dropping offer as peerConnection already received offer");

    RtcSessionDescriptionInit offerSessionDescriptionInit;
    PBYTE rawOfferSessionDescriptionInit = NULL;

    MEMSET(&offerSessionDescriptionInit, 0x00, SIZEOF(RtcSessionDescriptionInit));
    MEMSET(&pSampleConfiguration->answerSessionDescriptionInit, 0x00, SIZEOF(RtcSessionDescriptionInit));

    CHK_STATUS(deserializeSessionDescriptionInit(pSignalingMessage->payload, pSignalingMessage->payloadLen, &offerSessionDescriptionInit));
    CHK_STATUS(setRemoteDescription(pSampleConfiguration->pPeerConnection, &offerSessionDescriptionInit));
    ATOMIC_STORE_BOOL(&pSampleConfiguration->peerConnectionStarted, TRUE);
    CHK_STATUS(createAnswer(pSampleConfiguration->pPeerConnection, &pSampleConfiguration->answerSessionDescriptionInit));
    CHK_STATUS(setLocalDescription(pSampleConfiguration->pPeerConnection, &pSampleConfiguration->answerSessionDescriptionInit));

    MUTEX_LOCK(pSampleConfiguration->sampleConfigurationObjLock);
    locked = TRUE;

    if (!pSampleConfiguration->trickleIce) {
        while (!ATOMIC_LOAD_BOOL(&pSampleConfiguration->candidateGatheringDone)) {
            CHK_WARN(!ATOMIC_LOAD_BOOL(&pSampleConfiguration->terminateFlag), STATUS_INTERNAL_ERROR, "application terminated and candidate gathering still not done");
            CVAR_WAIT(pSampleConfiguration->cvar, pSampleConfiguration->sampleConfigurationObjLock, INFINITE_TIME_VALUE);
        }

        DLOGD("Candidate collection done for non trickle ice");
        // get the latest local description once candidate gathering is done
        CHK_STATUS(peerConnectionGetCurrentLocalDescription(pSampleConfiguration->pPeerConnection, &pSampleConfiguration->answerSessionDescriptionInit));
    }

    MUTEX_UNLOCK(pSampleConfiguration->sampleConfigurationObjLock);
    locked = FALSE;

    STRCPY(pSampleConfiguration->peerId, pSignalingMessage->peerClientId);
    ATOMIC_STORE_BOOL(&pSampleConfiguration->peerIdReceived, TRUE);

    CHK_STATUS(respondWithAnswer(pSampleConfiguration));

    if (pSampleConfiguration->videoSource != NULL) {
        THREAD_CREATE(&pSampleConfiguration->videoSenderTid, pSampleConfiguration->videoSource,
                      (PVOID) pSampleConfiguration);
    }

    if (pSampleConfiguration->audioSource != NULL) {
        THREAD_CREATE(&pSampleConfiguration->audioSenderTid, pSampleConfiguration->audioSource,
                      (PVOID) pSampleConfiguration);
    }

    if (pSampleConfiguration->receiveAudioVideoSource != NULL) {
        THREAD_CREATE(&pSampleConfiguration->receiveAudioVideoSenderTid, pSampleConfiguration->receiveAudioVideoSource,
                      (PVOID) pSampleConfiguration);
    }

CleanUp:

    CHK_LOG_ERR(retStatus);
    SAFE_MEMFREE(rawOfferSessionDescriptionInit);

    if (locked) {
        MUTEX_UNLOCK(pSampleConfiguration->sampleConfigurationObjLock);
    }

    return retStatus;
}

STATUS respondWithAnswer(PSampleConfiguration pSampleConfiguration)
{
    STATUS retStatus = STATUS_SUCCESS;
    SignalingMessage message;
    UINT32 buffLen = 0;

    CHK_STATUS(serializeSessionDescriptionInit(&pSampleConfiguration->answerSessionDescriptionInit, NULL, &buffLen));
    CHK_STATUS(serializeSessionDescriptionInit(&pSampleConfiguration->answerSessionDescriptionInit, message.payload, &buffLen));

    message.version = SIGNALING_MESSAGE_CURRENT_VERSION;
    message.messageType = SIGNALING_MESSAGE_TYPE_ANSWER;
    STRCPY(message.peerClientId, pSampleConfiguration->peerId);
    message.payloadLen = (UINT32) STRLEN(message.payload);

    retStatus = signalingClientSendMessageSync(pSampleConfiguration->signalingClientHandle, &message);

CleanUp:

    CHK_LOG_ERR(retStatus);
    return retStatus;
}

STATUS resetSampleConfigurationState(PSampleConfiguration pSampleConfiguration)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pSampleConfiguration != NULL, STATUS_NULL_ARG);

    ATOMIC_STORE_BOOL(&pSampleConfiguration->terminateFlag, FALSE);
    ATOMIC_STORE_BOOL(&pSampleConfiguration->peerConnectionStarted, FALSE);
    ATOMIC_STORE_BOOL(&pSampleConfiguration->candidateGatheringDone, FALSE);

CleanUp:

    CHK_LOG_ERR(retStatus);
    return retStatus;
}

VOID onIceCandidateHandler(UINT64 customData, PCHAR candidateJson)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleConfiguration pSampleConfiguration = (PSampleConfiguration) customData;
    SignalingMessage message;

    CHK(pSampleConfiguration != NULL, STATUS_NULL_ARG);

    if (candidateJson == NULL) {
        DLOGD("ice candidate gathering finished");
        ATOMIC_STORE_BOOL(&pSampleConfiguration->candidateGatheringDone, TRUE);
        CVAR_BROADCAST(pSampleConfiguration->cvar);

    } else if (pSampleConfiguration->trickleIce && ATOMIC_LOAD_BOOL(&pSampleConfiguration->peerIdReceived)) {
        message.version = SIGNALING_MESSAGE_CURRENT_VERSION;
        message.messageType = SIGNALING_MESSAGE_TYPE_ICE_CANDIDATE;
        STRCPY(message.peerClientId, pSampleConfiguration->peerId);
        message.payloadLen = (UINT32) STRLEN(candidateJson);
        STRCPY(message.payload, candidateJson);
        CHK_STATUS(signalingClientSendMessageSync(pSampleConfiguration->signalingClientHandle, &message));
    }

CleanUp:

    CHK_LOG_ERR(retStatus);
}

STATUS initializePeerConnection(PSampleConfiguration pSampleConfiguration)
{
    STATUS retStatus = STATUS_SUCCESS;
    RtcMediaStreamTrack videoTrack, audioTrack;
    RtcConfiguration configuration;
    UINT32 i, j, iceConfigCount, uriCount;
    PIceConfigInfo pIceConfigInfo;

    MEMSET(&configuration, 0x00, SIZEOF(RtcConfiguration));
    MEMSET(&videoTrack, 0x00, SIZEOF(RtcMediaStreamTrack));
    MEMSET(&audioTrack, 0x00, SIZEOF(RtcMediaStreamTrack));

    // Set the  STUN server
    SNPRINTF(configuration.iceServers[0].urls, MAX_ICE_CONFIG_URI_LEN, KINESIS_VIDEO_STUN_URL, pSampleConfiguration->pRegion);

    if (pSampleConfiguration->useTurn) {
        // Set the URIs from the configuration
        CHK_STATUS(signalingClientGetIceConfigInfoCout(pSampleConfiguration->signalingClientHandle, &iceConfigCount));

        for (uriCount = 0, i = 0; i < iceConfigCount; i++) {
            CHK_STATUS(signalingClientGetIceConfigInfo(pSampleConfiguration->signalingClientHandle, i, &pIceConfigInfo));
            for (j = 0; j < pIceConfigInfo->uriCount; j++) {
                CHECK(uriCount < MAX_ICE_SERVERS_COUNT);
                STRNCPY(configuration.iceServers[uriCount + 1].urls, pIceConfigInfo->uris[j], MAX_ICE_CONFIG_URI_LEN);
                STRNCPY(configuration.iceServers[uriCount + 1].credential, pIceConfigInfo->password, MAX_ICE_CONFIG_CREDENTIAL_LEN);
                STRNCPY(configuration.iceServers[uriCount + 1].username, pIceConfigInfo->userName, MAX_ICE_CONFIG_USER_NAME_LEN);

                uriCount++;
            }
        }
    }

    CHK_STATUS(createPeerConnection(&configuration, &pSampleConfiguration->pPeerConnection));
    CHK_STATUS(peerConnectionOnIceCandidate(pSampleConfiguration->pPeerConnection, (UINT64) pSampleConfiguration, onIceCandidateHandler));
    CHK_STATUS(peerConnectionOnConnectionStateChange(pSampleConfiguration->pPeerConnection, (UINT64) pSampleConfiguration, onConnectionStateChange));
    if (pSampleConfiguration->onDataChannel != NULL) {
        CHK_STATUS(peerConnectionOnDataChannel(pSampleConfiguration->pPeerConnection, (UINT64) pSampleConfiguration, pSampleConfiguration->onDataChannel));
    }

    // Declare that we support H264,Profile=42E01F,level-asymmetry-allowed=1,packetization-mode=1 and Opus
    CHK_STATUS(addSupportedCodec(pSampleConfiguration->pPeerConnection, RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE));
    CHK_STATUS(addSupportedCodec(pSampleConfiguration->pPeerConnection, RTC_CODEC_OPUS));

    // Add a SendRecv Transceiver of type video
    videoTrack.kind = MEDIA_STREAM_TRACK_KIND_VIDEO;
    videoTrack.codec = RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE;
    STRCPY(videoTrack.streamId, "myKvsVideoStream");
    STRCPY(videoTrack.trackId, "myVideoTrack");
    CHK_STATUS(addTransceiver(pSampleConfiguration->pPeerConnection, &videoTrack, NULL, &pSampleConfiguration->pVideoRtcRtpTransceiver));

    // Add a SendRecv Transceiver of type video
    audioTrack.kind = MEDIA_STREAM_TRACK_KIND_AUDIO;
    audioTrack.codec = RTC_CODEC_OPUS;
    STRCPY(audioTrack.streamId, "myKvsVideoStream");
    STRCPY(audioTrack.trackId, "myAudioTrack");
    CHK_STATUS(addTransceiver(pSampleConfiguration->pPeerConnection, &audioTrack, NULL, &pSampleConfiguration->pAudioRtcRtpTransceiver));
    transceiverOnFrame(pSampleConfiguration->pAudioRtcRtpTransceiver, (UINT64) pSampleConfiguration, sampleFrameHandler);

CleanUp:

    return retStatus;
}

VOID sampleFrameHandler(UINT64 customData, PFrame pFrame)
{
    DLOGI("Frame received. TrackId: %" PRIu64 ", Size: %u, Flags %u", pFrame->trackId, pFrame->size, pFrame->flags);
}

STATUS handleRemoteCandidate(PSampleConfiguration pSampleConfiguration, PSignalingMessage pSignalingMessage)
{
    STATUS retStatus = STATUS_SUCCESS;
    RtcIceCandidateInit iceCandidate;

    CHK_STATUS(deserializeRtcIceCandidateInit(pSignalingMessage->payload, pSignalingMessage->payloadLen, &iceCandidate));
    CHK_STATUS(addIceCandidate(pSampleConfiguration->pPeerConnection, iceCandidate.candidate));

CleanUp:

    CHK_LOG_ERR(retStatus);
    return retStatus;
}

STATUS createSampleConfiguration(PCHAR channelName, SIGNALING_CHANNEL_ROLE_TYPE roleType, BOOL trickleIce, BOOL useTurn, PSampleConfiguration* ppSampleConfiguration)
{
    STATUS retStatus = STATUS_SUCCESS;
    PCHAR pAccessKey, pSecretKey, pSessionToken;
    PSampleConfiguration pSampleConfiguration = NULL;

    CHK(ppSampleConfiguration != NULL, STATUS_NULL_ARG);

    CHK(NULL != (pSampleConfiguration = (PSampleConfiguration) MEMCALLOC(1, SIZEOF(SampleConfiguration))), STATUS_NOT_ENOUGH_MEMORY);

    pAccessKey = getenv(ACCESS_KEY_ENV_VAR);
    pSecretKey = getenv(SECRET_KEY_ENV_VAR);
    pSessionToken = getenv(SESSION_TOKEN_ENV_VAR);

    if ((pSampleConfiguration->pRegion = getenv(DEFAULT_REGION_ENV_VAR)) == NULL) {
        pSampleConfiguration->pRegion = DEFAULT_AWS_REGION;
    }

    // if ca cert path is not set from the environment, try to use the one that cmake detected
    if ((pSampleConfiguration->pCaCertPath = getenv(CACERT_PATH_ENV_VAR)) == NULL) {
        CHK_ERR(STRNLEN(DEFAULT_KVS_CACERT_PATH, MAX_PATH_LEN) > 0, STATUS_INVALID_OPERATION, "No ca cert path given");
        pSampleConfiguration->pCaCertPath = DEFAULT_KVS_CACERT_PATH;
    }

    CHK_STATUS(createStaticCredentialProvider(pAccessKey, 0,
                                              pSecretKey, 0,
                                              pSessionToken, 0,
                                              MAX_UINT64,
                                              &pSampleConfiguration->pCredentialProvider));

    pSampleConfiguration->audioSenderTid = INVALID_TID_VALUE;
    pSampleConfiguration->videoSenderTid = INVALID_TID_VALUE;
    pSampleConfiguration->replyTid = INVALID_TID_VALUE;
    pSampleConfiguration->signalingClientHandle = INVALID_SIGNALING_CLIENT_HANDLE_VALUE;
    pSampleConfiguration->sampleConfigurationObjLock = MUTEX_CREATE(FALSE);
    pSampleConfiguration->cvar = CVAR_CREATE();
    pSampleConfiguration->trickleIce = trickleIce;
    pSampleConfiguration->useTurn = useTurn;

    pSampleConfiguration->channelInfo.version = CHANNEL_INFO_CURRENT_VERSION;
    pSampleConfiguration->channelInfo.pChannelName = channelName;
    pSampleConfiguration->channelInfo.pKmsKeyId = NULL;
    pSampleConfiguration->channelInfo.tagCount = 0;
    pSampleConfiguration->channelInfo.pTags = NULL;
    pSampleConfiguration->channelInfo.channelType = SIGNALING_CHANNEL_TYPE_SINGLE_MASTER;
    pSampleConfiguration->channelInfo.channelRoleType = roleType;
    pSampleConfiguration->channelInfo.cachingEndpoint = FALSE;
    pSampleConfiguration->channelInfo.retry = TRUE;
    pSampleConfiguration->channelInfo.reconnect = TRUE;
    pSampleConfiguration->channelInfo.pCertPath = pSampleConfiguration->pCaCertPath;

    if (pSampleConfiguration->channelInfo.channelRoleType == SIGNALING_CHANNEL_ROLE_TYPE_VIEWER) {
        STRCPY(pSampleConfiguration->peerId, SAMPLE_MASTER_CLIENT_ID);
        ATOMIC_STORE_BOOL(&pSampleConfiguration->peerIdReceived, TRUE);
    } else {
        ATOMIC_STORE_BOOL(&pSampleConfiguration->peerIdReceived, FALSE);
    }

    ATOMIC_STORE_BOOL(&pSampleConfiguration->peerConnectionStarted, FALSE);
    ATOMIC_STORE_BOOL(&pSampleConfiguration->interrupted, FALSE);
    ATOMIC_STORE_BOOL(&pSampleConfiguration->candidateGatheringDone, FALSE);

CleanUp:

    if (STATUS_FAILED(retStatus)) {
        freeSampleConfiguration(&pSampleConfiguration);
    }

    if (ppSampleConfiguration != NULL) {
        *ppSampleConfiguration = pSampleConfiguration;
    }

    return retStatus;
}

STATUS freeSampleConfiguration(PSampleConfiguration* ppSampleConfiguration)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PSampleConfiguration pSampleConfiguration;

    CHK(ppSampleConfiguration != NULL, STATUS_NULL_ARG);
    pSampleConfiguration = *ppSampleConfiguration;

    CHK(pSampleConfiguration != NULL, retStatus);

    SAFE_MEMFREE(pSampleConfiguration->pVideoFrameBuffer);
    SAFE_MEMFREE(pSampleConfiguration->pAudioFrameBuffer);

    if (IS_VALID_CVAR_VALUE(pSampleConfiguration->cvar) &&
        IS_VALID_MUTEX_VALUE(pSampleConfiguration->sampleConfigurationObjLock)) {
        CVAR_BROADCAST(pSampleConfiguration->cvar);
        // lock to wait until awoken thread finish.
        MUTEX_LOCK(pSampleConfiguration->sampleConfigurationObjLock);
        MUTEX_UNLOCK(pSampleConfiguration->sampleConfigurationObjLock);
    }

    if (IS_VALID_MUTEX_VALUE(pSampleConfiguration->sampleConfigurationObjLock)) {
        MUTEX_FREE(pSampleConfiguration->sampleConfigurationObjLock);
    }

    freeStaticCredentialProvider(&pSampleConfiguration->pCredentialProvider);

    MEMFREE(*ppSampleConfiguration);
    *ppSampleConfiguration = NULL;

CleanUp:

    LEAVES();
    return retStatus;
}
