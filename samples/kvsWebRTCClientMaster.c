#include "Samples.h"

static PSampleConfiguration gSampleConfiguration = NULL;
VOID sigintHandler(INT32 sigNum)
{
    UNUSED_PARAM(sigNum);
    if (gSampleConfiguration != NULL) {
        ATOMIC_STORE_BOOL(&gSampleConfiguration->interrupted, TRUE);
        CVAR_BROADCAST(gSampleConfiguration->cvar);
    }
}

INT32 main(INT32 argc, CHAR *argv[])
{
    STATUS retStatus = STATUS_SUCCESS;
    SignalingClientCallbacks signalingClientCallbacks;
    SignalingClientInfo clientInfo;
    UINT32 frameSize;
    PSampleConfiguration pSampleConfiguration = NULL;

    signal(SIGINT, sigintHandler);

    // do tricketIce by default
    CHK_STATUS(createSampleConfiguration(argc > 1 ? argv[1] : SAMPLE_CHANNEL_NAME,
            SIGNALING_CHANNEL_ROLE_TYPE_MASTER,
            TRUE,
            TRUE,
            &pSampleConfiguration));
    // Set the audio and video handlers
    pSampleConfiguration->audioSource = sendAudioPackets;
    pSampleConfiguration->videoSource = sendVideoPackets;
    pSampleConfiguration->onDataChannel = onDataChannel;
    pSampleConfiguration->mediaType = SAMPLE_STREAMING_AUDIO_VIDEO;

    // Check if the samples are present
    CHK_STATUS(readFrameFromDisk(NULL, &frameSize, "./h264SampleFrames/frame-001.h264"));
    CHK_STATUS(readFrameFromDisk(NULL, &frameSize, "./opusSampleFrames/sample-001.opus"));

    // Initialize KVS WebRTC. This must be done before anything else, and must only be done once.
    CHK_STATUS(initKvsWebRtc());

    signalingClientCallbacks.version = SIGNALING_CLIENT_CALLBACKS_CURRENT_VERSION;
    signalingClientCallbacks.messageReceivedFn = masterMessageReceived;
    signalingClientCallbacks.errorReportFn = NULL;
    signalingClientCallbacks.stateChangeFn = signalingClientStateChanged;
    signalingClientCallbacks.customData = (UINT64) pSampleConfiguration;

    clientInfo.version = SIGNALING_CLIENT_INFO_CURRENT_VERSION;
    clientInfo.loggingLevel = LOG_LEVEL_VERBOSE;
    STRCPY(clientInfo.clientId, SAMPLE_MASTER_CLIENT_ID);

    CHK_STATUS(createSignalingClientSync(&clientInfo,
            &pSampleConfiguration->channelInfo,
            &signalingClientCallbacks,
            pSampleConfiguration->pCredentialProvider,
            &pSampleConfiguration->signalingClientHandle));

    // Initialize the peer connection
    CHK_STATUS(initializePeerConnection(pSampleConfiguration));

    // Enable the processing of the messages
    CHK_STATUS(signalingClientConnectSync(pSampleConfiguration->signalingClientHandle));

    gSampleConfiguration = pSampleConfiguration;

    // Checking for termination
    while(!ATOMIC_LOAD_BOOL(&pSampleConfiguration->interrupted)) {
        if (ATOMIC_LOAD_BOOL(&pSampleConfiguration->terminateFlag)) {
            DLOGD("Shutdown flag detected");

            // Join the threads
            if (IS_VALID_TID_VALUE(pSampleConfiguration->replyTid)) {
                THREAD_JOIN(pSampleConfiguration->replyTid, NULL);
            }

            if (IS_VALID_TID_VALUE(pSampleConfiguration->videoSenderTid)) {
                THREAD_JOIN(pSampleConfiguration->videoSenderTid, NULL);
            }

            if (IS_VALID_TID_VALUE(pSampleConfiguration->audioSenderTid)) {
                THREAD_JOIN(pSampleConfiguration->audioSenderTid, NULL);
            }

            CHK_STATUS(freePeerConnection(&pSampleConfiguration->pPeerConnection));
            DLOGD("Completed freeing peerConnection and Reinitializing peerConnection");
            CHK_STATUS(resetSampleConfigurationState(pSampleConfiguration));
            CHK_STATUS(initializePeerConnection(pSampleConfiguration));

        }

        MUTEX_LOCK(pSampleConfiguration->sampleConfigurationObjLock);
        CVAR_WAIT(pSampleConfiguration->cvar, pSampleConfiguration->sampleConfigurationObjLock, INFINITE_TIME_VALUE);
        MUTEX_UNLOCK(pSampleConfiguration->sampleConfigurationObjLock);
    }

CleanUp:

    CHK_LOG_ERR(retStatus);

    if (pSampleConfiguration != NULL) {
        // Kick of the termination sequence
        ATOMIC_STORE_BOOL(&pSampleConfiguration->terminateFlag, TRUE);

        // Join the threads
        if (IS_VALID_TID_VALUE(pSampleConfiguration->replyTid)) {
            THREAD_JOIN(pSampleConfiguration->replyTid, NULL);
        }

        if (IS_VALID_TID_VALUE(pSampleConfiguration->videoSenderTid)) {
            THREAD_JOIN(pSampleConfiguration->videoSenderTid, NULL);
        }

        if (IS_VALID_TID_VALUE(pSampleConfiguration->audioSenderTid)) {
            THREAD_JOIN(pSampleConfiguration->audioSenderTid, NULL);
        }

        CHK_LOG_ERR(freePeerConnection(&pSampleConfiguration->pPeerConnection));
        CHK_LOG_ERR(freeSignalingClient(&pSampleConfiguration->signalingClientHandle));
        CHK_LOG_ERR(freeSampleConfiguration(&pSampleConfiguration));
    }

    return (INT32) retStatus;
}

STATUS readFrameFromDisk(PBYTE pFrame, PUINT32 pSize, PCHAR frameFilePath)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT64 size = 0;

    CHK(pSize != NULL, STATUS_NULL_ARG);
    size = *pSize;

    // Get the size and read into frame
    CHK_STATUS(readFile(frameFilePath, TRUE, pFrame, &size));

CleanUp:

    if (pSize != NULL) {
        *pSize = (UINT32) size;
    }

    return retStatus;
}

PVOID sendVideoPackets(PVOID args)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleConfiguration pSampleConfiguration = (PSampleConfiguration) args;
    Frame frame;
    UINT32 fileIndex = 0, frameSize;
    UINT64 curTime = GETTIME();
    UINT64 startTimeStamp = curTime;
    CHAR filePath[MAX_PATH_LEN + 1];

    CHK(pSampleConfiguration != NULL, STATUS_NULL_ARG);

    while (!ATOMIC_LOAD_BOOL(&pSampleConfiguration->terminateFlag)) {
        fileIndex = fileIndex % NUMBER_OF_H264_FRAME_FILES + 1;
        SNPRINTF(filePath, MAX_PATH_LEN, "./h264SampleFrames/frame-%03d.h264", fileIndex);
        CHK_STATUS(readFrameFromDisk(NULL, &frameSize, filePath));

        // Re-alloc if needed
        if (frameSize > pSampleConfiguration->videoBufferSize) {
            CHK(NULL != (pSampleConfiguration->pVideoFrameBuffer = (PBYTE) MEMREALLOC(pSampleConfiguration->pVideoFrameBuffer, frameSize)), STATUS_NOT_ENOUGH_MEMORY);

            pSampleConfiguration->videoBufferSize = frameSize;
        }

        frame.frameData = pSampleConfiguration->pVideoFrameBuffer;
        frame.size = frameSize;

        CHK_STATUS(readFrameFromDisk(frame.frameData, &frameSize, filePath));

        frame.presentationTs += SAMPLE_VIDEO_FRAME_DURATION;
        CHK_STATUS(writeFrame(pSampleConfiguration->pVideoRtcRtpTransceiver, &frame));

        THREAD_SLEEP(SAMPLE_VIDEO_FRAME_DURATION);
        curTime = GETTIME();
    }

CleanUp:
    CHK_LOG_ERR(retStatus);
    return (PVOID) (ULONG_PTR) retStatus;
}

PVOID sendAudioPackets(PVOID args)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleConfiguration pSampleConfiguration = (PSampleConfiguration) args;
    Frame frame;
    UINT32 fileIndex = 0, frameSize;
    CHAR filePath[MAX_PATH_LEN + 1];

    CHK(pSampleConfiguration != NULL, STATUS_NULL_ARG);

    frame.presentationTs = 0;

    while (!ATOMIC_LOAD_BOOL(&pSampleConfiguration->terminateFlag)) {
        fileIndex = fileIndex % NUMBER_OF_OPUS_FRAME_FILES + 1;
        SNPRINTF(filePath, MAX_PATH_LEN, "./opusSampleFrames/sample-%03d.opus", fileIndex);
        CHK_STATUS(readFrameFromDisk(NULL, &frameSize, filePath));

        // Re-alloc if needed
        if (frameSize > pSampleConfiguration->audioBufferSize) {
            CHK(NULL != (pSampleConfiguration->pAudioFrameBuffer = (PBYTE) MEMREALLOC(pSampleConfiguration->pAudioFrameBuffer, frameSize)), STATUS_NOT_ENOUGH_MEMORY);

            pSampleConfiguration->audioBufferSize = frameSize;
        }

        frame.frameData = pSampleConfiguration->pAudioFrameBuffer;
        frame.size = frameSize;

        CHK_STATUS(readFrameFromDisk(frame.frameData, &frameSize, filePath));

        frame.presentationTs += SAMPLE_AUDIO_FRAME_DURATION;
        CHK_STATUS(writeFrame(pSampleConfiguration->pAudioRtcRtpTransceiver, &frame));

        THREAD_SLEEP(SAMPLE_AUDIO_FRAME_DURATION);
    }

CleanUp:
    CHK_LOG_ERR(retStatus);
    return (PVOID) (ULONG_PTR) retStatus;
}
