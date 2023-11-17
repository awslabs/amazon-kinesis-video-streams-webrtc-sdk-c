#include "Samples.h"
#include "../Include.h"


extern PSampleConfiguration gSampleConfiguration;

VOID shceduleShutdown(UINT64 duration, PSampleConfiguration pSampleConfiguration)
{
    THREAD_SLEEP(duration);
    DLOGD("Terminating canary due to duration reached");
    ATOMIC_STORE_BOOL(&pSampleConfiguration->interrupted, TRUE);
}

INT32 main(INT32 argc, CHAR* argv[])
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 frameSize;
    PSampleConfiguration pSampleConfiguration = NULL;
    PCHAR pChannelName;
    SignalingClientMetrics signalingClientMetrics;
    signalingClientMetrics.version = SIGNALING_CLIENT_METRICS_CURRENT_VERSION;

    auto canaryConfig = Canary::Config();
    Aws::SDKOptions options;
    UINT64 t1;
    BOOL initialized = FALSE;

    SET_INSTRUMENTED_ALLOCATORS();
    UINT32 logLevel = setLogLevel();


    t1 = GETTIME() / HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
    std::string filePath = "../toConsumer.txt";
    remove(filePath.c_str());

    std::thread canaryDurationThread;

    Aws::InitAPI(options);
    CHK_STATUS(canaryConfig.init(argc, argv));
    canaryConfig.isStorage = true;	
    CHK_STATUS(Canary::Cloudwatch::init(&canaryConfig));
    canaryConfig.print();
    SET_LOGGER_LOG_LEVEL(canaryConfig.logLevel.value);
    DLOGD("Canary init time: %d [ms]", (GETTIME() / HUNDREDS_OF_NANOS_IN_A_MILLISECOND) - t1);

#ifndef _WIN32
    signal(SIGINT, sigintHandler);
#endif

#ifdef IOT_CORE_ENABLE_CREDENTIALS
    CHK_ERR((pChannelName = getenv(IOT_CORE_THING_NAME)) != NULL, STATUS_INVALID_OPERATION, "AWS_IOT_CORE_THING_NAME must be set");
#else
    pChannelName = (PCHAR) canaryConfig.channelName.value.c_str();
#endif

    CHK_STATUS(createSampleConfiguration(pChannelName, SIGNALING_CHANNEL_ROLE_TYPE_MASTER, TRUE, TRUE, logLevel, &pSampleConfiguration));

    // TODO: can remove this log once testing is complete
    DLOGD("pSampleConfiguration address after creation: %d", pSampleConfiguration);

    // Set the audio and video handlers
    pSampleConfiguration->audioSource = sendAudioPackets;
    pSampleConfiguration->videoSource = sendVideoPackets;
    pSampleConfiguration->receiveAudioVideoSource = sampleReceiveAudioVideoFrame;

    // Force sample to use storage mode.
    pSampleConfiguration->channelInfo.useMediaStorage = TRUE;

    pSampleConfiguration->storageDisconnectedTime = 0;


#ifdef ENABLE_DATA_CHANNEL
    pSampleConfiguration->onDataChannel = onDataChannel;
#endif
    pSampleConfiguration->mediaType = SAMPLE_STREAMING_AUDIO_VIDEO;
    DLOGI("[KVS Master] Finished setting handlers");

    // Check if the samples are present
    DLOGI("[KVS Master] Checking sample video frame availability....");
    CHK_STATUS(readFrameFromDisk(NULL, &frameSize, "./assets/h264SampleFrames/frame-0001.h264"));
    DLOGI("[KVS Master] Checked sample video frame availability....available");

    CHK_STATUS(readFrameFromDisk(NULL, &frameSize, "./assets/opusSampleFrames/sample-001.opus"));
    DLOGI("[KVS Master] Checked sample audio frame availability....available");

    // Initialize KVS WebRTC. This must be done before anything else, and must only be done once.
    CHK_STATUS(initKvsWebRtc());
    DLOGI("[KVS Master] KVS WebRTC initialization completed successfully");
    initialized = TRUE;

    CHK_STATUS(initSignaling(pSampleConfiguration, SAMPLE_MASTER_CLIENT_ID));
    DLOGI("[KVS Master] Channel %s set up done ", pChannelName);

    if(canaryConfig.duration.value != 0) {
        DLOGD("Scheduling canary duration for %lu seconds", canaryConfig.duration.value / HUNDREDS_OF_NANOS_IN_A_SECOND);
        canaryDurationThread = std::thread(shceduleShutdown, canaryConfig.duration.value, pSampleConfiguration);
        canaryDurationThread.detach();
    }

    // Checking for termination
    CHK_STATUS(sessionCleanupWait(pSampleConfiguration));
    DLOGI("[KVS Master] Streaming session terminated");

CleanUp:

    if (retStatus != STATUS_SUCCESS) {
        DLOGE("[KVS Master] Terminated with status code 0x%08x", retStatus);
    }

    DLOGI("Exiting with 0x%08x", retStatus);
    if (initialized) {
        Canary::Cloudwatch::getInstance().monitoring.pushExitStatus(retStatus);
    }
    Canary::Cloudwatch::deinit();

    DLOGI("[KVS Master] Cleaning up....");
    if (pSampleConfiguration != NULL) {
        // Kick of the termination sequence
        ATOMIC_STORE_BOOL(&pSampleConfiguration->appTerminateFlag, TRUE);

        if (pSampleConfiguration->mediaSenderTid != INVALID_TID_VALUE) {
            THREAD_JOIN(pSampleConfiguration->mediaSenderTid, NULL);
        }

        retStatus = signalingClientGetMetrics(pSampleConfiguration->signalingClientHandle, &signalingClientMetrics);
        if (retStatus == STATUS_SUCCESS) {
            logSignalingClientStats(&signalingClientMetrics);
        } else {
            DLOGE("[KVS Master] signalingClientGetMetrics() operation returned status code: 0x%08x", retStatus);
        }
        retStatus = freeSignalingClient(&pSampleConfiguration->signalingClientHandle);
        if (retStatus != STATUS_SUCCESS) {
            DLOGE("[KVS Master] freeSignalingClient(): operation returned status code: 0x%08x", retStatus);
        }

        retStatus = freeSampleConfiguration(&pSampleConfiguration);
        if (retStatus != STATUS_SUCCESS) {
            DLOGE("[KVS Master] freeSampleConfiguration(): operation returned status code: 0x%08x", retStatus);
        }
    }
    DLOGI("[KVS Master] Cleanup done");
    CHK_LOG_ERR(retStatus);

    RESET_INSTRUMENTED_ALLOCATORS();

    // https://www.gnu.org/software/libc/manual/html_node/Exit-Status.html
    // We can only return with 0 - 127. Some platforms treat exit code >= 128
    // to be a success code, which might give an unintended behaviour.
    // Some platforms also treat 1 or 0 differently, so it's better to use
    // EXIT_FAILURE and EXIT_SUCCESS macros for portability.
    return STATUS_FAILED(retStatus) ? EXIT_FAILURE : EXIT_SUCCESS;
}

STATUS readFrameFromDisk(PBYTE pFrame, PUINT32 pSize, PCHAR frameFilePath)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT64 size = 0;
    CHK_ERR(pSize != NULL, STATUS_NULL_ARG, "[KVS Master] Invalid file size");
    size = *pSize;
    // Get the size and read into frame
    CHK_STATUS(readFile(frameFilePath, TRUE, pFrame, &size));
CleanUp:

    if (pSize != NULL) {
        *pSize = (UINT32) size;
    }

    return retStatus;
}

// Save first-frame-sent time to file for consumer-end access.
PVOID writeFirstFrameSentTimeToFile(){
    DLOGI("Opening toConsumer file");
    FILE *toConsumer = FOPEN("../toConsumer.txt", "w");
    if (toConsumer == NULL)
    {
        printf("Error opening toConsumer file\n");
        exit(1);
    }
    fprintf(toConsumer, "%lli\n", GETTIME() / HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
    FCLOSE(toConsumer);
    
}

VOID calculateDisconnectToFrameSentTime(PSampleConfiguration pSampleConfiguration)
{
    UINT64 disconnectTime = pSampleConfiguration->storageDisconnectedTime.load();
    if (disconnectTime != 0){
        DOUBLE storageDisconnectToFrameSentTime = (DOUBLE) (GETTIME() - disconnectTime) / HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
        Canary::Cloudwatch::getInstance().monitoring.pushStorageDisconnectToFrameSentTime(storageDisconnectToFrameSentTime,
                                                                        Aws::CloudWatch::Model::StandardUnit::Milliseconds);
        DLOGI("Setting storageDisconnectedTime to zero");
        pSampleConfiguration->storageDisconnectedTime = 0;
    } else {
        DLOGI("Not sending storageDisconnectToFrameSentTime metric, storageDisconnectedTime is zero (not set)");
    }
}

PVOID sendVideoPackets(PVOID args)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleConfiguration pSampleConfiguration = (PSampleConfiguration) args;
    RtcEncoderStats encoderStats;
    Frame frame;
    UINT32 fileIndex = 0, frameSize;
    CHAR filePath[MAX_PATH_LEN + 1];
    STATUS status;
    UINT32 i;
    UINT64 startTime, lastFrameTime, elapsed;
    MEMSET(&encoderStats, 0x00, SIZEOF(RtcEncoderStats));
    CHK_ERR(pSampleConfiguration != NULL, STATUS_NULL_ARG, "[KVS Master] Streaming session is NULL");

    frame.presentationTs = 0;
    startTime = GETTIME();
    lastFrameTime = startTime;

    while (!ATOMIC_LOAD_BOOL(&pSampleConfiguration->appTerminateFlag)) {
        fileIndex = fileIndex % NUMBER_OF_H264_FRAME_FILES + 1;
        SNPRINTF(filePath, MAX_PATH_LEN, "./assets/h264SampleFrames/frame-%04d.h264", fileIndex);

        CHK_STATUS(readFrameFromDisk(NULL, &frameSize, filePath));

        // Re-alloc if needed
        if (frameSize > pSampleConfiguration->videoBufferSize) {
            pSampleConfiguration->pVideoFrameBuffer = (PBYTE) MEMREALLOC(pSampleConfiguration->pVideoFrameBuffer, frameSize);
            CHK_ERR(pSampleConfiguration->pVideoFrameBuffer != NULL, STATUS_NOT_ENOUGH_MEMORY, "[KVS Master] Failed to allocate video frame buffer");
            pSampleConfiguration->videoBufferSize = frameSize;
        }

        frame.frameData = pSampleConfiguration->pVideoFrameBuffer;
        frame.size = frameSize;

        CHK_STATUS(readFrameFromDisk(frame.frameData, &frameSize, filePath));

        // based on bitrate of samples/h264SampleFrames/frame-*
        encoderStats.width = 640;
        encoderStats.height = 480;
        encoderStats.targetBitrate = 262000;
        frame.presentationTs += SAMPLE_VIDEO_FRAME_DURATION;
        MUTEX_LOCK(pSampleConfiguration->streamingSessionListReadLock);

        for (i = 0; i < pSampleConfiguration->streamingSessionCount; ++i) {

            handleWriteFrameMetricIncrementation(pSampleConfiguration->sampleStreamingSessionList[i], frame.size);

            status = writeFrame(pSampleConfiguration->sampleStreamingSessionList[i]->pVideoRtcRtpTransceiver, &frame);
            if (pSampleConfiguration->sampleStreamingSessionList[i]->firstFrame && status == STATUS_SUCCESS) {
                writeFirstFrameSentTimeToFile();
                PROFILE_WITH_START_TIME(pSampleConfiguration->sampleStreamingSessionList[i]->offerReceiveTime, "Time to first frame");
                
                DOUBLE timeToFirstFrame = (DOUBLE) (GETTIME() - pSampleConfiguration->offerReceiveTimestamp) / HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
                DLOGD("Start up latency from offer receive to first frame write: %lf ms", timeToFirstFrame);
                Canary::Cloudwatch::getInstance().monitoring.pushTimeToFirstFrame(timeToFirstFrame,
                                                                                Aws::CloudWatch::Model::StandardUnit::Milliseconds);
                calculateDisconnectToFrameSentTime(pSampleConfiguration);

                pSampleConfiguration->sampleStreamingSessionList[i]->firstFrame = FALSE;
            }
            encoderStats.encodeTimeMsec = 4; // update encode time to an arbitrary number to demonstrate stats update
            updateEncoderStats(pSampleConfiguration->sampleStreamingSessionList[i]->pVideoRtcRtpTransceiver, &encoderStats);
            if (status != STATUS_SRTP_NOT_READY_YET) {
                if (status != STATUS_SUCCESS) {
                    DLOGV("writeFrame() failed with 0x%08x", status);
                }
            } else {
                // Reset file index to ensure first frame sent upon SRTP ready is a key frame.
                fileIndex = 0;
            }
        }
        MUTEX_UNLOCK(pSampleConfiguration->streamingSessionListReadLock);

        // Adjust sleep in the case the sleep itself and writeFrame take longer than expected. Since sleep makes sure that the thread
        // will be paused at least until the given amount, we can assume that there's no too early frame scenario.
        // Also, it's very unlikely to have a delay greater than SAMPLE_VIDEO_FRAME_DURATION, so the logic assumes that this is always
        // true for simplicity.
        elapsed = lastFrameTime - startTime;
        THREAD_SLEEP(SAMPLE_VIDEO_FRAME_DURATION - elapsed % SAMPLE_VIDEO_FRAME_DURATION);
        lastFrameTime = GETTIME();
    }

CleanUp:
    DLOGI("[KVS Master] Closing video thread");
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
    UINT32 i;
    STATUS status;

    CHK_ERR(pSampleConfiguration != NULL, STATUS_NULL_ARG, "[KVS Master] Streaming session is NULL");
    frame.presentationTs = 0;

    while (!ATOMIC_LOAD_BOOL(&pSampleConfiguration->appTerminateFlag)) {
        fileIndex = fileIndex % NUMBER_OF_OPUS_FRAME_FILES + 1;
        SNPRINTF(filePath, MAX_PATH_LEN, "./assets/opusSampleFrames/sample-%03d.opus", fileIndex);

        CHK_STATUS(readFrameFromDisk(NULL, &frameSize, filePath));

        // Re-alloc if needed
        if (frameSize > pSampleConfiguration->audioBufferSize) {
            pSampleConfiguration->pAudioFrameBuffer = (UINT8*) MEMREALLOC(pSampleConfiguration->pAudioFrameBuffer, frameSize);
            CHK_ERR(pSampleConfiguration->pAudioFrameBuffer != NULL, STATUS_NOT_ENOUGH_MEMORY, "[KVS Master] Failed to allocate audio frame buffer");
            pSampleConfiguration->audioBufferSize = frameSize;
        }

        frame.frameData = pSampleConfiguration->pAudioFrameBuffer;
        frame.size = frameSize;

        CHK_STATUS(readFrameFromDisk(frame.frameData, &frameSize, filePath));

        frame.presentationTs += SAMPLE_AUDIO_FRAME_DURATION;

        MUTEX_LOCK(pSampleConfiguration->streamingSessionListReadLock);
        for (i = 0; i < pSampleConfiguration->streamingSessionCount; ++i) {
            status = writeFrame(pSampleConfiguration->sampleStreamingSessionList[i]->pAudioRtcRtpTransceiver, &frame);
            if (status != STATUS_SRTP_NOT_READY_YET) {
                if (status != STATUS_SUCCESS) {
                    DLOGV("writeFrame() failed with 0x%08x", status);
                } else if (pSampleConfiguration->sampleStreamingSessionList[i]->firstFrame && status == STATUS_SUCCESS) {
                    writeFirstFrameSentTimeToFile();
                    PROFILE_WITH_START_TIME(pSampleConfiguration->sampleStreamingSessionList[i]->offerReceiveTime, "Time to first frame");

                    DOUBLE timeToFirstFrame = (DOUBLE) (GETTIME() - pSampleConfiguration->offerReceiveTimestamp) / HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
                    DLOGD("Start up latency from offer receive to first frame write: %lf ms", timeToFirstFrame);
                    Canary::Cloudwatch::getInstance().monitoring.pushTimeToFirstFrame(timeToFirstFrame,
                                                                                Aws::CloudWatch::Model::StandardUnit::Milliseconds);

                    calculateDisconnectToFrameSentTime(pSampleConfiguration);

                    pSampleConfiguration->sampleStreamingSessionList[i]->firstFrame = FALSE;
                }
            } else {
                // Reset file index to ensure first frame sent upon SRTP ready is a key frame.
                fileIndex = 0;
            }
        }
        MUTEX_UNLOCK(pSampleConfiguration->streamingSessionListReadLock);
        THREAD_SLEEP(SAMPLE_AUDIO_FRAME_DURATION);
    }

CleanUp:
    DLOGI("[KVS Master] closing audio thread");
    return (PVOID) (ULONG_PTR) retStatus;
}

PVOID sampleReceiveAudioVideoFrame(PVOID args)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleStreamingSession pSampleStreamingSession = (PSampleStreamingSession) args;
    CHK_ERR(pSampleStreamingSession != NULL, STATUS_NULL_ARG, "[KVS Master] Streaming session is NULL");
    CHK_STATUS(transceiverOnFrame(pSampleStreamingSession->pVideoRtcRtpTransceiver, (UINT64) pSampleStreamingSession, sampleVideoFrameHandler));
    CHK_STATUS(transceiverOnFrame(pSampleStreamingSession->pAudioRtcRtpTransceiver, (UINT64) pSampleStreamingSession, sampleAudioFrameHandler));

CleanUp:

    return (PVOID) (ULONG_PTR) retStatus;
}
