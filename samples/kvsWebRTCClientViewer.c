#include "Samples.h"

INT32 main(INT32 argc, CHAR *argv[])
{
    STATUS retStatus = STATUS_SUCCESS;
    RtcSessionDescriptionInit offerSessionDescriptionInit;
    UINT32 buffLen = 0;
    SignalingMessage message;
    PSampleConfiguration pSampleConfiguration = NULL;
    PSampleStreamingSession pSampleStreamingSession = NULL;
    BOOL locked = FALSE;

    // do trickle-ice by default
    printf("[KVS Master] Using trickleICE by default\n");

    retStatus = createSampleConfiguration(argc > 1 ? argv[1] : SAMPLE_CHANNEL_NAME,
                                          SIGNALING_CHANNEL_ROLE_TYPE_VIEWER,
                                          TRUE,
                                          TRUE,
                                          &pSampleConfiguration);
    if(retStatus != STATUS_SUCCESS) {
        printf("[KVS Viewer] createSampleConfiguration(): operation returned status code: 0x%08x \n", retStatus);
        goto CleanUp;
    }
    printf("[KVS Viewer] Created signaling channel %s\n", (argc > 1 ? argv[1] : SAMPLE_CHANNEL_NAME));

    // Initialize KVS WebRTC. This must be done before anything else, and must only be done once.
    retStatus = initKvsWebRtc();
    if(retStatus != STATUS_SUCCESS) {
        printf("[KVS Master] initKvsWebRtc(): operation returned status code: 0x%08x \n", retStatus);
        goto CleanUp;
    }

    printf("[KVS Viewer] KVS WebRTC initialization completed successfully\n");

    pSampleConfiguration->signalingClientCallbacks.messageReceivedFn = viewerMessageReceived;

    strcpy(pSampleConfiguration->clientInfo.clientId, SAMPLE_VIEWER_CLIENT_ID);

    retStatus = createSignalingClientSync(&pSampleConfiguration->clientInfo, &pSampleConfiguration->channelInfo,
                                          &pSampleConfiguration->signalingClientCallbacks, pSampleConfiguration->pCredentialProvider,
                                          &pSampleConfiguration->signalingClientHandle);
    if(retStatus != STATUS_SUCCESS) {
        printf("[KVS Viewer] createSignalingClientSync(): operation returned status code: 0x%08x \n", retStatus);
        goto CleanUp;
    }

    printf("[KVS Viewer] Signaling client created successfully\n");

    // Initialize streaming session
    MUTEX_LOCK(pSampleConfiguration->sampleConfigurationObjLock);
    locked = TRUE;

    retStatus = createSampleStreamingSession(pSampleConfiguration, NULL, FALSE, &pSampleStreamingSession);
    if(retStatus != STATUS_SUCCESS) {
        printf("[KVS Viewer] createSampleStreamingSession(): operation returned status code: 0x%08x \n", retStatus);
        goto CleanUp;
    }

    printf("[KVS Viewer] Creating streaming session...completed\n");
    pSampleConfiguration->sampleStreamingSessionList[pSampleConfiguration->streamingSessionCount++] = pSampleStreamingSession;

    MUTEX_UNLOCK(pSampleConfiguration->sampleConfigurationObjLock);
    locked = FALSE;

    // Enable the processing of the messages
    retStatus = signalingClientConnectSync(pSampleConfiguration->signalingClientHandle);
    if(retStatus != STATUS_SUCCESS) {
        printf("[KVS Viewer] signalingClientConnectSync(): operation returned status code: 0x%08x \n", retStatus);
        goto CleanUp;
    }

    printf("[KVS Viewer] Signaling client connection to socket established\n");

    memset(&offerSessionDescriptionInit, 0x00, SIZEOF(RtcSessionDescriptionInit));

    retStatus = createOffer(pSampleStreamingSession->pPeerConnection, &offerSessionDescriptionInit);
    if(retStatus != STATUS_SUCCESS) {
        printf("[KVS Viewer] createOffer(): operation returned status code: 0x%08x \n", retStatus);
        goto CleanUp;
    }
    printf("[KVS Viewer] Offer creation successful\n");

    retStatus = setLocalDescription(pSampleStreamingSession->pPeerConnection, &offerSessionDescriptionInit);
    if(retStatus != STATUS_SUCCESS) {
        printf("[KVS Viewer] setLocalDescription(): operation returned status code: 0x%08x \n", retStatus);
        goto CleanUp;
    }
    printf("[KVS Viewer] Completed setting local description\n");

    retStatus = transceiverOnFrame(pSampleStreamingSession->pAudioRtcRtpTransceiver,
                                   (UINT64) pSampleStreamingSession,
                                    sampleFrameHandler);
    if(retStatus != STATUS_SUCCESS) {
        printf("[KVS Viewer] transceiverOnFrame(): operation returned status code: 0x%08x \n", retStatus);
        goto CleanUp;
    }

    printf("[KVS Viewer] Generating JSON of session description....");
    retStatus = serializeSessionDescriptionInit(&offerSessionDescriptionInit, NULL, &buffLen);
    if(retStatus != STATUS_SUCCESS) {
        printf("[KVS Viewer] serializeSessionDescriptionInit(): operation returned status code: 0x%08x \n", retStatus);
        goto CleanUp;
    }
    if(buffLen >= SIZEOF(message.payload)) {
        printf("[KVS Viewer] serializeSessionDescriptionInit(): operation returned status code: 0x%08x \n", STATUS_INVALID_OPERATION);
        retStatus = STATUS_INVALID_OPERATION;
        goto CleanUp;
    }

    retStatus = serializeSessionDescriptionInit(&offerSessionDescriptionInit, message.payload, &buffLen);
    if(retStatus != STATUS_SUCCESS) {
        printf("[KVS Viewer] serializeSessionDescriptionInit(): operation returned status code: 0x%08x \n", retStatus);
        goto CleanUp;
    }

    printf("Completed\n");

    message.version = SIGNALING_MESSAGE_CURRENT_VERSION;
    message.messageType = SIGNALING_MESSAGE_TYPE_OFFER;
    STRCPY(message.peerClientId, SAMPLE_MASTER_CLIENT_ID);
    message.payloadLen = (buffLen / SIZEOF(CHAR)) - 1;
    message.correlationId[0] = '\0';

    retStatus = signalingClientSendMessageSync(pSampleConfiguration->signalingClientHandle, &message);
    if(retStatus != STATUS_SUCCESS) {
        printf("[KVS Viewer] signalingClientSendMessageSync(): operation returned status code: 0x%08x \n", retStatus);
        goto CleanUp;
    }

    // Block forever
    THREAD_SLEEP(MAX_UINT64);

CleanUp:

    if (retStatus != STATUS_SUCCESS) {
        printf("[KVS Viewer] Terminated with status code 0x%08x", retStatus);
    }

    printf("[KVS Viewer] Cleaning up....\n");

    if (locked) {
        MUTEX_UNLOCK(pSampleConfiguration->sampleConfigurationObjLock);
    }

    if (pSampleConfiguration != NULL) {
        retStatus = freeSignalingClient(&pSampleConfiguration->signalingClientHandle);
        if(retStatus != STATUS_SUCCESS) {
            printf("[KVS Master] freeSignalingClient(): operation returned status code: 0x%08x \n", retStatus);
        }

        retStatus = freeSampleConfiguration(&pSampleConfiguration);
        if(retStatus != STATUS_SUCCESS) {
            printf("[KVS Master] freeSampleConfiguration(): operation returned status code: 0x%08x \n", retStatus);
        }
    }
    printf("[KVS Viewer] Cleanup done\n");
    return (INT32) retStatus;
}
