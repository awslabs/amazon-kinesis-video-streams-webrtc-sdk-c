#define LOG_CLASS "WebRtcSamples"
#include "Samples.h"

VOID onDataChannelMessage(UINT64 customData, BOOL isBinary, PBYTE pMessage, UINT32 pMessageLen)
{
    UNUSED_PARAM(customData);
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
    PSampleStreamingSession pSampleStreamingSession = (PSampleStreamingSession) customData;

    DLOGI("New connection state %u", newState);

    if (newState == RTC_PEER_CONNECTION_STATE_FAILED ||
        newState == RTC_PEER_CONNECTION_STATE_CLOSED ||
        newState == RTC_PEER_CONNECTION_STATE_DISCONNECTED) {
        ATOMIC_STORE_BOOL(&pSampleStreamingSession->terminateFlag, TRUE);
        CVAR_BROADCAST(pSampleStreamingSession->pSampleConfiguration->cvar);
    }
}

STATUS viewerMessageReceived(UINT64 customData, PReceivedSignalingMessage pReceivedSignalingMessage)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleConfiguration pSampleConfiguration = (PSampleConfiguration) customData;
    PSampleStreamingSession pSampleStreamingSession = NULL;
    BOOL locked = FALSE;

    CHK(pSampleConfiguration != NULL, STATUS_NULL_ARG);

    MUTEX_LOCK(pSampleConfiguration->sampleConfigurationObjLock);
    locked = TRUE;

    // viewer should only be viewing a single master. So there should only be one streaming session if running viewer sample
    CHK_ERR(pSampleConfiguration->streamingSessionCount > 0, STATUS_INTERNAL_ERROR, "Should've created streaming session for viewer");
    pSampleStreamingSession = pSampleConfiguration->sampleStreamingSessionList[0];

    MUTEX_UNLOCK(pSampleConfiguration->sampleConfigurationObjLock);
    locked = FALSE;

    switch (pReceivedSignalingMessage->signalingMessage.messageType) {
        case SIGNALING_MESSAGE_TYPE_OFFER:
            DLOGE("Unexpected message SIGNALING_MESSAGE_TYPE_OFFER \n");
            break;
        case SIGNALING_MESSAGE_TYPE_ICE_CANDIDATE:
            CHK_STATUS(handleRemoteCandidate(pSampleStreamingSession, &pReceivedSignalingMessage->signalingMessage));
            break;
        case SIGNALING_MESSAGE_TYPE_ANSWER:
            CHK_STATUS(handleAnswer(pSampleConfiguration, pSampleStreamingSession,
                                    &pReceivedSignalingMessage->signalingMessage));
            break;
        default:
            DLOGW("Unknown message type %u", pReceivedSignalingMessage->signalingMessage.messageType);
    }

CleanUp:

    if (locked) {
        MUTEX_UNLOCK(pSampleConfiguration->sampleConfigurationObjLock);
    }

    // Return success to continue
    return retStatus;
}

STATUS signalingClientStateChanged(UINT64 customData, SIGNALING_CLIENT_STATE state)
{
    UNUSED_PARAM(customData);
    STATUS retStatus = STATUS_SUCCESS;

    DLOGV("Signaling client state changed to %d", state);

    // Return success to continue
    return retStatus;
}

STATUS masterMessageReceived(UINT64 customData, PReceivedSignalingMessage pReceivedSignalingMessage)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleConfiguration pSampleConfiguration = (PSampleConfiguration) customData;
    PSampleStreamingSession pSampleStreamingSession = NULL;
    UINT32 i;

    CHK(pSampleConfiguration != NULL, STATUS_NULL_ARG);

    MUTEX_LOCK(pSampleConfiguration->sampleConfigurationObjLock);

    // ice candidate message and offer message can come at any order. Therefore, if we see a new peerId, then create
    // a new SampleStreamingSession, which in turn creates a new peerConnection
    for (i = 0; i < pSampleConfiguration->streamingSessionCount && pSampleStreamingSession == NULL; ++i) {
        if (0 == STRCMP(pReceivedSignalingMessage->signalingMessage.peerClientId, pSampleConfiguration->sampleStreamingSessionList[i]->peerId)) {
            pSampleStreamingSession = pSampleConfiguration->sampleStreamingSessionList[i];
        }
    }

    if (pSampleStreamingSession == NULL) {
        CHK_WARN(pSampleConfiguration->streamingSessionCount < ARRAY_SIZE(pSampleConfiguration->sampleStreamingSessionList),
                 retStatus, "Dropping signalling message from peer %s since maximum simultaneous streaming session of %u is reached",
                 pReceivedSignalingMessage->signalingMessage.peerClientId, ARRAY_SIZE(pSampleConfiguration->sampleStreamingSessionList));

        DLOGD("Creating new streaming session for peer %s", pReceivedSignalingMessage->signalingMessage.peerClientId);
        CHK_STATUS(createSampleStreamingSession(pSampleConfiguration,
                                                       pReceivedSignalingMessage->signalingMessage.peerClientId,
                                                       TRUE,
                                                       &pSampleStreamingSession));
        pSampleConfiguration->sampleStreamingSessionList[pSampleConfiguration->streamingSessionCount++] = pSampleStreamingSession;
    }

    MUTEX_UNLOCK(pSampleConfiguration->sampleConfigurationObjLock);

    switch (pReceivedSignalingMessage->signalingMessage.messageType) {
        case SIGNALING_MESSAGE_TYPE_OFFER:
            CHK_STATUS(handleOffer(pSampleConfiguration,
                                   pSampleStreamingSession,
                                   &pReceivedSignalingMessage->signalingMessage));
            break;
        case SIGNALING_MESSAGE_TYPE_ICE_CANDIDATE:
            CHK_STATUS(handleRemoteCandidate(pSampleStreamingSession, &pReceivedSignalingMessage->signalingMessage));
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

STATUS handleAnswer(PSampleConfiguration pSampleConfiguration, PSampleStreamingSession pSampleStreamingSession,
                    PSignalingMessage pSignalingMessage)
{
    UNUSED_PARAM(pSampleConfiguration);
    STATUS retStatus = STATUS_SUCCESS;
    RtcSessionDescriptionInit answerSessionDescriptionInit;

    MEMSET(&answerSessionDescriptionInit, 0x00, SIZEOF(RtcSessionDescriptionInit));

    CHK_STATUS(deserializeSessionDescriptionInit(pSignalingMessage->payload, pSignalingMessage->payloadLen, &answerSessionDescriptionInit));
    CHK_STATUS(setRemoteDescription(pSampleStreamingSession->pPeerConnection, &answerSessionDescriptionInit));

CleanUp:

    CHK_LOG_ERR_NV(retStatus);

    return retStatus;
}

STATUS handleOffer(PSampleConfiguration pSampleConfiguration, PSampleStreamingSession pSampleStreamingSession,
                   PSignalingMessage pSignalingMessage)
{
    STATUS retStatus = STATUS_SUCCESS;
    BOOL locked = FALSE;
    RtcSessionDescriptionInit offerSessionDescriptionInit;

    CHK(pSampleConfiguration != NULL && pSignalingMessage != NULL, STATUS_NULL_ARG);

    MEMSET(&offerSessionDescriptionInit, 0x00, SIZEOF(RtcSessionDescriptionInit));
    MEMSET(&pSampleStreamingSession->answerSessionDescriptionInit, 0x00, SIZEOF(RtcSessionDescriptionInit));

    CHK_STATUS(deserializeSessionDescriptionInit(pSignalingMessage->payload, pSignalingMessage->payloadLen, &offerSessionDescriptionInit));
    CHK_STATUS(setRemoteDescription(pSampleStreamingSession->pPeerConnection, &offerSessionDescriptionInit));
    CHK_STATUS(createAnswer(pSampleStreamingSession->pPeerConnection, &pSampleStreamingSession->answerSessionDescriptionInit));
    CHK_STATUS(setLocalDescription(pSampleStreamingSession->pPeerConnection, &pSampleStreamingSession->answerSessionDescriptionInit));

    if (!pSampleConfiguration->trickleIce) {
        while (!ATOMIC_LOAD_BOOL(&pSampleStreamingSession->candidateGatheringDone)) {
            CHK_WARN(!ATOMIC_LOAD_BOOL(&pSampleStreamingSession->terminateFlag), STATUS_INTERNAL_ERROR, "application terminated and candidate gathering still not done");
            CVAR_WAIT(pSampleConfiguration->cvar, pSampleConfiguration->sampleConfigurationObjLock, INFINITE_TIME_VALUE);
        }

        DLOGD("Candidate collection done for non trickle ice");
        // get the latest local description once candidate gathering is done
        CHK_STATUS(peerConnectionGetCurrentLocalDescription(pSampleStreamingSession->pPeerConnection,
                                                            &pSampleStreamingSession->answerSessionDescriptionInit));
    }

    CHK_STATUS(respondWithAnswer(pSampleStreamingSession));

    if (!ATOMIC_LOAD_BOOL(&pSampleConfiguration->mediaThreadStarted)) {
        ATOMIC_STORE_BOOL(&pSampleConfiguration->mediaThreadStarted, TRUE);
        if (pSampleConfiguration->videoSource != NULL) {
            THREAD_CREATE(&pSampleConfiguration->videoSenderTid, pSampleConfiguration->videoSource,
                          (PVOID) pSampleConfiguration);
        }

        if (pSampleConfiguration->audioSource != NULL) {
            THREAD_CREATE(&pSampleConfiguration->audioSenderTid, pSampleConfiguration->audioSource,
                          (PVOID) pSampleConfiguration);
        }
    }

    // The audio video receive routine should be per streaming session
    if (pSampleConfiguration->receiveAudioVideoSource != NULL) {
        THREAD_CREATE(&pSampleStreamingSession->receiveAudioVideoSenderTid, pSampleConfiguration->receiveAudioVideoSource,
                      (PVOID) pSampleStreamingSession);
    }

CleanUp:

    CHK_LOG_ERR_NV(retStatus);

    if (locked) {
        MUTEX_UNLOCK(pSampleConfiguration->sampleConfigurationObjLock);
    }

    return retStatus;
}

STATUS respondWithAnswer(PSampleStreamingSession pSampleStreamingSession)
{
    STATUS retStatus = STATUS_SUCCESS;
    SignalingMessage message;
    UINT32 buffLen = 0;

    CHK_STATUS(serializeSessionDescriptionInit(&pSampleStreamingSession->answerSessionDescriptionInit, NULL, &buffLen));
    CHK_STATUS(serializeSessionDescriptionInit(&pSampleStreamingSession->answerSessionDescriptionInit, message.payload, &buffLen));

    message.version = SIGNALING_MESSAGE_CURRENT_VERSION;
    message.messageType = SIGNALING_MESSAGE_TYPE_ANSWER;
    STRCPY(message.peerClientId, pSampleStreamingSession->peerId);
    message.payloadLen = (UINT32) STRLEN(message.payload);
    message.correlationId[0] = '\0';

    retStatus = signalingClientSendMessageSync(pSampleStreamingSession->pSampleConfiguration->signalingClientHandle, &message);

CleanUp:

    CHK_LOG_ERR_NV(retStatus);
    return retStatus;
}

VOID onIceCandidateHandler(UINT64 customData, PCHAR candidateJson)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleStreamingSession pSampleStreamingSession = (PSampleStreamingSession) customData;
    SignalingMessage message;

    CHK(pSampleStreamingSession != NULL, STATUS_NULL_ARG);

    if (candidateJson == NULL) {
        DLOGD("ice candidate gathering finished");
        ATOMIC_STORE_BOOL(&pSampleStreamingSession->candidateGatheringDone, TRUE);
        CVAR_BROADCAST(pSampleStreamingSession->pSampleConfiguration->cvar);

    } else if (pSampleStreamingSession->pSampleConfiguration->trickleIce &&
               ATOMIC_LOAD_BOOL(&pSampleStreamingSession->peerIdReceived)) {
        message.version = SIGNALING_MESSAGE_CURRENT_VERSION;
        message.messageType = SIGNALING_MESSAGE_TYPE_ICE_CANDIDATE;
        STRCPY(message.peerClientId, pSampleStreamingSession->peerId);
        message.payloadLen = (UINT32) STRLEN(candidateJson);
        STRCPY(message.payload, candidateJson);
        message.correlationId[0] = '\0';
        CHK_STATUS(signalingClientSendMessageSync(pSampleStreamingSession->pSampleConfiguration->signalingClientHandle, &message));
    }

CleanUp:

    CHK_LOG_ERR_NV(retStatus);
}

STATUS initializePeerConnection(PSampleConfiguration pSampleConfiguration, PRtcPeerConnection* ppRtcPeerConnection)
{
    STATUS retStatus = STATUS_SUCCESS;
    RtcConfiguration configuration;
    UINT32 i, j, iceConfigCount, uriCount;
    PIceConfigInfo pIceConfigInfo;

    CHK(pSampleConfiguration != NULL && ppRtcPeerConnection != NULL, STATUS_NULL_ARG);

    MEMSET(&configuration, 0x00, SIZEOF(RtcConfiguration));

    // Set the  STUN server
    SNPRINTF(configuration.iceServers[0].urls, MAX_ICE_CONFIG_URI_LEN, KINESIS_VIDEO_STUN_URL, pSampleConfiguration->channelInfo.pRegion);

    if (pSampleConfiguration->useTurn) {
        // Set the URIs from the configuration
        CHK_STATUS(signalingClientGetIceConfigInfoCount(pSampleConfiguration->signalingClientHandle, &iceConfigCount));

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

    CHK_STATUS(createPeerConnection(&configuration, ppRtcPeerConnection));

CleanUp:

    return retStatus;
}

STATUS createSampleStreamingSession(PSampleConfiguration pSampleConfiguration, PCHAR peerId, BOOL isMaster,
                                           PSampleStreamingSession *ppSampleStreamingSession)
{
    STATUS retStatus = STATUS_SUCCESS;
    RtcMediaStreamTrack videoTrack, audioTrack;
    PSampleStreamingSession pSampleStreamingSession = NULL;

    MEMSET(&videoTrack, 0x00, SIZEOF(RtcMediaStreamTrack));
    MEMSET(&audioTrack, 0x00, SIZEOF(RtcMediaStreamTrack));

    CHK(pSampleConfiguration != NULL && ppSampleStreamingSession != NULL, STATUS_NULL_ARG);
    CHK((isMaster && peerId != NULL) || !isMaster, STATUS_INVALID_ARG);

    pSampleStreamingSession = (PSampleStreamingSession) MEMCALLOC(1, SIZEOF(SampleStreamingSession));
    CHK(pSampleStreamingSession != NULL, STATUS_NOT_ENOUGH_MEMORY);

    if (isMaster) {
        STRCPY(pSampleStreamingSession->peerId, peerId);
    } else {
        STRCPY(pSampleStreamingSession->peerId, SAMPLE_VIEWER_CLIENT_ID);
    }
    ATOMIC_STORE_BOOL(&pSampleStreamingSession->peerIdReceived, TRUE);

    pSampleStreamingSession->pSampleConfiguration = pSampleConfiguration;

    ATOMIC_STORE_BOOL(&pSampleStreamingSession->terminateFlag, FALSE);
    ATOMIC_STORE_BOOL(&pSampleStreamingSession->candidateGatheringDone, FALSE);

    CHK_STATUS(initializePeerConnection(pSampleConfiguration, &pSampleStreamingSession->pPeerConnection));
    CHK_STATUS(peerConnectionOnIceCandidate(pSampleStreamingSession->pPeerConnection,
                                            (UINT64) pSampleStreamingSession,
                                            onIceCandidateHandler));
    CHK_STATUS(peerConnectionOnConnectionStateChange(pSampleStreamingSession->pPeerConnection,
                                                     (UINT64) pSampleStreamingSession,
                                                     onConnectionStateChange));
    if (pSampleConfiguration->onDataChannel != NULL) {
        CHK_STATUS(peerConnectionOnDataChannel(pSampleStreamingSession->pPeerConnection,
                                               (UINT64) pSampleStreamingSession,
                                               pSampleConfiguration->onDataChannel));
    }

    // Declare that we support H264,Profile=42E01F,level-asymmetry-allowed=1,packetization-mode=1 and Opus
    CHK_STATUS(addSupportedCodec(pSampleStreamingSession->pPeerConnection, RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE));
    CHK_STATUS(addSupportedCodec(pSampleStreamingSession->pPeerConnection, RTC_CODEC_OPUS));

    // Add a SendRecv Transceiver of type video
    videoTrack.kind = MEDIA_STREAM_TRACK_KIND_VIDEO;
    videoTrack.codec = RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE;
    STRCPY(videoTrack.streamId, "myKvsVideoStream");
    STRCPY(videoTrack.trackId, "myVideoTrack");
    CHK_STATUS(addTransceiver(pSampleStreamingSession->pPeerConnection,
                              &videoTrack,
                              NULL,
                              &pSampleStreamingSession->pVideoRtcRtpTransceiver));

    // Add a SendRecv Transceiver of type video
    audioTrack.kind = MEDIA_STREAM_TRACK_KIND_AUDIO;
    audioTrack.codec = RTC_CODEC_OPUS;
    STRCPY(audioTrack.streamId, "myKvsVideoStream");
    STRCPY(audioTrack.trackId, "myAudioTrack");
    CHK_STATUS(addTransceiver(pSampleStreamingSession->pPeerConnection,
                              &audioTrack,
                              NULL,
                              &pSampleStreamingSession->pAudioRtcRtpTransceiver));

CleanUp:

    if (STATUS_FAILED(retStatus) && pSampleStreamingSession != NULL) {
        freeSampleStreamingSession(&pSampleStreamingSession);
        pSampleStreamingSession = NULL;
    }

    if (ppSampleStreamingSession != NULL) {
        *ppSampleStreamingSession = pSampleStreamingSession;
    }

    return retStatus;
}

STATUS freeSampleStreamingSession(PSampleStreamingSession* ppSampleStreamingSession)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleStreamingSession pSampleStreamingSession = NULL;

    CHK(ppSampleStreamingSession != NULL, STATUS_NULL_ARG);
    pSampleStreamingSession = *ppSampleStreamingSession;
    CHK(pSampleStreamingSession != NULL, retStatus);

    DLOGD("Freeing streaming session with peer id: %s ", pSampleStreamingSession->peerId);

    ATOMIC_STORE_BOOL(&pSampleStreamingSession->terminateFlag, TRUE);

    if (pSampleStreamingSession->shutdownCallback != NULL) {
        pSampleStreamingSession->shutdownCallback(pSampleStreamingSession->shutdownCallbackCustomData, pSampleStreamingSession);
    }

    if (IS_VALID_TID_VALUE(pSampleStreamingSession->receiveAudioVideoSenderTid)) {
        THREAD_JOIN(pSampleStreamingSession->receiveAudioVideoSenderTid, NULL);
    }

    freePeerConnection(&pSampleStreamingSession->pPeerConnection);
    SAFE_MEMFREE(pSampleStreamingSession);

CleanUp:

    CHK_LOG_ERR_NV(retStatus);

    return retStatus;
}

STATUS streamingSessionOnShutdown(PSampleStreamingSession pSampleStreamingSession, UINT64 customData,
                                  StreamSessionShutdownCallback streamSessionShutdownCallback)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pSampleStreamingSession != NULL && streamSessionShutdownCallback != NULL, STATUS_NULL_ARG);

    pSampleStreamingSession->shutdownCallbackCustomData = customData;
    pSampleStreamingSession->shutdownCallback = streamSessionShutdownCallback;

CleanUp:

    return retStatus;
}

VOID sampleFrameHandler(UINT64 customData, PFrame pFrame)
{
    UNUSED_PARAM(customData);
    DLOGV("Frame received. TrackId: %" PRIu64 ", Size: %u, Flags %u", pFrame->trackId, pFrame->size, pFrame->flags);
}

STATUS handleRemoteCandidate(PSampleStreamingSession pSampleStreamingSession, PSignalingMessage pSignalingMessage)
{
    STATUS retStatus = STATUS_SUCCESS;
    RtcIceCandidateInit iceCandidate;

    CHK_STATUS(deserializeRtcIceCandidateInit(pSignalingMessage->payload, pSignalingMessage->payloadLen, &iceCandidate));
    CHK_STATUS(addIceCandidate(pSampleStreamingSession->pPeerConnection, iceCandidate.candidate));

CleanUp:

    CHK_LOG_ERR_NV(retStatus);
    return retStatus;
}

STATUS createSampleConfiguration(PCHAR channelName, SIGNALING_CHANNEL_ROLE_TYPE roleType, BOOL trickleIce, BOOL useTurn, PSampleConfiguration* ppSampleConfiguration)
{
    STATUS retStatus = STATUS_SUCCESS;
    PCHAR pAccessKey, pSecretKey, pSessionToken;
    PSampleConfiguration pSampleConfiguration = NULL;

    CHK(ppSampleConfiguration != NULL, STATUS_NULL_ARG);

    CHK(NULL != (pSampleConfiguration = (PSampleConfiguration) MEMCALLOC(1, SIZEOF(SampleConfiguration))), STATUS_NOT_ENOUGH_MEMORY);

    CHK_ERR((pAccessKey = getenv(ACCESS_KEY_ENV_VAR)) != NULL, STATUS_INVALID_OPERATION, "AWS_ACCESS_KEY_ID must be set");
    CHK_ERR((pSecretKey = getenv(SECRET_KEY_ENV_VAR)) != NULL, STATUS_INVALID_OPERATION, "AWS_SECRET_ACCESS_KEY must be set");
    pSessionToken = getenv(SESSION_TOKEN_ENV_VAR);

    if ((pSampleConfiguration->channelInfo.pRegion = getenv(DEFAULT_REGION_ENV_VAR)) == NULL) {
        pSampleConfiguration->channelInfo.pRegion = DEFAULT_AWS_REGION;
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
    pSampleConfiguration->signalingClientHandle = INVALID_SIGNALING_CLIENT_HANDLE_VALUE;
    pSampleConfiguration->sampleConfigurationObjLock = MUTEX_CREATE(TRUE);
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
    pSampleConfiguration->channelInfo.messageTtl = 0; // Default is 60 seconds

    ATOMIC_STORE_BOOL(&pSampleConfiguration->interrupted, FALSE);
    ATOMIC_STORE_BOOL(&pSampleConfiguration->mediaThreadStarted, FALSE);
    ATOMIC_STORE_BOOL(&pSampleConfiguration->appTerminateFlag, FALSE);
    ATOMIC_STORE_BOOL(&pSampleConfiguration->updatingSampleStreamingSessionList, FALSE);

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
    UINT32 i;

    CHK(ppSampleConfiguration != NULL, STATUS_NULL_ARG);
    pSampleConfiguration = *ppSampleConfiguration;

    CHK(pSampleConfiguration != NULL, retStatus);

    for (i = 0; i < pSampleConfiguration->streamingSessionCount; ++i) {
        freeSampleStreamingSession(&pSampleConfiguration->sampleStreamingSessionList[i]);
    }

    deinitKvsWebRtc();

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
