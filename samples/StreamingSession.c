#include "Samples.h"

// ----Callbacks----- //

BOOL sampleFilterNetworkInterfaces(UINT64 customData, PCHAR networkInt)
{
    UNUSED_PARAM(customData);
    BOOL useInterface = FALSE;
    if (STRNCMP(networkInt, (PCHAR) "eth0", ARRAY_SIZE("eth0")) == 0) {
        useInterface = TRUE;
    }
    DLOGD("%s %s", networkInt, (useInterface) ? ("allowed. Candidates to be gathered") : ("blocked. Candidates will not be gathered"));
    return useInterface;
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

        // if application is master and non-trickle ice, send answer now.
        if (pSampleStreamingSession->pSampleConfiguration->channelInfo.channelRoleType == SIGNALING_CHANNEL_ROLE_TYPE_MASTER &&
            !pSampleStreamingSession->remoteCanTrickleIce) {
            CHK_STATUS(createAnswer(pSampleStreamingSession->pPeerConnection, &pSampleStreamingSession->answerSessionDescriptionInit));
            CHK_STATUS(respondWithAnswer(pSampleStreamingSession));
        } else if (pSampleStreamingSession->pSampleConfiguration->channelInfo.channelRoleType == SIGNALING_CHANNEL_ROLE_TYPE_VIEWER &&
                   !pSampleStreamingSession->pSampleConfiguration->appConfigCtx.trickleIce) {
            CVAR_BROADCAST(pSampleStreamingSession->pSampleConfiguration->cvar);
        }

    } else if (pSampleStreamingSession->remoteCanTrickleIce && ATOMIC_LOAD_BOOL(&pSampleStreamingSession->peerIdReceived)) {
        message.version = SIGNALING_MESSAGE_CURRENT_VERSION;
        message.messageType = SIGNALING_MESSAGE_TYPE_ICE_CANDIDATE;
        STRNCPY(message.peerClientId, pSampleStreamingSession->peerId, MAX_SIGNALING_CLIENT_ID_LEN);
        message.payloadLen = (UINT32) STRNLEN(candidateJson, MAX_SIGNALING_MESSAGE_LEN);
        STRNCPY(message.payload, candidateJson, message.payloadLen);
        message.correlationId[0] = '\0';
        CHK_STATUS(sendSignalingMessage(pSampleStreamingSession, &message));
    }

CleanUp:

    CHK_LOG_ERR(retStatus);
}

VOID onConnectionStateChange(UINT64 customData, RTC_PEER_CONNECTION_STATE newState)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleStreamingSession pSampleStreamingSession = (PSampleStreamingSession) customData;
    CHK(pSampleStreamingSession != NULL && pSampleStreamingSession->pSampleConfiguration != NULL, STATUS_INTERNAL_ERROR);

    PSampleConfiguration pSampleConfiguration = pSampleStreamingSession->pSampleConfiguration;
    DLOGI("New connection state %u", newState);

    switch (newState) {
        case RTC_PEER_CONNECTION_STATE_CONNECTED:
            ATOMIC_STORE_BOOL(&pSampleConfiguration->connected, TRUE);
            CVAR_BROADCAST(pSampleConfiguration->cvar);
            if (STATUS_FAILED(retStatus = logSelectedIceCandidatesInformation(pSampleStreamingSession))) {
                DLOGW("Failed to get information about selected Ice candidates: 0x%08x", retStatus);
            }
            break;
        case RTC_PEER_CONNECTION_STATE_FAILED:
            // explicit fallthrough
        case RTC_PEER_CONNECTION_STATE_CLOSED:
            // explicit fallthrough
        case RTC_PEER_CONNECTION_STATE_DISCONNECTED:
            DLOGD("p2p connection disconnected");
            ATOMIC_STORE_BOOL(&pSampleStreamingSession->terminateFlag, TRUE);
            CVAR_BROADCAST(pSampleConfiguration->cvar);
            // explicit fallthrough
        default:
            ATOMIC_STORE_BOOL(&pSampleConfiguration->connected, FALSE);
            CVAR_BROADCAST(pSampleConfiguration->cvar);

            break;
    }

CleanUp:

    CHK_LOG_ERR(retStatus);
}

// ----------//
STATUS createStreamingSession(PSampleConfiguration pSampleConfiguration, PCHAR peerId, BOOL isMaster,
                              PSampleStreamingSession* ppSampleStreamingSession)
{
    STATUS retStatus = STATUS_SUCCESS;
    RtcMediaStreamTrack videoTrack, audioTrack;
    PSampleStreamingSession pSampleStreamingSession = NULL;
    RtcRtpTransceiverInit audioRtpTransceiverInit;
    RtcRtpTransceiverInit videoRtpTransceiverInit;

    MEMSET(&videoTrack, 0x00, SIZEOF(RtcMediaStreamTrack));
    MEMSET(&audioTrack, 0x00, SIZEOF(RtcMediaStreamTrack));

    CHK(pSampleConfiguration != NULL && ppSampleStreamingSession != NULL, STATUS_NULL_ARG);
    CHK((isMaster && peerId != NULL) || !isMaster, STATUS_INVALID_ARG);

    pSampleStreamingSession = (PSampleStreamingSession) MEMCALLOC(1, SIZEOF(SampleStreamingSession));
    pSampleStreamingSession->firstFrame = TRUE;
    pSampleStreamingSession->offerReceiveTime = GETTIME();
    CHK(pSampleStreamingSession != NULL, STATUS_NOT_ENOUGH_MEMORY);

    if (isMaster) {
        STRCPY(pSampleStreamingSession->peerId, peerId);
    } else {
        STRCPY(pSampleStreamingSession->peerId, SAMPLE_VIEWER_CLIENT_ID);
    }
    ATOMIC_STORE_BOOL(&pSampleStreamingSession->peerIdReceived, TRUE);

    pSampleStreamingSession->pAudioRtcRtpTransceiver = NULL;
    pSampleStreamingSession->pVideoRtcRtpTransceiver = NULL;

    pSampleStreamingSession->pSampleConfiguration = pSampleConfiguration;
    pSampleStreamingSession->rtcMetricsHistory.prevTs = GETTIME();

    // if we're the viewer, we control the trickle ice mode
    pSampleStreamingSession->remoteCanTrickleIce = !isMaster && pSampleConfiguration->appConfigCtx.trickleIce;

    ATOMIC_STORE_BOOL(&pSampleStreamingSession->terminateFlag, FALSE);
    ATOMIC_STORE_BOOL(&pSampleStreamingSession->candidateGatheringDone, FALSE);

    pSampleStreamingSession->peerConnectionMetrics.peerConnectionStats.peerConnectionStartTime = GETTIME() / HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
    CHK_STATUS(initializePeerConnection(pSampleConfiguration, &pSampleStreamingSession->pPeerConnection));
    CHK_STATUS(peerConnectionOnIceCandidate(pSampleStreamingSession->pPeerConnection, (UINT64) pSampleStreamingSession, onIceCandidateHandler));
    CHK_STATUS(
        peerConnectionOnConnectionStateChange(pSampleStreamingSession->pPeerConnection, (UINT64) pSampleStreamingSession, onConnectionStateChange));
#ifdef ENABLE_DATA_CHANNEL
    if (pSampleConfiguration->onDataChannel != NULL) {
        CHK_STATUS(peerConnectionOnDataChannel(pSampleStreamingSession->pPeerConnection, (UINT64) pSampleStreamingSession,
                                               pSampleConfiguration->onDataChannel));
    }
#endif

    // Declare that we support H264,Profile=42E01F,level-asymmetry-allowed=1,packetization-mode=1 and Opus
    CHK_STATUS(addSupportedCodec(pSampleStreamingSession->pPeerConnection, RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE));
    CHK_STATUS(addSupportedCodec(pSampleStreamingSession->pPeerConnection, RTC_CODEC_OPUS));

    // Add a SendRecv Transceiver of type video
    videoTrack.kind = MEDIA_STREAM_TRACK_KIND_VIDEO;
    videoTrack.codec = RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE;
    videoRtpTransceiverInit.direction = RTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV;
    videoRtpTransceiverInit.rollingBufferDurationSec = 3;
    // Considering 4 Mbps for 720p (which is what our samples use). This is for H.264.
    // The value could be different for other codecs.
    videoRtpTransceiverInit.rollingBufferBitratebps = 4 * 1024 * 1024;
    STRCPY(videoTrack.streamId, "myKvsVideoStream");
    STRCPY(videoTrack.trackId, "myVideoTrack");
    CHK_STATUS(addTransceiver(pSampleStreamingSession->pPeerConnection, &videoTrack, &videoRtpTransceiverInit,
                              &pSampleStreamingSession->pVideoRtcRtpTransceiver));

    CHK_STATUS(transceiverOnBandwidthEstimation(pSampleStreamingSession->pVideoRtcRtpTransceiver, (UINT64) pSampleStreamingSession,
                                                sampleBandwidthEstimationHandler));

    // Add a SendRecv Transceiver of type audio
    audioTrack.kind = MEDIA_STREAM_TRACK_KIND_AUDIO;
    audioTrack.codec = RTC_CODEC_OPUS;
    audioRtpTransceiverInit.direction = RTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV;
    audioRtpTransceiverInit.rollingBufferDurationSec = 3;
    // For opus, the bitrate could be between 6 Kbps to 510 Kbps
    audioRtpTransceiverInit.rollingBufferBitratebps = 510 * 1024;
    STRCPY(audioTrack.streamId, "myKvsVideoStream");
    STRCPY(audioTrack.trackId, "myAudioTrack");
    CHK_STATUS(addTransceiver(pSampleStreamingSession->pPeerConnection, &audioTrack, &audioRtpTransceiverInit,
                              &pSampleStreamingSession->pAudioRtcRtpTransceiver));

    CHK_STATUS(transceiverOnBandwidthEstimation(pSampleStreamingSession->pAudioRtcRtpTransceiver, (UINT64) pSampleStreamingSession,
                                                sampleBandwidthEstimationHandler));
    // twcc bandwidth estimation
    CHK_STATUS(peerConnectionOnSenderBandwidthEstimation(pSampleStreamingSession->pPeerConnection, (UINT64) pSampleStreamingSession,
                                                         sampleSenderBandwidthEstimationHandler));
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

STATUS freeSampleStreamingSession(PSampleStreamingSession* ppSampleStreamingSession)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleStreamingSession pSampleStreamingSession = NULL;
    PSampleConfiguration pSampleConfiguration;

    CHK(ppSampleStreamingSession != NULL, STATUS_NULL_ARG);
    pSampleStreamingSession = *ppSampleStreamingSession;
    CHK(pSampleStreamingSession != NULL && pSampleStreamingSession->pSampleConfiguration != NULL, retStatus);
    pSampleConfiguration = pSampleStreamingSession->pSampleConfiguration;

    DLOGD("Freeing streaming session with peer id: %s ", pSampleStreamingSession->peerId);

    ATOMIC_STORE_BOOL(&pSampleStreamingSession->terminateFlag, TRUE);

    if (pSampleStreamingSession->shutdownCallback != NULL) {
        pSampleStreamingSession->shutdownCallback(pSampleStreamingSession->shutdownCallbackCustomData, pSampleStreamingSession);
    }

    if (IS_VALID_TID_VALUE(pSampleStreamingSession->receiveAudioVideoSenderTid)) {
        THREAD_JOIN(pSampleStreamingSession->receiveAudioVideoSenderTid, NULL);
    }

    CHK_LOG_ERR(closePeerConnection(pSampleStreamingSession->pPeerConnection));
    CHK_LOG_ERR(freePeerConnection(&pSampleStreamingSession->pPeerConnection));
    SAFE_MEMFREE(pSampleStreamingSession);

CleanUp:

    CHK_LOG_ERR(retStatus);

    return retStatus;
}