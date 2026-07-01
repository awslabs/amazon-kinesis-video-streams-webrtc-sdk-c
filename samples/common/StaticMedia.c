#include "StaticMedia.h"

STATUS useStaticFramePresets(PSampleConfiguration pSampleConfiguration, RTC_CODEC codec)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pSampleConfiguration != NULL, STATUS_NULL_ARG);

    CHK_STATUS(checkSampleFramesExist(codec));

    switch (codec) {
        case RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE:
            pSampleConfiguration->videoCodec = RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE;
            pSampleConfiguration->videoRollingBufferDurationSec = 3;
            pSampleConfiguration->videoRollingBufferBitratebps = 1.4 * 1024 * 1024;
            pSampleConfiguration->videoSource = sendVideoPacketsFromDisk;
            break;
        case RTC_CODEC_H265:
            pSampleConfiguration->videoCodec = RTC_CODEC_H265;
            pSampleConfiguration->videoRollingBufferDurationSec = 3;
            pSampleConfiguration->videoRollingBufferBitratebps = 462 * 1024;
            pSampleConfiguration->videoSource = sendVideoPacketsFromDisk;
            break;
        case RTC_CODEC_OPUS:
            pSampleConfiguration->audioCodec = RTC_CODEC_OPUS;
            pSampleConfiguration->audioRollingBufferDurationSec = 3;
            pSampleConfiguration->audioRollingBufferBitratebps = 512 * 1024;
            pSampleConfiguration->audioSource = sendAudioPacketsFromDisk;
            break;
        default:
            CHK_ERR(FALSE, STATUS_NOT_IMPLEMENTED, "Codec %d not implemented", codec);
    }

CleanUp:
    CHK_LOG_ERR(retStatus);
    return retStatus;
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

PVOID sendVideoPacketsFromDisk(PVOID args)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleConfiguration pSampleConfiguration = (PSampleConfiguration) args;
    RtcEncoderStats encoderStats;
    Frame frame[MAX_VIDEO_TRACK_COUNT];
    UINT32 fileIndex[MAX_VIDEO_TRACK_COUNT];
    UINT32 frameSize;
    PBYTE pFrameBuffers[MAX_VIDEO_TRACK_COUNT];
    UINT32 frameBufferSizes[MAX_VIDEO_TRACK_COUNT];
    CHAR filePath[MAX_PATH_LEN + 1];
    STATUS status;
    UINT32 i, trackIdx;
    UINT64 startTime, lastFrameTime, elapsed;
    BOOL usePerTrackDirs = FALSE;

    MEMSET(&encoderStats, 0x00, SIZEOF(RtcEncoderStats));
    MEMSET(fileIndex, 0x00, SIZEOF(fileIndex));
    MEMSET(pFrameBuffers, 0x00, SIZEOF(pFrameBuffers));
    MEMSET(frameBufferSizes, 0x00, SIZEOF(frameBufferSizes));
    MEMSET(frame, 0x00, SIZEOF(frame));
    CHK_ERR(pSampleConfiguration != NULL, STATUS_NULL_ARG, "[KVS Master] Streaming session is NULL");

    if (pSampleConfiguration->videoCodec == RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE) {
        UINT32 dummySize = 0;
        STATUS checkStatus = readFrameFromDisk(NULL, &dummySize, "./h264SampleFrames_track0/frame-0001.h264");
        usePerTrackDirs = (checkStatus == STATUS_SUCCESS);
    }

    if (usePerTrackDirs) {
        DLOGI("[KVS Master] Using per-track frame directories (h264SampleFrames_track0..3)");
    } else {
        DLOGI("[KVS Master] Using single frame directory for all tracks");
    }

    startTime = GETTIME();
    lastFrameTime = startTime;

    while (!ATOMIC_LOAD_BOOL(&pSampleConfiguration->appTerminateFlag)) {
        for (trackIdx = 0; trackIdx < MAX_VIDEO_TRACK_COUNT; trackIdx++) {
            if (pSampleConfiguration->videoCodec == RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE) {
                fileIndex[trackIdx] = fileIndex[trackIdx] % NUMBER_OF_H264_FRAME_FILES + 1;
                if (usePerTrackDirs) {
                    SNPRINTF(filePath, MAX_PATH_LEN, "./h264SampleFrames_track%u/frame-%04d.h264", trackIdx, fileIndex[trackIdx]);
                } else {
                    SNPRINTF(filePath, MAX_PATH_LEN, "./h264SampleFrames/frame-%04d.h264", fileIndex[trackIdx]);
                }
            } else if (pSampleConfiguration->videoCodec == RTC_CODEC_H265) {
                fileIndex[trackIdx] = fileIndex[trackIdx] % NUMBER_OF_H265_FRAME_FILES + 1;
                SNPRINTF(filePath, MAX_PATH_LEN, "./h265SampleFrames/frame-%04d.h265", fileIndex[trackIdx]);
            }

            frameSize = 0;
            CHK_STATUS(readFrameFromDisk(NULL, &frameSize, filePath));

            if (frameSize > frameBufferSizes[trackIdx]) {
                pFrameBuffers[trackIdx] = (PBYTE) MEMREALLOC(pFrameBuffers[trackIdx], frameSize);
                CHK_ERR(pFrameBuffers[trackIdx] != NULL, STATUS_NOT_ENOUGH_MEMORY, "[KVS Master] Failed to allocate video frame buffer for track %u",
                         trackIdx);
                frameBufferSizes[trackIdx] = frameSize;
            }

            frame[trackIdx].frameData = pFrameBuffers[trackIdx];
            frame[trackIdx].size = frameSize;
            CHK_STATUS(readFrameFromDisk(frame[trackIdx].frameData, &frameSize, filePath));
            frame[trackIdx].presentationTs += SAMPLE_VIDEO_FRAME_DURATION;
        }

        encoderStats.width = 640;
        encoderStats.height = 480;
        encoderStats.targetBitrate = 262000;

        MUTEX_LOCK(pSampleConfiguration->streamingSessionListReadLock);
        for (i = 0; i < pSampleConfiguration->streamingSessionCount; ++i) {
            for (trackIdx = 0; trackIdx < pSampleConfiguration->sampleStreamingSessionList[i]->videoTrackCount; ++trackIdx) {
                status = writeFrame(pSampleConfiguration->sampleStreamingSessionList[i]->pVideoRtcRtpTransceiver[trackIdx], &frame[trackIdx]);
                if (trackIdx == 0 && pSampleConfiguration->sampleStreamingSessionList[i]->firstFrame && status == STATUS_SUCCESS) {
                    PROFILE_WITH_START_TIME(pSampleConfiguration->sampleStreamingSessionList[i]->offerReceiveTime, "Time to first frame");
                    pSampleConfiguration->sampleStreamingSessionList[i]->firstFrame = FALSE;
                }
                encoderStats.encodeTimeMsec = 4;
                updateEncoderStats(pSampleConfiguration->sampleStreamingSessionList[i]->pVideoRtcRtpTransceiver[trackIdx], &encoderStats);
                if (status == STATUS_SRTP_NOT_READY_YET) {
                    fileIndex[trackIdx] = 0;
                } else if (status != STATUS_SUCCESS) {
                    DLOGV("writeFrame() failed with 0x%08x for track %u", status, trackIdx);
                }
            }
        }
        MUTEX_UNLOCK(pSampleConfiguration->streamingSessionListReadLock);

        elapsed = lastFrameTime - startTime;
        THREAD_SLEEP(SAMPLE_VIDEO_FRAME_DURATION - elapsed % SAMPLE_VIDEO_FRAME_DURATION);
        lastFrameTime = GETTIME();
    }

CleanUp:
    for (trackIdx = 0; trackIdx < MAX_VIDEO_TRACK_COUNT; trackIdx++) {
        SAFE_MEMFREE(pFrameBuffers[trackIdx]);
    }
    DLOGI("Closing video thread");
    CHK_LOG_ERR(retStatus);

    return (PVOID) (ULONG_PTR) retStatus;
}

PVOID sendAudioPacketsFromDisk(PVOID args)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleConfiguration pSampleConfiguration = (PSampleConfiguration) args;
    Frame frame;
    UINT32 fileIndex = 0, frameSize = 0;
    CHAR filePath[MAX_PATH_LEN + 1];
    UINT32 i;
    STATUS status;

    CHK_ERR(pSampleConfiguration != NULL, STATUS_NULL_ARG, "[KVS Master] Streaming session is NULL");
    frame.presentationTs = 0;

    while (!ATOMIC_LOAD_BOOL(&pSampleConfiguration->appTerminateFlag)) {
        fileIndex = fileIndex % NUMBER_OF_OPUS_FRAME_FILES + 1;

        if (pSampleConfiguration->audioCodec == RTC_CODEC_OPUS) {
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
                    PROFILE_WITH_START_TIME(pSampleConfiguration->sampleStreamingSessionList[i]->offerReceiveTime, "Time to first frame");
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
    DLOGI("Closing audio thread");
    return (PVOID) (ULONG_PTR) retStatus;
}

STATUS checkSampleFramesExist(RTC_CODEC codec)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 frameSize = 0;

    switch (codec) {
        case RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE:
            retStatus = readFrameFromDisk(NULL, &frameSize, "./h264SampleFrames/frame-0001.h264");
            if (STATUS_FAILED(retStatus)) {
                frameSize = 0;
                retStatus = readFrameFromDisk(NULL, &frameSize, "./h264SampleFrames_track0/frame-0001.h264");
            }
            CHK_STATUS(retStatus);
            DLOGI("Checked H264 sample video frame availability....available");
            break;
        case RTC_CODEC_H265:
            CHK_STATUS(readFrameFromDisk(NULL, &frameSize, "./h265SampleFrames/frame-0001.h265"));
            DLOGI("Checked H265 sample video frame availability....available");
            break;
        case RTC_CODEC_OPUS:
            CHK_STATUS(readFrameFromDisk(NULL, &frameSize, "./opusSampleFrames/sample-001.opus"));
            DLOGI("Checked Opus sample audio frame availability....available");
            break;
        default:
            CHK_ERR(FALSE, STATUS_NOT_IMPLEMENTED, "No sample frames for codec type: %d", codec);
    }

CleanUp:
    CHK_LOG_ERR(retStatus);

    LEAVES();
    return retStatus;
}
