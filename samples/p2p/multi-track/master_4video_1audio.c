#include "../../common/Samples.h"
#include "../../common/StaticMedia.h"

extern PSampleConfiguration gSampleConfiguration;

#define NUM_VIDEO_TRACKS 4

typedef struct {
    PRtcRtpTransceiver pVideoRtcRtpTransceivers[NUM_VIDEO_TRACKS];
} MultiTrackStreamingSession, *PMultiTrackStreamingSession;

static VOID onMultiTrackSessionShutdown(UINT64 customData, PSampleStreamingSession pSampleStreamingSession)
{
    UNUSED_PARAM(pSampleStreamingSession);
    PMultiTrackStreamingSession pMultiTrackSession = (PMultiTrackStreamingSession) customData;
    SAFE_MEMFREE(pMultiTrackSession);
}

STATUS addFourVideoOneAudioTransceivers(PSampleConfiguration pSampleConfiguration, PSampleStreamingSession pSampleStreamingSession)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    RtcRtpTransceiverInit videoRtpTransceiverInit;
    RtcMediaStreamTrack videoTrack;
    UINT32 i;
    CHAR trackId[MAX_MEDIA_STREAM_TRACK_ID_LEN + 1];
    CHAR streamId[MAX_MEDIA_STREAM_TRACK_ID_LEN + 1];
    PMultiTrackStreamingSession pMultiTrackSession = NULL;

    CHK(pSampleConfiguration != NULL && pSampleStreamingSession != NULL, STATUS_NULL_ARG);

    pMultiTrackSession = (PMultiTrackStreamingSession) MEMCALLOC(1, SIZEOF(MultiTrackStreamingSession));
    CHK(pMultiTrackSession != NULL, STATUS_NOT_ENOUGH_MEMORY);

    for (i = 0; i < NUM_VIDEO_TRACKS; i++) {
        MEMSET(&videoTrack, 0x00, SIZEOF(RtcMediaStreamTrack));
        videoTrack.kind = MEDIA_STREAM_TRACK_KIND_VIDEO;
        videoTrack.codec = pSampleConfiguration->videoCodec;
        videoRtpTransceiverInit.direction = RTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY;

        SNPRINTF(streamId, MAX_MEDIA_STREAM_TRACK_ID_LEN, "myKvsVideoStream%u", i);
        SNPRINTF(trackId, MAX_MEDIA_STREAM_TRACK_ID_LEN, "myVideoTrack%u", i);
        STRCPY(videoTrack.streamId, streamId);
        STRCPY(videoTrack.trackId, trackId);

        CHK_STATUS(addTransceiver(pSampleStreamingSession->pPeerConnection, &videoTrack, &videoRtpTransceiverInit,
                                  &pMultiTrackSession->pVideoRtcRtpTransceivers[i]));

        CHK_STATUS(configureTransceiverRollingBuffer(pMultiTrackSession->pVideoRtcRtpTransceivers[i], &videoTrack,
                                                     pSampleConfiguration->videoRollingBufferDurationSec,
                                                     pSampleConfiguration->videoRollingBufferBitratebps));

        CHK_STATUS(transceiverOnBandwidthEstimation(pMultiTrackSession->pVideoRtcRtpTransceivers[i], (UINT64) pSampleStreamingSession,
                                                    sampleBandwidthEstimationHandler));
    }

    pSampleStreamingSession->pVideoRtcRtpTransceiver = pMultiTrackSession->pVideoRtcRtpTransceivers[0];

    CHK_STATUS(streamingSessionOnShutdown(pSampleStreamingSession, (UINT64) pMultiTrackSession, onMultiTrackSessionShutdown));

    RtcRtpTransceiverInit audioRtpTransceiverInit;
    RtcMediaStreamTrack audioTrack;
    MEMSET(&audioTrack, 0x00, SIZEOF(RtcMediaStreamTrack));
    audioTrack.kind = MEDIA_STREAM_TRACK_KIND_AUDIO;
    audioTrack.codec = pSampleConfiguration->audioCodec;
    audioRtpTransceiverInit.direction = RTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY;
    STRCPY(audioTrack.streamId, "myKvsAudioStream");
    STRCPY(audioTrack.trackId, "myAudioTrack");

    CHK_STATUS(addTransceiver(pSampleStreamingSession->pPeerConnection, &audioTrack, &audioRtpTransceiverInit,
                              &pSampleStreamingSession->pAudioRtcRtpTransceiver));

    CHK_STATUS(configureTransceiverRollingBuffer(pSampleStreamingSession->pAudioRtcRtpTransceiver, &audioTrack,
                                                 pSampleConfiguration->audioRollingBufferDurationSec,
                                                 pSampleConfiguration->audioRollingBufferBitratebps));

    CHK_STATUS(transceiverOnBandwidthEstimation(pSampleStreamingSession->pAudioRtcRtpTransceiver, (UINT64) pSampleStreamingSession,
                                                sampleBandwidthEstimationHandler));

    DLOGI("[KVS Master] Successfully added %u video transceivers and 1 audio transceiver", NUM_VIDEO_TRACKS);

CleanUp:
    if (STATUS_FAILED(retStatus)) {
        SAFE_MEMFREE(pMultiTrackSession);
    }
    CHK_LOG_ERR(retStatus);
    LEAVES();
    return retStatus;
}

PVOID sendFourVideoPacketsFromDisk(PVOID args)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleConfiguration pSampleConfiguration = (PSampleConfiguration) args;
    Frame frame[NUM_VIDEO_TRACKS];
    UINT32 fileIndex[NUM_VIDEO_TRACKS];
    UINT32 frameSize;
    PBYTE pFrameBuffers[NUM_VIDEO_TRACKS];
    UINT32 frameBufferSizes[NUM_VIDEO_TRACKS];
    CHAR filePath[MAX_PATH_LEN + 1];
    STATUS status;
    UINT32 i, trackIdx;
    UINT64 startTime, lastFrameTime, elapsed;
    BOOL usePerTrackDirs = FALSE;
    BOOL trackSentFirstFrame[NUM_VIDEO_TRACKS];
    PCHAR codecDir;
    PCHAR codecExt;
    UINT32 numFrameFiles;

    MEMSET(trackSentFirstFrame, 0x00, SIZEOF(trackSentFirstFrame));
    MEMSET(fileIndex, 0x00, SIZEOF(fileIndex));
    MEMSET(pFrameBuffers, 0x00, SIZEOF(pFrameBuffers));
    MEMSET(frameBufferSizes, 0x00, SIZEOF(frameBufferSizes));
    MEMSET(frame, 0x00, SIZEOF(frame));
    CHK_ERR(pSampleConfiguration != NULL, STATUS_NULL_ARG, "[KVS Master] Streaming session is NULL");

    if (pSampleConfiguration->videoCodec == RTC_CODEC_H265) {
        codecDir = "h265SampleFrames";
        codecExt = "h265";
        numFrameFiles = NUMBER_OF_H265_FRAME_FILES;
    } else {
        codecDir = "h264SampleFrames";
        codecExt = "h264";
        numFrameFiles = NUMBER_OF_H264_FRAME_FILES;
    }

    {
        UINT32 dummySize = 0;
        STATUS checkStatus;
        SNPRINTF(filePath, MAX_PATH_LEN, "./%s_track0/frame-0001.%s", codecDir, codecExt);
        checkStatus = readFrameFromDisk(NULL, &dummySize, filePath);
        usePerTrackDirs = (checkStatus == STATUS_SUCCESS);
    }

    if (usePerTrackDirs) {
        DLOGI("[KVS Master] Using per-track frame directories (%s_track0..3)", codecDir);
    } else {
        DLOGW("[KVS Master] Per-track %s frame directories not found. Sending the same frames on all 4 tracks.", codecExt);
        DLOGW("[KVS Master] To generate unique frames per track, run from the build/samples/ directory:");
        DLOGW("[KVS Master]   ../../scripts/generate_multi_track_frames.sh %s", codecExt);
    }

    startTime = GETTIME();
    lastFrameTime = startTime;

    while (!ATOMIC_LOAD_BOOL(&pSampleConfiguration->appTerminateFlag)) {
        for (trackIdx = 0; trackIdx < NUM_VIDEO_TRACKS; trackIdx++) {
            fileIndex[trackIdx] = fileIndex[trackIdx] % numFrameFiles + 1;
            if (usePerTrackDirs) {
                SNPRINTF(filePath, MAX_PATH_LEN, "./%s_track%u/frame-%04d.%s", codecDir, trackIdx, fileIndex[trackIdx], codecExt);
            } else {
                SNPRINTF(filePath, MAX_PATH_LEN, "./%s/frame-%04d.%s", codecDir, fileIndex[trackIdx], codecExt);
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

        MUTEX_LOCK(pSampleConfiguration->streamingSessionListReadLock);
        for (i = 0; i < pSampleConfiguration->streamingSessionCount; ++i) {
            PMultiTrackStreamingSession pMultiTrackSession =
                (PMultiTrackStreamingSession) pSampleConfiguration->sampleStreamingSessionList[i]->shutdownCallbackCustomData;
            if (pMultiTrackSession == NULL) {
                continue;
            }
            for (trackIdx = 0; trackIdx < NUM_VIDEO_TRACKS; trackIdx++) {
                status = writeFrame(pMultiTrackSession->pVideoRtcRtpTransceivers[trackIdx], &frame[trackIdx]);
                if (trackIdx == 0 && pSampleConfiguration->sampleStreamingSessionList[i]->firstFrame && status == STATUS_SUCCESS) {
                    PROFILE_WITH_START_TIME(pSampleConfiguration->sampleStreamingSessionList[i]->offerReceiveTime, "Time to first frame");
                    pSampleConfiguration->sampleStreamingSessionList[i]->firstFrame = FALSE;
                }
                if (status == STATUS_SUCCESS && !trackSentFirstFrame[trackIdx]) {
                    DLOGI("[KVS Master] First frame sent on video track %u", trackIdx);
                    trackSentFirstFrame[trackIdx] = TRUE;
                } else if (status == STATUS_SRTP_NOT_READY_YET) {
                    fileIndex[trackIdx] = 0;
                } else if (status != STATUS_SUCCESS) {
                    DLOGV("writeFrame() for track %u failed with 0x%08x", trackIdx, status);
                }
            }
        }
        MUTEX_UNLOCK(pSampleConfiguration->streamingSessionListReadLock);

        elapsed = lastFrameTime - startTime;
        THREAD_SLEEP(SAMPLE_VIDEO_FRAME_DURATION - elapsed % SAMPLE_VIDEO_FRAME_DURATION);
        lastFrameTime = GETTIME();
    }

CleanUp:
    for (trackIdx = 0; trackIdx < NUM_VIDEO_TRACKS; trackIdx++) {
        SAFE_MEMFREE(pFrameBuffers[trackIdx]);
    }
    DLOGI("Closing video thread");
    CHK_LOG_ERR(retStatus);
    return (PVOID) (ULONG_PTR) retStatus;
}

INT32 main(INT32 argc, CHAR* argv[])
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleConfiguration pSampleConfiguration = NULL;
    PCHAR pChannelName;
    SignalingClientMetrics signalingClientMetrics;
    signalingClientMetrics.version = SIGNALING_CLIENT_METRICS_CURRENT_VERSION;
    RTC_CODEC videoCodec = RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE;

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

    CHK_STATUS(createSampleConfiguration(pChannelName, SIGNALING_CHANNEL_ROLE_TYPE_MASTER, TRUE, TRUE, logLevel, &pSampleConfiguration));

    if (argc > 2) {
        if (!STRCMP(argv[2], VIDEO_CODEC_NAME_H265)) {
            videoCodec = RTC_CODEC_H265;
        } else {
            DLOGI("[KVS Master] Defaulting to H264 as the specified codec's sample frames may not be available");
        }
    }

    CHK_STATUS(useStaticFramePresets(pSampleConfiguration, videoCodec));
    CHK_STATUS(useStaticFramePresets(pSampleConfiguration, RTC_CODEC_OPUS));
    pSampleConfiguration->videoSource = sendFourVideoPacketsFromDisk;

#ifdef ENABLE_DATA_CHANNEL
    pSampleConfiguration->onDataChannel = onDataChannel;
#endif
    pSampleConfiguration->mediaType = SAMPLE_STREAMING_AUDIO_VIDEO;
    pSampleConfiguration->addTransceiversCallback = addFourVideoOneAudioTransceivers;
    DLOGI("[KVS Master] Finished setting up four video track handlers");

    CHK_STATUS(initKvsWebRtc());
    DLOGI("[KVS Master] KVS WebRTC initialization completed successfully");

    PROFILE_CALL_WITH_START_END_T_OBJ(
        retStatus = initSignaling(pSampleConfiguration, SAMPLE_MASTER_CLIENT_ID), pSampleConfiguration->signalingClientMetrics.signalingStartTime,
        pSampleConfiguration->signalingClientMetrics.signalingEndTime, pSampleConfiguration->signalingClientMetrics.signalingCallTime,
        "Initialize signaling client and connect to the signaling channel");

    DLOGI("[KVS Master] Channel %s set up done ", pChannelName);

    CHK_STATUS(sessionCleanupWait(pSampleConfiguration));
    DLOGI("[KVS Master] Streaming session terminated");

CleanUp:

    if (retStatus != STATUS_SUCCESS) {
        DLOGE("[KVS Master] Terminated with status code 0x%08x", retStatus);
    }

    DLOGI("[KVS Master] Cleaning up....");
    if (pSampleConfiguration != NULL) {
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

    return STATUS_FAILED(retStatus) ? EXIT_FAILURE : EXIT_SUCCESS;
}
