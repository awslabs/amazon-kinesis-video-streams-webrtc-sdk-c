#include "../common/Samples.h"
#include "../common/StaticMedia.h"

extern PSampleConfiguration gSampleConfiguration;

#define SAMPLE_NAME ((PCHAR) "KVS WebRTC Storage AudioVideo Master")

STATUS addSendOnlyVideoSendrecvAudioTransceivers(PSampleConfiguration, PSampleStreamingSession);

// Debug: periodically dump remote-inbound RTP stats (populated from RTCP receiver reports sent by the media service)
// to verify getRtpRemoteInboundStats returns data during ingestion.
static VOID logRemoteInboundStatsForTransceiver(PRtcPeerConnection pPeerConnection, PRtcRtpTransceiver pTransceiver, PCHAR streamName)
{
    RtcStats rtcStats;

    if (pTransceiver == NULL) {
        return;
    }

    rtcStats.requestedTypeOfStats = RTC_STATS_TYPE_REMOTE_INBOUND_RTP;
    if (STATUS_SUCCEEDED(rtcPeerConnectionGetMetrics(pPeerConnection, pTransceiver, &rtcStats))) {
        DLOGI("[remote-inbound %s] reportsReceived=%" PRIu64 " fractionLost=%.3f roundTripTime=%" PRIu64 " ms totalRoundTripTime=%" PRIu64
              " rttMeasurements=%" PRIu64,
              streamName, rtcStats.rtcStatsObject.remoteInboundRtpStreamStats.reportsReceived,
              rtcStats.rtcStatsObject.remoteInboundRtpStreamStats.fractionLost, rtcStats.rtcStatsObject.remoteInboundRtpStreamStats.roundTripTime,
              rtcStats.rtcStatsObject.remoteInboundRtpStreamStats.totalRoundTripTime,
              rtcStats.rtcStatsObject.remoteInboundRtpStreamStats.roundTripTimeMeasurements);
    }
}

static PVOID remoteInboundStatsThread(PVOID args)
{
    PSampleConfiguration pSampleConfiguration = (PSampleConfiguration) args;
    PSampleStreamingSession pSampleStreamingSession;
    UINT32 i, sleepCounter = 0;

    while (!ATOMIC_LOAD_BOOL(&pSampleConfiguration->appTerminateFlag)) {
        THREAD_SLEEP(HUNDREDS_OF_NANOS_IN_A_SECOND);
        if (++sleepCounter < 10) {
            continue;
        }
        sleepCounter = 0;

        // Use trylock to avoid contending with session teardown
        if (!MUTEX_TRYLOCK(pSampleConfiguration->sampleConfigurationObjLock)) {
            continue;
        }
        for (i = 0; i < pSampleConfiguration->streamingSessionCount; ++i) {
            pSampleStreamingSession = pSampleConfiguration->sampleStreamingSessionList[i];
            logRemoteInboundStatsForTransceiver(pSampleStreamingSession->pPeerConnection, pSampleStreamingSession->pVideoRtcRtpTransceiver,
                                                (PCHAR) "VIDEO");
            logRemoteInboundStatsForTransceiver(pSampleStreamingSession->pPeerConnection, pSampleStreamingSession->pAudioRtcRtpTransceiver,
                                                (PCHAR) "AUDIO");
        }
        MUTEX_UNLOCK(pSampleConfiguration->sampleConfigurationObjLock);
    }

    return NULL;
}

INT32 main(INT32 argc, CHAR* argv[])
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 frameSize;
    TID statsTid = INVALID_TID_VALUE;
    PSampleConfiguration pSampleConfiguration = NULL;
    PCHAR pChannelName;
    SignalingClientMetrics signalingClientMetrics;
    signalingClientMetrics.version = SIGNALING_CLIENT_METRICS_CURRENT_VERSION;
    RTC_CODEC audioCodec = RTC_CODEC_OPUS;
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

    // Set the audio and video handlers
    CHK_STATUS(useStaticFramePresets(pSampleConfiguration, RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE));
    CHK_STATUS(useStaticFramePresets(pSampleConfiguration, RTC_CODEC_OPUS));

    pSampleConfiguration->receiveAudioVideoSource = sampleReceiveAudioVideoFrame;

    pSampleConfiguration->channelInfo.useMediaStorage = TRUE;
    pSampleConfiguration->mediaType = SAMPLE_STREAMING_AUDIO_VIDEO;
    pSampleConfiguration->addTransceiversCallback = addSendOnlyVideoSendrecvAudioTransceivers;
    DLOGI("[%s] Finished setting handlers", SAMPLE_NAME);

    CHK_STATUS(initKvsWebRtc());
    DLOGI("[%s] KVS WebRTC initialization completed successfully", SAMPLE_NAME);

    PROFILE_CALL_WITH_START_END_T_OBJ(
        retStatus = initSignaling(pSampleConfiguration, SAMPLE_MASTER_CLIENT_ID), pSampleConfiguration->signalingClientMetrics.signalingStartTime,
        pSampleConfiguration->signalingClientMetrics.signalingEndTime, pSampleConfiguration->signalingClientMetrics.signalingCallTime,
        "Initialize signaling client and connect to the signaling channel");

    DLOGI("[%s] Channel %s set up done ", SAMPLE_NAME, pChannelName);

    // Debug: start periodic remote-inbound stats dump
    THREAD_CREATE(&statsTid, remoteInboundStatsThread, (PVOID) pSampleConfiguration);

    // Checking for termination
    CHK_STATUS(sessionCleanupWait(pSampleConfiguration));
    DLOGI("[%s] Streaming session terminated", SAMPLE_NAME);

CleanUp:

    if (retStatus != STATUS_SUCCESS) {
        DLOGE("[%s] Terminated with status code 0x%08x", SAMPLE_NAME, retStatus);
    }

    DLOGI("[%s] Cleaning up....", SAMPLE_NAME);
    if (pSampleConfiguration != NULL) {
        // Kick of the termination sequence
        ATOMIC_STORE_BOOL(&pSampleConfiguration->appTerminateFlag, TRUE);

        if (pSampleConfiguration->mediaSenderTid != INVALID_TID_VALUE) {
            THREAD_JOIN(pSampleConfiguration->mediaSenderTid, NULL);
        }

        if (statsTid != INVALID_TID_VALUE) {
            THREAD_JOIN(statsTid, NULL);
        }

        retStatus = signalingClientGetMetrics(pSampleConfiguration->signalingClientHandle, &signalingClientMetrics);
        if (retStatus == STATUS_SUCCESS) {
            logSignalingClientStats(&signalingClientMetrics);
        } else {
            DLOGE("[%s] signalingClientGetMetrics() operation returned status code: 0x%08x", SAMPLE_NAME, retStatus);
        }
        retStatus = freeSignalingClient(&pSampleConfiguration->signalingClientHandle);
        if (retStatus != STATUS_SUCCESS) {
            DLOGE("[%s] freeSignalingClient(): operation returned status code: 0x%08x", SAMPLE_NAME, retStatus);
        }

        retStatus = freeSampleConfiguration(&pSampleConfiguration);
        if (retStatus != STATUS_SUCCESS) {
            DLOGE("[%s] freeSampleConfiguration(): operation returned status code: 0x%08x", SAMPLE_NAME, retStatus);
        }
    }
    DLOGI("[%s] Cleanup done", SAMPLE_NAME);
    CHK_LOG_ERR(retStatus);

    RESET_INSTRUMENTED_ALLOCATORS();

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
    CHK_ERR(pSampleStreamingSession != NULL, STATUS_NULL_ARG, "[%s] Streaming session is NULL", SAMPLE_NAME);
    CHK_STATUS(transceiverOnFrame(pSampleStreamingSession->pVideoRtcRtpTransceiver, (UINT64) pSampleStreamingSession, sampleVideoFrameHandler));
    CHK_STATUS(transceiverOnFrame(pSampleStreamingSession->pAudioRtcRtpTransceiver, (UINT64) pSampleStreamingSession, sampleAudioFrameHandler));

CleanUp:

    return (PVOID) (ULONG_PTR) retStatus;
}
