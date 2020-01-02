#include "Samples.h"

INT32 main(INT32 argc, CHAR *argv[])
{
    STATUS retStatus = STATUS_SUCCESS;
    SignalingClientCallbacks signalingClientCallbacks;
    SignalingClientInfo clientInfo;
    RtcSessionDescriptionInit offerSessionDescriptionInit;
    UINT32 buffLen = 0;
    SignalingMessage message;
    PSampleConfiguration pSampleConfiguration = NULL;
    PSampleStreamingSession pSampleStreamingSession = NULL;
    BOOL locked = FALSE;

    // do trickle-ice by default
    CHK_STATUS(createSampleConfiguration(argc > 1 ? argv[1] : SAMPLE_CHANNEL_NAME,
            SIGNALING_CHANNEL_ROLE_TYPE_VIEWER,
            TRUE,
            TRUE,
            &pSampleConfiguration));

    // Initalize KVS WebRTC. This must be done before anything else, and must only be done once.
    CHK_STATUS(initKvsWebRtc());

    signalingClientCallbacks.version = SIGNALING_CLIENT_CALLBACKS_CURRENT_VERSION;
    signalingClientCallbacks.customData = (UINT64) pSampleConfiguration;
    signalingClientCallbacks.messageReceivedFn = viewerMessageReceived;
    signalingClientCallbacks.errorReportFn = NULL;
    signalingClientCallbacks.stateChangeFn = signalingClientStateChanged;

    clientInfo.version = SIGNALING_CLIENT_INFO_CURRENT_VERSION;
    clientInfo.loggingLevel = loggerGetLogLevel();
    STRCPY(clientInfo.clientId, SAMPLE_VIEWER_CLIENT_ID);

    CHK_STATUS(createSignalingClientSync(&clientInfo,
            &pSampleConfiguration->channelInfo,
            &signalingClientCallbacks,
            pSampleConfiguration->pCredentialProvider,
            &pSampleConfiguration->signalingClientHandle));

    // Initialize streaming session
    MUTEX_LOCK(pSampleConfiguration->sampleConfigurationObjLock);
    locked = TRUE;

    CHK_STATUS(createSampleStreamingSession(pSampleConfiguration, NULL, FALSE, &pSampleStreamingSession));
    pSampleConfiguration->sampleStreamingSessionList[pSampleConfiguration->streamingSessionCount++] = pSampleStreamingSession;

    MUTEX_UNLOCK(pSampleConfiguration->sampleConfigurationObjLock);
    locked = FALSE;

    // Enable the processing of the messages
    CHK_STATUS(signalingClientConnectSync(pSampleConfiguration->signalingClientHandle));

    MEMSET(&offerSessionDescriptionInit, 0x00, SIZEOF(RtcSessionDescriptionInit));
    CHK_STATUS(createOffer(pSampleStreamingSession->pPeerConnection, &offerSessionDescriptionInit));
    CHK_STATUS(setLocalDescription(pSampleStreamingSession->pPeerConnection, &offerSessionDescriptionInit));
    CHK_STATUS(transceiverOnFrame(pSampleStreamingSession->pAudioRtcRtpTransceiver,
                                  (UINT64) pSampleStreamingSession,
                                  sampleFrameHandler));

    CHK_STATUS(serializeSessionDescriptionInit(&offerSessionDescriptionInit, NULL, &buffLen));
    CHK(buffLen < SIZEOF(message.payload), STATUS_INVALID_OPERATION);
    CHK_STATUS(serializeSessionDescriptionInit(&offerSessionDescriptionInit, message.payload, &buffLen));

    message.version = SIGNALING_MESSAGE_CURRENT_VERSION;
    message.messageType = SIGNALING_MESSAGE_TYPE_OFFER;
    STRCPY(message.peerClientId, SAMPLE_MASTER_CLIENT_ID);
    message.payloadLen = (buffLen / SIZEOF(CHAR)) - 1;
    message.correlationId[0] = '\0';

    CHK_STATUS(signalingClientSendMessageSync(pSampleConfiguration->signalingClientHandle, &message));

    // Block forever
    THREAD_SLEEP(MAX_UINT64);

CleanUp:

    CHK_LOG_ERR_NV(retStatus);

    if (locked) {
        MUTEX_UNLOCK(pSampleConfiguration->sampleConfigurationObjLock);
    }

    if (pSampleConfiguration != NULL) {
        CHK_LOG_ERR_NV(freeSignalingClient(&pSampleConfiguration->signalingClientHandle));
        CHK_LOG_ERR_NV(freeSampleStreamingSession(&pSampleStreamingSession));
        CHK_LOG_ERR_NV(freeSampleConfiguration(&pSampleConfiguration));
    }

    return (INT32) retStatus;
}
