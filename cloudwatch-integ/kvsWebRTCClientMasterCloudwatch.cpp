#include <aws/core/Aws.h>
#include "../samples/Samples.h"
#include "Cloudwatch.h"

// Save first-frame-sent time to file for consumer-end access.
STATUS writeFirstFrameSentTimeToFile(PCHAR fileName) {
    STATUS retStatus = STATUS_SUCCESS;
    DLOGI("Writing to %s file", fileName);
    UINT64 currentTimeMilliS =  GETTIME() / HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
    CHAR cuurrentTimeChars[MAX_UINT64_DIGIT_COUNT + 1]; // +1 accounts for null terminator
    UINT64 writeSize = SPRINTF(cuurrentTimeChars, "%llu", currentTimeMilliS);
    CHK_STATUS(writeFile((PCHAR) fileName, false, false, static_cast<PBYTE>(static_cast<PVOID>(cuurrentTimeChars)), writeSize));
    CleanUp:
    return retStatus;
}

VOID calculateDisconnectToFrameSentTime(PSampleConfiguration pSampleConfiguration)
{
    UINT64 disconnectTime = pSampleConfiguration->storageDisconnectedTime;
    if (disconnectTime != 0) {
        DOUBLE storageDisconnectToFrameSentTime = (DOUBLE) (GETTIME() - disconnectTime) / HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
        CppInteg::Cloudwatch::getInstance().monitoring.pushStorageDisconnectToFrameSentTime(storageDisconnectToFrameSentTime,
                                                                                            Aws::CloudWatch::Model::StandardUnit::Milliseconds);
        DLOGI("Setting storageDisconnectedTime to zero (not set)");
        pSampleConfiguration->storageDisconnectedTime = 0;
    } else {
        DLOGI("Not sending storageDisconnectToFrameSentTime metric, storageDisconnectedTime is zero (not set)");
    }
}

STATUS publishStatsForCanary(RTC_STATS_TYPE statsType, PSampleStreamingSession pSampleStreamingSession)
{
    STATUS retStatus = STATUS_SUCCESS;
    pSampleStreamingSession->pStatsCtx->kvsRtcStats.requestedTypeOfStats = statsType;
    switch (statsType) {
        case RTC_STATS_TYPE_OUTBOUND_RTP:
            CHK_LOG_ERR(rtcPeerConnectionGetMetrics(pSampleStreamingSession->pPeerConnection, pSampleStreamingSession->pVideoRtcRtpTransceiver, &pSampleStreamingSession->pStatsCtx->kvsRtcStats));
            populateOutgoingRtpMetricsContext(pSampleStreamingSession);
            CppInteg::Cloudwatch::getInstance().monitoring.pushOutboundRtpStats(&pSampleStreamingSession->pStatsCtx->outgoingRTPStatsCtx);
            break;
        case RTC_STATS_TYPE_INBOUND_RTP:
            CHK_LOG_ERR(rtcPeerConnectionGetMetrics(pSampleStreamingSession->pPeerConnection, pSampleStreamingSession->pVideoRtcRtpTransceiver, &pSampleStreamingSession->pStatsCtx->kvsRtcStats));
            populateIncomingRtpMetricsContext(pSampleStreamingSession);
            CppInteg::Cloudwatch::getInstance().monitoring.pushInboundRtpStats(&pSampleStreamingSession->pStatsCtx->incomingRTPStatsCtx);
            break;
        default:
            CHK(FALSE, STATUS_NOT_IMPLEMENTED);
    }
    CleanUp:
    return retStatus;
}

VOID sendProfilingMetrics(PSampleConfiguration pSampleConfiguration)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleStreamingSession pSampleStreamingSession = NULL;

    if(pSampleConfiguration == NULL) {
        return;
    }

    while((pSampleStreamingSession = pSampleConfiguration->sampleStreamingSessionList[0]) == NULL) {
        THREAD_SLEEP(HUNDREDS_OF_NANOS_IN_A_MILLISECOND * 100);
    }
    while (!ATOMIC_LOAD_BOOL(&pSampleConfiguration->interrupted)) {
        retStatus = getSdkTimeProfile(pSampleConfiguration->sampleStreamingSessionList[0]);

        if(STATUS_SUCCEEDED(retStatus)) {
            CppInteg::Cloudwatch::getInstance().monitoring.pushSignalingClientMetrics(&pSampleConfiguration->signalingClientMetrics);
            CppInteg::Cloudwatch::getInstance().monitoring.pushPeerConnectionMetrics(&pSampleStreamingSession->pStatsCtx->peerConnectionMetrics);
            CppInteg::Cloudwatch::getInstance().monitoring.pushKvsIceAgentMetrics(&pSampleStreamingSession->pStatsCtx->iceMetrics);
            return;
        } else {
            DLOGI("Waiting on streaming to start 0x%08x", retStatus);
        }
        THREAD_SLEEP(HUNDREDS_OF_NANOS_IN_A_MILLISECOND * 100);
    }
}

VOID addMetadataToFrameData(PBYTE buffer, PFrame pFrame)
{
    PBYTE pCurPtr = buffer + ANNEX_B_NALU_SIZE;
    putUnalignedInt64BigEndian((PINT64) pCurPtr, pFrame->presentationTs);
    pCurPtr += SIZEOF(UINT64);
    putUnalignedInt32BigEndian((PINT32) pCurPtr, pFrame->size);
    pCurPtr += SIZEOF(UINT32);
    putUnalignedInt32BigEndian((PINT32) pCurPtr, COMPUTE_CRC32(buffer, pFrame->size));
}

VOID createMockFrames(PBYTE buffer, PFrame pFrame)
{
    UINT32 i;
    // For decoding purposes, the first 4 bytes need to be a NALu
    putUnalignedInt32BigEndian((PINT32) buffer, 0x00000001);
    for (i = ANNEX_B_NALU_SIZE + FRAME_METADATA_SIZE; i < pFrame->size; i++) {
        buffer[i] = RAND();
    }
    addMetadataToFrameData(buffer, pFrame);
}

PVOID sendMockVideoPackets(PVOID args)
{
    STATUS retStatus = STATUS_SUCCESS;
    STATUS status = STATUS_SUCCESS;
    Frame frame;
    UINT32 i;
    UINT32 hexStrLen = 0;
    UINT32 actualFrameSize = 0;
    UINT32 frameSizeWithoutNalu = 0;
    UINT32 minFrameSize = FRAME_METADATA_SIZE + ((DEFAULT_BITRATE / 8) / DEFAULT_FRAMERATE);
    UINT32 maxFrameSize = (FRAME_METADATA_SIZE + ((DEFAULT_BITRATE / 8) / DEFAULT_FRAMERATE)) * 2;
    PBYTE frameData = NULL;
    UINT64 firstFrameTime = 0;
    PSampleConfiguration pSampleConfiguration = (PSampleConfiguration) args;

    frameData = (PBYTE) MEMALLOC(maxFrameSize);

    // We allocate a bigger buffer to accomodate the hex encoded string
    frame.frameData = (PBYTE) MEMALLOC(maxFrameSize * 3 + 1 + ANNEX_B_NALU_SIZE);
    frame.version = FRAME_CURRENT_VERSION;
    frame.presentationTs = GETTIME();

    while (!ATOMIC_LOAD_BOOL(&pSampleConfiguration->appTerminateFlag)) {

        // This is the actual frame size that includes the metadata and the actual frame data
        // It generates a number between 1082 and 2164 but the actual frame.size may be larger
        // because hexStrLen will vary
        actualFrameSize = (RAND() % (maxFrameSize - minFrameSize + 1)) + minFrameSize;
        frameSizeWithoutNalu = actualFrameSize - ANNEX_B_NALU_SIZE;

        frame.size = actualFrameSize;
        createMockFrames(frameData, &frame);

        // Hex encode the data (without the ANNEX-B NALu) to ensure parts of random frame data is not skipped if they
        // are the same as the ANNEX-B NALu
        CHK_STATUS(hexEncode(frame.frameData + ANNEX_B_NALU_SIZE, frameSizeWithoutNalu, NULL, &hexStrLen));

        // This re-alloc is done in case the estimated size does not match the actual requirement.
        // We do not want to constantly malloc within a loop. Hence, we re-allocate only if required
        // Either ways, the realloc should not happen
        if (hexStrLen != (frameSizeWithoutNalu * 2 + 1)) {
            DLOGW("Re allocating...this should not happen...something might be wrong");
            frame.frameData = (PBYTE) REALLOC(frame.frameData, hexStrLen + ANNEX_B_NALU_SIZE);
            CHK_ERR(frame.frameData != NULL, STATUS_NOT_ENOUGH_MEMORY, "Failed to realloc media buffer");
        }
        CHK_STATUS(hexEncode(frameData + ANNEX_B_NALU_SIZE, frameSizeWithoutNalu, (PCHAR)(frame.frameData + ANNEX_B_NALU_SIZE), &hexStrLen));
        MEMCPY(frame.frameData, frameData, ANNEX_B_NALU_SIZE);

        // We must update the size to reflect the original data with hex encoded data
        frame.size = hexStrLen + ANNEX_B_NALU_SIZE;
        MUTEX_LOCK(pSampleConfiguration->streamingSessionListReadLock);
        for (i = 0; i < pSampleConfiguration->streamingSessionCount; ++i) {
            status = writeFrame(pSampleConfiguration->sampleStreamingSessionList[i]->pVideoRtcRtpTransceiver, &frame);
            pSampleConfiguration->sampleStreamingSessionList[i]->pStatsCtx->outgoingRTPStatsCtx.videoFramesGenerated++;
            pSampleConfiguration->sampleStreamingSessionList[i]->pStatsCtx->outgoingRTPStatsCtx.videoBytesGenerated += frame.size;
            if (pSampleConfiguration->sampleStreamingSessionList[i]->firstFrame && status == STATUS_SUCCESS) {
                PROFILE_WITH_START_TIME_OBJ(pSampleConfiguration->sampleStreamingSessionList[i]->offerReceiveTime, firstFrameTime, "Time to first frame");
                CppInteg::Cloudwatch::getInstance().monitoring.pushTimeToFirstFrame(firstFrameTime, Aws::CloudWatch::Model::StandardUnit::Milliseconds);

                pSampleConfiguration->sampleStreamingSessionList[i]->firstFrame = FALSE;
            }
            if (status != STATUS_SRTP_NOT_READY_YET) {
                if (status != STATUS_SUCCESS) {
                    DLOGV("writeFrame() failed with 0x%08x", status);
                }
            }
            CHK_STATUS(publishStatsForCanary(RTC_STATS_TYPE_OUTBOUND_RTP, pSampleConfiguration->sampleStreamingSessionList[i]));
        }
        MUTEX_UNLOCK(pSampleConfiguration->streamingSessionListReadLock);
        THREAD_SLEEP(HUNDREDS_OF_NANOS_IN_A_SECOND / DEFAULT_FRAME_RATE);
        frame.presentationTs = GETTIME();
    }
    CleanUp:

    SAFE_MEMFREE(frame.frameData);
    SAFE_MEMFREE(frameData);
    return NULL;
}

PVOID sendRealVideoPackets(PVOID args)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleConfiguration pSampleConfiguration = (PSampleConfiguration) args;
    Frame frame;
    UINT32 fileIndex = 0, frameSize;
    CHAR filePath[MAX_PATH_LEN + 1];
    STATUS status;
    UINT32 i;
    UINT64 startTime, lastFrameTime, elapsed;
    CHAR tsFileName[MAX_PATH_LEN + 1];
    UINT64 firstFrameTime = 0;
    CHK_ERR(pSampleConfiguration != NULL, STATUS_NULL_ARG, "[KVS Master] Streaming session is NULL");

    frame.presentationTs = 0;
    startTime = GETTIME();
    lastFrameTime = startTime;

    while (!ATOMIC_LOAD_BOOL(&pSampleConfiguration->appTerminateFlag)) {
        fileIndex = fileIndex % NUMBER_OF_H264_FRAME_FILES + 1;
        if (pSampleConfiguration->videoCodec == RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE) {
            SNPRINTF(filePath, MAX_PATH_LEN, "./h264SampleFrames/frame-%04d.h264", fileIndex);
        } else if (pSampleConfiguration->videoCodec == RTC_CODEC_H265) {
            SNPRINTF(filePath, MAX_PATH_LEN, "./h265SampleFrames/frame-%04d.h265", fileIndex);
        }

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
        frame.presentationTs += SAMPLE_VIDEO_FRAME_DURATION;
        MUTEX_LOCK(pSampleConfiguration->streamingSessionListReadLock);
        for (i = 0; i < pSampleConfiguration->streamingSessionCount; ++i) {
            status = writeFrame(pSampleConfiguration->sampleStreamingSessionList[i]->pVideoRtcRtpTransceiver, &frame);
            if (pSampleConfiguration->sampleStreamingSessionList[i]->firstFrame && status == STATUS_SUCCESS) {
                SNPRINTF(tsFileName, SIZEOF(tsFileName), "%s%s", FIRST_FRAME_TS_FILE_PATH, STORAGE_DEFAULT_FIRST_FRAME_TS_FILE);
                CHK_STATUS(writeFirstFrameSentTimeToFile(tsFileName));
                PROFILE_WITH_START_TIME_OBJ(pSampleConfiguration->sampleStreamingSessionList[i]->offerReceiveTime, firstFrameTime, "Time to first frame");
                CppInteg::Cloudwatch::getInstance().monitoring.pushTimeToFirstFrame(firstFrameTime,
                                                                                    Aws::CloudWatch::Model::StandardUnit::Milliseconds);
                calculateDisconnectToFrameSentTime(pSampleConfiguration);
                pSampleConfiguration->sampleStreamingSessionList[i]->firstFrame = FALSE;
            }
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

PVOID sendRealAudioPackets(PVOID args)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleConfiguration pSampleConfiguration = (PSampleConfiguration) args;
    Frame frame;
    UINT32 fileIndex = 0, frameSize;
    CHAR filePath[MAX_PATH_LEN + 1];
    CHAR tsFileName[MAX_PATH_LEN + 1];
    UINT64 firstFrameTime = 0;
    UINT32 i;
    STATUS status;

    CHK_ERR(pSampleConfiguration != NULL, STATUS_NULL_ARG, "[KVS Master] Streaming session is NULL");
    frame.presentationTs = 0;

    while (!ATOMIC_LOAD_BOOL(&pSampleConfiguration->appTerminateFlag)) {
        fileIndex = fileIndex % NUMBER_OF_OPUS_FRAME_FILES + 1;

        if (pSampleConfiguration->audioCodec == RTC_CODEC_AAC) {
            SNPRINTF(filePath, MAX_PATH_LEN, "./aacSampleFrames/sample-%03d.aac", fileIndex);
        } else if (pSampleConfiguration->audioCodec == RTC_CODEC_OPUS) {
            SNPRINTF(filePath, MAX_PATH_LEN, "./opusSampleFrames/sample-%03d.opus", fileIndex);
        }

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
                    SNPRINTF(tsFileName, SIZEOF(tsFileName), "%s%s", FIRST_FRAME_TS_FILE_PATH, STORAGE_DEFAULT_FIRST_FRAME_TS_FILE);
                    CHK_STATUS(writeFirstFrameSentTimeToFile(tsFileName));
                    PROFILE_WITH_START_TIME_OBJ(pSampleConfiguration->sampleStreamingSessionList[i]->offerReceiveTime, firstFrameTime, "Time to first frame");
                    CppInteg::Cloudwatch::getInstance().monitoring.pushTimeToFirstFrame(firstFrameTime,
                                                                                        Aws::CloudWatch::Model::StandardUnit::Milliseconds);

                    calculateDisconnectToFrameSentTime(pSampleConfiguration);
                    pSampleConfiguration->sampleStreamingSessionList[i]->firstFrame = FALSE;
                }
            } else {
                // Reset file index to stay in sync with video frames.
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

INT32 main(INT32 argc, CHAR* argv[])
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 frameSize;
    PSampleConfiguration pSampleConfiguration = NULL;
    PCHAR region;
    UINT32 terminateId = MAX_UINT32;
    CHAR channelName[MAX_CHANNEL_NAME_LEN];
    PCHAR channelNamePrefix;
    Aws::SDKOptions options;
    options.httpOptions.installSigPipeHandler = TRUE;
    CHAR tsFileName[MAX_PATH_LEN + 1];
    Aws::InitAPI(options);
    {
        SET_INSTRUMENTED_ALLOCATORS();
        UINT32 logLevel = setLogLevel();
        // Initialize KVS WebRTC. This must be done before anything else, and must only be done once.
        CHK_STATUS(initKvsWebRtc());

        if (USE_IOT) {
            DLOGI("Here");
            PCHAR pChannelName;
            CHK_ERR((pChannelName = GETENV(IOT_CORE_THING_NAME)) != NULL, STATUS_INVALID_OPERATION, "AWS_IOT_CORE_THING_NAME must be set since USE_IOT is enabled");
            STRNCPY(channelName, pChannelName, SIZEOF(channelName));
        } else {
            channelNamePrefix = argc > 1 ? argv[1] : CHANNEL_NAME_PREFIX;
            SNPRINTF(channelName, SIZEOF(channelName), CHANNEL_NAME_TEMPLATE, channelNamePrefix, RUNNER_LABEL);
        }
        DLOGI("Here: %s", channelName);
        CHK_STATUS(createSampleConfiguration(channelName, SIGNALING_CHANNEL_ROLE_TYPE_MASTER, TRUE, TRUE, logLevel, &pSampleConfiguration));
        CHK_STATUS(setUpCredentialProvider(pSampleConfiguration, USE_IOT));

        pSampleConfiguration->forceTurn = FORCE_TURN_ONLY;
        pSampleConfiguration->enableMetrics = ENABLE_METRICS;
        pSampleConfiguration->channelInfo.useMediaStorage = ENABLE_STORAGE;
        pSampleConfiguration->audioCodec = AUDIO_CODEC;
        pSampleConfiguration->videoCodec = VIDEO_CODEC;

        if(pSampleConfiguration->channelInfo.useMediaStorage) {
            pSampleConfiguration->audioSource = sendRealAudioPackets;
            pSampleConfiguration->videoSource = sendRealVideoPackets;
            pSampleConfiguration->receiveAudioVideoSource = NULL;
        } else {
            pSampleConfiguration->audioSource = sendRealAudioPackets;
            pSampleConfiguration->videoSource = sendMockVideoPackets;
            pSampleConfiguration->receiveAudioVideoSource = NULL;
        }

        if ((region = GETENV(DEFAULT_REGION_ENV_VAR)) == NULL) {
            region = (PCHAR) DEFAULT_AWS_REGION;
        }
        CppInteg::Cloudwatch::init(channelName, region, TRUE, pSampleConfiguration->channelInfo.useMediaStorage);

        if(ENABLE_DATA_CHANNEL) {
            pSampleConfiguration->onDataChannel = onDataChannel;
        }

        pSampleConfiguration->mediaType = SAMPLE_STREAMING_AUDIO_VIDEO;
        DLOGI("[KVS Master] Finished setting handlers");

        DLOGI("[KVS Master] KVS WebRTC initialization completed successfully");

        PROFILE_CALL_WITH_START_END_T_OBJ(
                retStatus = initSignaling(pSampleConfiguration, (PCHAR) SAMPLE_MASTER_CLIENT_ID), pSampleConfiguration->signalingClientMetrics.signalingStartTime,
                pSampleConfiguration->signalingClientMetrics.signalingEndTime, pSampleConfiguration->signalingClientMetrics.signalingCallTime,
                "Initialize signaling client and connect to the signaling channel");

        DLOGI("[KVS Master] Channel %s set up done ", channelName);

        std::thread pushProfilingThread(sendProfilingMetrics, pSampleConfiguration);

        CHK_STATUS(timerQueueAddTimer(pSampleConfiguration->timerQueueHandle, RUN_TIME, TIMER_QUEUE_SINGLE_INVOCATION_PERIOD, terminate,
                                      (UINT64) pSampleConfiguration, &terminateId));

        // Checking for termination
        CHK_STATUS(sessionCleanupWait(pSampleConfiguration));
        DLOGI("[KVS Master] Streaming session terminated");
        pushProfilingThread.join();
    }

CleanUp:

    if (retStatus != STATUS_SUCCESS) {
        DLOGE("[KVS Master] Terminated with status code 0x%08x", retStatus);
    }

    DLOGI("[KVS Master] Cleaning up....");
    if (pSampleConfiguration != NULL) {
        // Kick of the termination sequence
        ATOMIC_STORE_BOOL(&pSampleConfiguration->appTerminateFlag, TRUE);

        if (pSampleConfiguration->mediaSenderTid != INVALID_TID_VALUE) {
            THREAD_JOIN(pSampleConfiguration->mediaSenderTid, NULL);
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

    retStatus = RESET_INSTRUMENTED_ALLOCATORS();
    DLOGI("All SDK allocations freed? %s..0x%08x", retStatus == STATUS_SUCCESS ? "Yes" : "No", retStatus);
    SNPRINTF(tsFileName, SIZEOF(tsFileName), "%s%s", FIRST_FRAME_TS_FILE_PATH, STORAGE_DEFAULT_FIRST_FRAME_TS_FILE);
    CppInteg::Cloudwatch::getInstance().monitoring.pushExitStatus(retStatus);
    CppInteg::Cloudwatch::deinit();
    Aws::ShutdownAPI(options);

    // https://www.gnu.org/software/libc/manual/html_node/Exit-Status.html
    // We can only return with 0 - 127. Some platforms treat exit code >= 128
    // to be a success code, which might give an unintended behaviour.
    // Some platforms also treat 1 or 0 differently, so it's better to use
    // EXIT_FAILURE and EXIT_SUCCESS macros for portability.
    return STATUS_FAILED(retStatus) ? EXIT_FAILURE : EXIT_SUCCESS;
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
