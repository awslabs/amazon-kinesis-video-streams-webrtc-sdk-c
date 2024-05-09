#include "Samples.h"

extern PSampleConfiguration gSampleConfiguration;

#ifdef ENABLE_DATA_CHANNEL

// onMessage callback for a message received by the viewer on a data channel
VOID dataChannelOnMessageCallback(UINT64 customData, PRtcDataChannel pDataChannel, BOOL isBinary, PBYTE pMessage, UINT32 pMessageLen)
{
    UNUSED_PARAM(customData);
    UNUSED_PARAM(pDataChannel);
    if (isBinary) {
        DLOGI("DataChannel Binary Message");
    } else {
        DLOGI("DataChannel String Message: %.*s", pMessageLen, pMessage);
    }
}

// onOpen callback for the onOpen event of a viewer created data channel
VOID dataChannelOnOpenCallback(UINT64 customData, PRtcDataChannel pDataChannel)
{
    STATUS retStatus = STATUS_SUCCESS;
    DLOGI("New DataChannel has been opened %s ", pDataChannel->name);
    dataChannelOnMessage(pDataChannel, customData, dataChannelOnMessageCallback);
    ATOMIC_INCREMENT((PSIZE_T) customData);
    // Sending first message to the master over the data channel
    retStatus = dataChannelSend(pDataChannel, FALSE, (PBYTE) VIEWER_DATA_CHANNEL_MESSAGE, STRLEN(VIEWER_DATA_CHANNEL_MESSAGE));
    if (retStatus != STATUS_SUCCESS) {
        DLOGI("[KVS Gstreamer Viewer] dataChannelSend(): operation returned status code: 0x%08x ", retStatus);
    }
}
#endif

INT32 main(INT32 argc, CHAR* argv[])
{
    STATUS retStatus = STATUS_SUCCESS;
    RtcSessionDescriptionInit offerSessionDescriptionInit;
    UINT32 buffLen = 0;
    SignalingMessage message;
    PSampleConfiguration pSampleConfiguration = NULL;
    PSampleStreamingSession pSampleStreamingSession = NULL;
    RTC_CODEC audioCodec = RTC_CODEC_OPUS;
    RTC_CODEC videoCodec = RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE;
    BOOL locked = FALSE;
    PCHAR pChannelName;
    CHAR clientId[256];

    SET_INSTRUMENTED_ALLOCATORS();
    UINT32 logLevel = setLogLevel();

#ifndef _WIN32
    signal(SIGINT, sigintHandler);
#endif

#ifdef IOT_CORE_ENABLE_CREDENTIALS
    CHK_ERR((pChannelName = argc > 1 ? argv[1] : GETENV(IOT_CORE_THING_NAME)) != NULL, STATUS_INVALID_OPERATION,
            "AWS_IOT_CORE_THING_NAME must be set");
#else
    pChannelName = argc > 1 ? argv[1] : SAMPLE_CHANNEL_NAME;
#endif

    if (argc > 3) {
        if (!STRCMP(argv[3], AUDIO_CODEC_NAME_OPUS)) {
            audioCodec = RTC_CODEC_OPUS;
        } else {
            DLOGI("[KVS Gstreamer Viewer] Defaulting to Opus audio codec");
        }
    }

    if (argc > 4) {
        if (!STRCMP(argv[4], VIDEO_CODEC_NAME_H265)) {
            videoCodec = RTC_CODEC_H265;
        } else {
            DLOGI("[KVS Gstreamer Viewer] Defaulting to H264 video codec");
        }
    }

    CHK_STATUS(createSampleConfiguration(pChannelName, SIGNALING_CHANNEL_ROLE_TYPE_VIEWER, TRUE, TRUE, logLevel, &pSampleConfiguration));
    pSampleConfiguration->mediaType = SAMPLE_STREAMING_AUDIO_VIDEO;
    pSampleConfiguration->receiveAudioVideoSource = receiveGstreamerAudioVideo;
    pSampleConfiguration->audioCodec = audioCodec;
    pSampleConfiguration->videoCodec = videoCodec;

    if (argc > 2) {
        if (STRCMP(argv[2], "video-only") == 0) {
            pSampleConfiguration->mediaType = SAMPLE_STREAMING_VIDEO_ONLY;
            DLOGI("[KVS Gstreamer Viewer] Streaming video only");
        } else if (STRCMP(argv[2], "audio-video") == 0) {
            pSampleConfiguration->mediaType = SAMPLE_STREAMING_AUDIO_VIDEO;
            DLOGI("[KVS Gstreamer Viewer] Streaming audio and video");
        } else {
            DLOGI("[KVS Gstreamer Viewer] Unrecognized streaming type. Default to video-only");
        }
    } else {
        DLOGI("[KVS Gstreamer Viewer] Streaming video only");
    }

    // Initialize KVS WebRTC. This must be done before anything else, and must only be done once.
    CHK_STATUS(initKvsWebRtc());
    DLOGI("[KVS Gstreamer Viewer] KVS WebRTC initialization completed successfully");

#ifdef ENABLE_DATA_CHANNEL
    pSampleConfiguration->onDataChannel = onDataChannel;
#endif

    SPRINTF(clientId, "%s_%u", SAMPLE_VIEWER_CLIENT_ID, RAND() % MAX_UINT32);
    CHK_STATUS(initSignaling(pSampleConfiguration, clientId));
    DLOGI("[KVS Gstreamer Viewer] Signaling client connection established");

    // Initialize streaming session
    MUTEX_LOCK(pSampleConfiguration->sampleConfigurationObjLock);
    locked = TRUE;
    CHK_STATUS(createSampleStreamingSession(pSampleConfiguration, NULL, FALSE, &pSampleStreamingSession));
    DLOGI("[KVS Gstreamer Viewer] Creating streaming session...completed");
    pSampleConfiguration->sampleStreamingSessionList[pSampleConfiguration->streamingSessionCount++] = pSampleStreamingSession;

    MUTEX_UNLOCK(pSampleConfiguration->sampleConfigurationObjLock);
    locked = FALSE;

    MEMSET(&offerSessionDescriptionInit, 0x00, SIZEOF(RtcSessionDescriptionInit));

    offerSessionDescriptionInit.useTrickleIce = pSampleStreamingSession->remoteCanTrickleIce;
    CHK_STATUS(setLocalDescription(pSampleStreamingSession->pPeerConnection, &offerSessionDescriptionInit));
    DLOGI("[KVS Gstreamer Viewer] Completed setting local description");

    if (!pSampleConfiguration->trickleIce) {
        DLOGI("[KVS Gstreamer Viewer] Non trickle ice. Wait for Candidate collection to complete");
        MUTEX_LOCK(pSampleConfiguration->sampleConfigurationObjLock);
        locked = TRUE;

        while (!ATOMIC_LOAD_BOOL(&pSampleStreamingSession->candidateGatheringDone)) {
            CHK_WARN(!ATOMIC_LOAD_BOOL(&pSampleStreamingSession->terminateFlag), STATUS_OPERATION_TIMED_OUT,
                     "application terminated and candidate gathering still not done");
            CVAR_WAIT(pSampleConfiguration->cvar, pSampleConfiguration->sampleConfigurationObjLock, 5 * HUNDREDS_OF_NANOS_IN_A_SECOND);
        }

        MUTEX_UNLOCK(pSampleConfiguration->sampleConfigurationObjLock);
        locked = FALSE;

        DLOGI("[KVS Gstreamer Viewer] Candidate collection completed");
    }

    CHK_STATUS(createOffer(pSampleStreamingSession->pPeerConnection, &offerSessionDescriptionInit));
    DLOGI("[KVS Gstreamer Viewer] Offer creation successful");

    DLOGI("[KVS Gstreamer Viewer] Generating JSON of session description....");
    CHK_STATUS(serializeSessionDescriptionInit(&offerSessionDescriptionInit, NULL, &buffLen));

    if (buffLen >= SIZEOF(message.payload)) {
        DLOGE("[KVS Gstreamer Viewer] serializeSessionDescriptionInit(): operation returned status code: 0x%08x ", STATUS_INVALID_OPERATION);
        retStatus = STATUS_INVALID_OPERATION;
        goto CleanUp;
    }

    CHK_STATUS(serializeSessionDescriptionInit(&offerSessionDescriptionInit, message.payload, &buffLen));

    message.version = SIGNALING_MESSAGE_CURRENT_VERSION;
    message.messageType = SIGNALING_MESSAGE_TYPE_OFFER;
    STRCPY(message.peerClientId, SAMPLE_MASTER_CLIENT_ID);
    message.payloadLen = (buffLen / SIZEOF(CHAR)) - 1;
    message.correlationId[0] = '\0';

    CHK_STATUS(signalingClientSendMessageSync(pSampleConfiguration->signalingClientHandle, &message));
#ifdef ENABLE_DATA_CHANNEL
    PRtcDataChannel pDataChannel = NULL;
    PRtcPeerConnection pPeerConnection = pSampleStreamingSession->pPeerConnection;
    SIZE_T datachannelLocalOpenCount = 0;

    // Creating a new datachannel on the peer connection of the existing sample streaming session
    CHK_STATUS(createDataChannel(pPeerConnection, pChannelName, NULL, &pDataChannel));
    DLOGI("[KVS Gstreamer Viewer] Creating data channel...completed");

    // Setting a callback for when the data channel is open
    CHK_STATUS(dataChannelOnOpen(pDataChannel, (UINT64) &datachannelLocalOpenCount, dataChannelOnOpenCallback));
    DLOGI("[KVS Gstreamer Viewer] Data Channel open now...");
#endif

    // Block until interrupted
    while (!ATOMIC_LOAD_BOOL(&pSampleConfiguration->interrupted) && !ATOMIC_LOAD_BOOL(&pSampleStreamingSession->terminateFlag)) {
        THREAD_SLEEP(HUNDREDS_OF_NANOS_IN_A_SECOND);
    }

CleanUp:

    if (retStatus != STATUS_SUCCESS) {
        DLOGE("[KVS Gstreamer Viewer] Terminated with status code 0x%08x", retStatus);
    }

    DLOGI("[KVS Gstreamer Viewer] Cleaning up....");

    if (locked) {
        MUTEX_UNLOCK(pSampleConfiguration->sampleConfigurationObjLock);
    }

    if (pSampleConfiguration->enableFileLogging) {
        freeFileLogger();
    }
    if (pSampleConfiguration != NULL) {
        retStatus = freeSignalingClient(&pSampleConfiguration->signalingClientHandle);
        if (retStatus != STATUS_SUCCESS) {
            DLOGE("[KVS Gstreamer Viewer] freeSignalingClient(): operation returned status code: 0x%08x ", retStatus);
        }

        retStatus = freeSampleConfiguration(&pSampleConfiguration);
        if (retStatus != STATUS_SUCCESS) {
            DLOGE("[KVS Gstreamer Viewer] freeSampleConfiguration(): operation returned status code: 0x%08x ", retStatus);
        }
    }
    DLOGI("[KVS Gstreamer Viewer] Cleanup done");

    RESET_INSTRUMENTED_ALLOCATORS();

    // https://www.gnu.org/software/libc/manual/html_node/Exit-Status.html
    // We can only return with 0 - 127. Some platforms treat exit code >= 128
    // to be a success code, which might give an unintended behaviour.
    // Some platforms also treat 1 or 0 differently, so it's better to use
    // EXIT_FAILURE and EXIT_SUCCESS macros for portability.
    return STATUS_FAILED(retStatus) ? EXIT_FAILURE : EXIT_SUCCESS;
}
