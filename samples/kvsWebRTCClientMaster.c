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
    UINT32 frameSize, i;
    PSampleConfiguration pSampleConfiguration = NULL;
    PSampleStreamingSession pSampleStreamingSession = NULL;
    BOOL locked = FALSE;

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
    pSampleConfiguration->receiveAudioVideoSource = sampleReceiveAudioFrame;
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
    clientInfo.loggingLevel = loggerGetLogLevel();
    STRCPY(clientInfo.clientId, SAMPLE_MASTER_CLIENT_ID);

    CHK_STATUS(createSignalingClientSync(&clientInfo,
            &pSampleConfiguration->channelInfo,
            &signalingClientCallbacks,
            pSampleConfiguration->pCredentialProvider,
            &pSampleConfiguration->signalingClientHandle));

    // Enable the processing of the messages
    CHK_STATUS(signalingClientConnectSync(pSampleConfiguration->signalingClientHandle));

    gSampleConfiguration = pSampleConfiguration;

    MUTEX_LOCK(pSampleConfiguration->sampleConfigurationObjLock);
    locked = TRUE;

    // Checking for termination
    while(!ATOMIC_LOAD_BOOL(&pSampleConfiguration->interrupted)) {

        // scan and cleanup terminated streaming session
        for (i = 0; i < pSampleConfiguration->streamingSessionCount; ++i) {
            if (ATOMIC_LOAD_BOOL(&pSampleConfiguration->sampleStreamingSessionList[i]->terminateFlag)) {
                pSampleStreamingSession = pSampleConfiguration->sampleStreamingSessionList[i];

                ATOMIC_STORE_BOOL(&pSampleConfiguration->updatingSampleStreamingSessionList, TRUE);
                while(ATOMIC_LOAD(&pSampleConfiguration->streamingSessionListReadingThreadCount) != 0) {
                    // busy loop until all media thread stopped reading stream session list
                    THREAD_SLEEP(5 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
                }

                // swap with last element and decrement count
                pSampleConfiguration->streamingSessionCount--;
                pSampleConfiguration->sampleStreamingSessionList[i] = pSampleConfiguration->sampleStreamingSessionList[pSampleConfiguration->streamingSessionCount];
                CHK_STATUS(freeSampleStreamingSession(&pSampleStreamingSession));
                ATOMIC_STORE_BOOL(&pSampleConfiguration->updatingSampleStreamingSessionList, FALSE);
            }
        }

        // periodically wake up and clean up terminated streaming session
        CVAR_WAIT(pSampleConfiguration->cvar, pSampleConfiguration->sampleConfigurationObjLock, 5 * HUNDREDS_OF_NANOS_IN_A_SECOND);
    }

CleanUp:

    CHK_LOG_ERR_NV(retStatus);

    if (locked) {
        MUTEX_UNLOCK(pSampleConfiguration->sampleConfigurationObjLock);
    }

    if (pSampleConfiguration != NULL) {
        // Kick of the termination sequence
        ATOMIC_STORE_BOOL(&pSampleConfiguration->appTerminateFlag, TRUE);

        // Join the threads
        if (IS_VALID_TID_VALUE(pSampleConfiguration->videoSenderTid)) {
            THREAD_JOIN(pSampleConfiguration->videoSenderTid, NULL);
        }

        if (IS_VALID_TID_VALUE(pSampleConfiguration->audioSenderTid)) {
            THREAD_JOIN(pSampleConfiguration->audioSenderTid, NULL);
        }

        CHK_LOG_ERR_NV(freeSignalingClient(&pSampleConfiguration->signalingClientHandle));
        CHK_LOG_ERR_NV(freeSampleConfiguration(&pSampleConfiguration));
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
    CHAR filePath[MAX_PATH_LEN + 1];
    STATUS status;
    UINT32 i;

    CHK(pSampleConfiguration != NULL, STATUS_NULL_ARG);

    frame.presentationTs = 0;

    while (!ATOMIC_LOAD_BOOL(&pSampleConfiguration->appTerminateFlag)) {
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

        if (!ATOMIC_LOAD_BOOL(&pSampleConfiguration->updatingSampleStreamingSessionList)) {
            ATOMIC_INCREMENT(&pSampleConfiguration->streamingSessionListReadingThreadCount);
            for (i = 0; i < pSampleConfiguration->streamingSessionCount; ++i) {
                status = writeFrame(pSampleConfiguration->sampleStreamingSessionList[i]->pVideoRtcRtpTransceiver, &frame);
                if (STATUS_FAILED(status)) {
                    DLOGD("writeFrame failed with 0x%08x", status);
                }
            }
            ATOMIC_DECREMENT(&pSampleConfiguration->streamingSessionListReadingThreadCount);
        }

        THREAD_SLEEP(SAMPLE_VIDEO_FRAME_DURATION);
    }

CleanUp:

    CHK_LOG_ERR_NV(retStatus);
    return (PVOID) (ULONG_PTR) retStatus;
}

PVOID sendAudioPackets(PVOID args)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleConfiguration pSampleConfiguration = (PSampleConfiguration) args;
    Frame frame;
    UINT32 fileIndex = 0, frameSize;
    CHAR filePath[MAX_PATH_LEN + 1];
    BOOL locked = FALSE;
    UINT32 i;
    STATUS status;

    CHK(pSampleConfiguration != NULL, STATUS_NULL_ARG);

    frame.presentationTs = 0;

    while (!ATOMIC_LOAD_BOOL(&pSampleConfiguration->appTerminateFlag)) {
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

        if (!ATOMIC_LOAD_BOOL(&pSampleConfiguration->updatingSampleStreamingSessionList)) {
            ATOMIC_INCREMENT(&pSampleConfiguration->streamingSessionListReadingThreadCount);
            for (i = 0; i < pSampleConfiguration->streamingSessionCount; ++i) {
                status = writeFrame(pSampleConfiguration->sampleStreamingSessionList[i]->pAudioRtcRtpTransceiver, &frame);
                if (STATUS_FAILED(status)) {
                    DLOGD("writeFrame failed with 0x%08x", status);
                }
            }
            ATOMIC_DECREMENT(&pSampleConfiguration->streamingSessionListReadingThreadCount);
        }

        THREAD_SLEEP(SAMPLE_AUDIO_FRAME_DURATION);
    }

CleanUp:

    if (locked) {
        MUTEX_UNLOCK(pSampleConfiguration->sampleConfigurationObjLock);
    }

    CHK_LOG_ERR_NV(retStatus);
    return (PVOID) (ULONG_PTR) retStatus;
}

PVOID sampleReceiveAudioFrame(PVOID args)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleStreamingSession pSampleStreamingSession = (PSampleStreamingSession) args;
    CHK(pSampleStreamingSession != NULL, STATUS_NULL_ARG);

    CHK_STATUS(transceiverOnFrame(pSampleStreamingSession->pAudioRtcRtpTransceiver,
                       (UINT64) pSampleStreamingSession,
                       sampleFrameHandler));
CleanUp:

    CHK_LOG_ERR_NV(retStatus);
    return (PVOID) (ULONG_PTR) retStatus;
}
