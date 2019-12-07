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
    clientInfo.loggingLevel = LOG_LEVEL_VERBOSE;
    STRCPY(clientInfo.clientId, SAMPLE_VIEWER_CLIENT_ID);

    CHK_STATUS(createSignalingClientSync(&clientInfo,
            &pSampleConfiguration->channelInfo,
            &signalingClientCallbacks,
            pSampleConfiguration->pCredentialProvider,
            &pSampleConfiguration->signalingClientHandle));

    // Initialize the peer connection
    CHK_STATUS(initializePeerConnection(pSampleConfiguration));

    // Enable the processing of the messages
    CHK_STATUS(signalingClientConnectSync(pSampleConfiguration->signalingClientHandle));

    MEMSET(&offerSessionDescriptionInit, 0x00, SIZEOF(RtcSessionDescriptionInit));
    CHK_STATUS(createOffer(pSampleConfiguration->pPeerConnection, &offerSessionDescriptionInit));
    CHK_STATUS(setLocalDescription(pSampleConfiguration->pPeerConnection, &offerSessionDescriptionInit));

    CHK_STATUS(serializeSessionDescriptionInit(&offerSessionDescriptionInit, NULL, &buffLen));
    CHK(buffLen < SIZEOF(message.payload), STATUS_INVALID_OPERATION);
    CHK_STATUS(serializeSessionDescriptionInit(&offerSessionDescriptionInit, message.payload, &buffLen));

    message.version = SIGNALING_MESSAGE_CURRENT_VERSION;
    message.messageType = SIGNALING_MESSAGE_TYPE_OFFER;
    STRCPY(message.peerClientId, SAMPLE_MASTER_CLIENT_ID);
    message.payloadLen = (buffLen / SIZEOF(CHAR)) - 1;

    CHK_STATUS(signalingClientSendMessageSync(pSampleConfiguration->signalingClientHandle, &message));

    // Block forever
    THREAD_SLEEP(MAX_UINT64);

CleanUp:

    CHK_LOG_ERR_NV(retStatus);

    if (pSampleConfiguration != NULL) {
        CHK_LOG_ERR_NV(freePeerConnection(&pSampleConfiguration->pPeerConnection));
        CHK_LOG_ERR_NV(freeSignalingClient(&pSampleConfiguration->signalingClientHandle));
        CHK_LOG_ERR_NV(freeSampleConfiguration(&pSampleConfiguration));
    }

    return (INT32) retStatus;
}
