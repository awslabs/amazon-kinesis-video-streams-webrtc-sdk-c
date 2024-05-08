#include "Samples.h"

STATUS logSelectedIceCandidatesInformation(PSampleStreamingSession pSampleStreamingSession)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    RtcStats rtcMetrics;

    CHK(pSampleStreamingSession != NULL, STATUS_NULL_ARG);
    rtcMetrics.requestedTypeOfStats = RTC_STATS_TYPE_LOCAL_CANDIDATE;
    CHK_STATUS(rtcPeerConnectionGetMetrics(pSampleStreamingSession->pPeerConnection, NULL, &rtcMetrics));
    DLOGI("Local Candidate IP Address: %s", rtcMetrics.rtcStatsObject.localIceCandidateStats.address);
    DLOGI("Local Candidate type: %s", rtcMetrics.rtcStatsObject.localIceCandidateStats.candidateType);
    DLOGI("Local Candidate port: %d", rtcMetrics.rtcStatsObject.localIceCandidateStats.port);
    DLOGI("Local Candidate priority: %d", rtcMetrics.rtcStatsObject.localIceCandidateStats.priority);
    DLOGI("Local Candidate transport protocol: %s", rtcMetrics.rtcStatsObject.localIceCandidateStats.protocol);
    DLOGI("Local Candidate relay protocol: %s", rtcMetrics.rtcStatsObject.localIceCandidateStats.relayProtocol);
    DLOGI("Local Candidate Ice server source: %s", rtcMetrics.rtcStatsObject.localIceCandidateStats.url);

    rtcMetrics.requestedTypeOfStats = RTC_STATS_TYPE_REMOTE_CANDIDATE;
    CHK_STATUS(rtcPeerConnectionGetMetrics(pSampleStreamingSession->pPeerConnection, NULL, &rtcMetrics));
    DLOGI("Remote Candidate IP Address: %s", rtcMetrics.rtcStatsObject.remoteIceCandidateStats.address);
    DLOGI("Remote Candidate type: %s", rtcMetrics.rtcStatsObject.remoteIceCandidateStats.candidateType);
    DLOGI("Remote Candidate port: %d", rtcMetrics.rtcStatsObject.remoteIceCandidateStats.port);
    DLOGI("Remote Candidate priority: %d", rtcMetrics.rtcStatsObject.remoteIceCandidateStats.priority);
    DLOGI("Remote Candidate transport protocol: %s", rtcMetrics.rtcStatsObject.remoteIceCandidateStats.protocol);
CleanUp:
    LEAVES();
    return retStatus;
}

STATUS getIceCandidatePairStatsCallback(UINT32 timerId, UINT64 currentTime, UINT64 customData)
{
    UNUSED_PARAM(timerId);
    UNUSED_PARAM(currentTime);
    STATUS retStatus = STATUS_SUCCESS;
    PSampleConfiguration pSampleConfiguration = (PSampleConfiguration) customData;
    UINT32 i;
    UINT64 currentMeasureDuration = 0;
    DOUBLE averagePacketsDiscardedOnSend = 0.0;
    DOUBLE averageNumberOfPacketsSentPerSecond = 0.0;
    DOUBLE averageNumberOfPacketsReceivedPerSecond = 0.0;
    DOUBLE outgoingBitrate = 0.0;
    DOUBLE incomingBitrate = 0.0;
    BOOL locked = FALSE;

    CHK_WARN(pSampleConfiguration != NULL, STATUS_NULL_ARG, "[KVS Master] getPeriodicStats(): Passed argument is NULL");

    pSampleConfiguration->rtcIceCandidatePairMetrics.requestedTypeOfStats = RTC_STATS_TYPE_CANDIDATE_PAIR;

    // Use MUTEX_TRYLOCK to avoid possible dead lock when canceling timerQueue
    if (!MUTEX_TRYLOCK(pSampleConfiguration->sampleConfigurationObjLock)) {
        return retStatus;
    } else {
        locked = TRUE;
    }

    for (i = 0; i < pSampleConfiguration->streamingSessionCount; ++i) {
        if (STATUS_SUCCEEDED(rtcPeerConnectionGetMetrics(pSampleConfiguration->sampleStreamingSessionList[i]->pPeerConnection, NULL,
                                                         &pSampleConfiguration->rtcIceCandidatePairMetrics))) {
            currentMeasureDuration = (pSampleConfiguration->rtcIceCandidatePairMetrics.timestamp -
                                      pSampleConfiguration->sampleStreamingSessionList[i]->rtcMetricsHistory.prevTs) /
                HUNDREDS_OF_NANOS_IN_A_SECOND;
            DLOGD("Current duration: %" PRIu64 " seconds", currentMeasureDuration);
            if (currentMeasureDuration > 0) {
                DLOGD("Selected local candidate ID: %s",
                      pSampleConfiguration->rtcIceCandidatePairMetrics.rtcStatsObject.iceCandidatePairStats.localCandidateId);
                DLOGD("Selected remote candidate ID: %s",
                      pSampleConfiguration->rtcIceCandidatePairMetrics.rtcStatsObject.iceCandidatePairStats.remoteCandidateId);
                // TODO: Display state as a string for readability
                DLOGD("Ice Candidate Pair state: %d", pSampleConfiguration->rtcIceCandidatePairMetrics.rtcStatsObject.iceCandidatePairStats.state);
                DLOGD("Nomination state: %s",
                      pSampleConfiguration->rtcIceCandidatePairMetrics.rtcStatsObject.iceCandidatePairStats.nominated ? "nominated"
                                                                                                                      : "not nominated");
                averageNumberOfPacketsSentPerSecond =
                    (DOUBLE) (pSampleConfiguration->rtcIceCandidatePairMetrics.rtcStatsObject.iceCandidatePairStats.packetsSent -
                              pSampleConfiguration->sampleStreamingSessionList[i]->rtcMetricsHistory.prevNumberOfPacketsSent) /
                    (DOUBLE) currentMeasureDuration;
                DLOGD("Packet send rate: %lf pkts/sec", averageNumberOfPacketsSentPerSecond);

                averageNumberOfPacketsReceivedPerSecond =
                    (DOUBLE) (pSampleConfiguration->rtcIceCandidatePairMetrics.rtcStatsObject.iceCandidatePairStats.packetsReceived -
                              pSampleConfiguration->sampleStreamingSessionList[i]->rtcMetricsHistory.prevNumberOfPacketsReceived) /
                    (DOUBLE) currentMeasureDuration;
                DLOGD("Packet receive rate: %lf pkts/sec", averageNumberOfPacketsReceivedPerSecond);

                outgoingBitrate = (DOUBLE) ((pSampleConfiguration->rtcIceCandidatePairMetrics.rtcStatsObject.iceCandidatePairStats.bytesSent -
                                             pSampleConfiguration->sampleStreamingSessionList[i]->rtcMetricsHistory.prevNumberOfBytesSent) *
                                            8.0) /
                    currentMeasureDuration;
                DLOGD("Outgoing bit rate: %lf bps", outgoingBitrate);

                incomingBitrate = (DOUBLE) ((pSampleConfiguration->rtcIceCandidatePairMetrics.rtcStatsObject.iceCandidatePairStats.bytesReceived -
                                             pSampleConfiguration->sampleStreamingSessionList[i]->rtcMetricsHistory.prevNumberOfBytesReceived) *
                                            8.0) /
                    currentMeasureDuration;
                DLOGD("Incoming bit rate: %lf bps", incomingBitrate);

                averagePacketsDiscardedOnSend =
                    (DOUBLE) (pSampleConfiguration->rtcIceCandidatePairMetrics.rtcStatsObject.iceCandidatePairStats.packetsDiscardedOnSend -
                              pSampleConfiguration->sampleStreamingSessionList[i]->rtcMetricsHistory.prevPacketsDiscardedOnSend) /
                    (DOUBLE) currentMeasureDuration;
                DLOGD("Packet discard rate: %lf pkts/sec", averagePacketsDiscardedOnSend);

                DLOGD("Current STUN request round trip time: %lf sec",
                      pSampleConfiguration->rtcIceCandidatePairMetrics.rtcStatsObject.iceCandidatePairStats.currentRoundTripTime);
                DLOGD("Number of STUN responses received: %llu",
                      pSampleConfiguration->rtcIceCandidatePairMetrics.rtcStatsObject.iceCandidatePairStats.responsesReceived);

                pSampleConfiguration->sampleStreamingSessionList[i]->rtcMetricsHistory.prevTs =
                    pSampleConfiguration->rtcIceCandidatePairMetrics.timestamp;
                pSampleConfiguration->sampleStreamingSessionList[i]->rtcMetricsHistory.prevNumberOfPacketsSent =
                    pSampleConfiguration->rtcIceCandidatePairMetrics.rtcStatsObject.iceCandidatePairStats.packetsSent;
                pSampleConfiguration->sampleStreamingSessionList[i]->rtcMetricsHistory.prevNumberOfPacketsReceived =
                    pSampleConfiguration->rtcIceCandidatePairMetrics.rtcStatsObject.iceCandidatePairStats.packetsReceived;
                pSampleConfiguration->sampleStreamingSessionList[i]->rtcMetricsHistory.prevNumberOfBytesSent =
                    pSampleConfiguration->rtcIceCandidatePairMetrics.rtcStatsObject.iceCandidatePairStats.bytesSent;
                pSampleConfiguration->sampleStreamingSessionList[i]->rtcMetricsHistory.prevNumberOfBytesReceived =
                    pSampleConfiguration->rtcIceCandidatePairMetrics.rtcStatsObject.iceCandidatePairStats.bytesReceived;
                pSampleConfiguration->sampleStreamingSessionList[i]->rtcMetricsHistory.prevPacketsDiscardedOnSend =
                    pSampleConfiguration->rtcIceCandidatePairMetrics.rtcStatsObject.iceCandidatePairStats.packetsDiscardedOnSend;
            }
        }
    }

CleanUp:

    if (locked) {
        MUTEX_UNLOCK(pSampleConfiguration->sampleConfigurationObjLock);
    }

    return retStatus;
}

STATUS populateOutgoingRtpMetricsContext(PSampleStreamingSession pSampleStreamingSession)
{
    DOUBLE currentDuration = 0;

    currentDuration = (DOUBLE) (pSampleStreamingSession->canaryMetrics.timestamp - pSampleStreamingSession->canaryOutgoingRTPMetricsContext.prevTs) /
        HUNDREDS_OF_NANOS_IN_A_SECOND;
    pSampleStreamingSession->canaryOutgoingRTPMetricsContext.framesPercentageDiscarded =
        ((DOUBLE) (pSampleStreamingSession->canaryMetrics.rtcStatsObject.outboundRtpStreamStats.framesDiscardedOnSend -
                   pSampleStreamingSession->canaryOutgoingRTPMetricsContext.prevFramesDiscardedOnSend) /
         (DOUBLE) pSampleStreamingSession->canaryOutgoingRTPMetricsContext.videoFramesGenerated) *
        100.0;
    pSampleStreamingSession->canaryOutgoingRTPMetricsContext.retxBytesPercentage =
        (((DOUBLE) pSampleStreamingSession->canaryMetrics.rtcStatsObject.outboundRtpStreamStats.retransmittedBytesSent -
          (DOUBLE) (pSampleStreamingSession->canaryOutgoingRTPMetricsContext.prevRetxBytesSent)) /
         (DOUBLE) pSampleStreamingSession->canaryOutgoingRTPMetricsContext.videoBytesGenerated) *
        100.0;

    // This flag ensures the reset of video bytes count is done only when this flag is set
    pSampleStreamingSession->recorded = TRUE;
    pSampleStreamingSession->canaryOutgoingRTPMetricsContext.averageFramesSentPerSecond =
        ((DOUBLE) (pSampleStreamingSession->canaryMetrics.rtcStatsObject.outboundRtpStreamStats.framesSent -
                   (DOUBLE) pSampleStreamingSession->canaryOutgoingRTPMetricsContext.prevFramesSent)) /
        currentDuration;
    pSampleStreamingSession->canaryOutgoingRTPMetricsContext.nacksPerSecond =
        ((DOUBLE) pSampleStreamingSession->canaryMetrics.rtcStatsObject.outboundRtpStreamStats.nackCount -
         pSampleStreamingSession->canaryOutgoingRTPMetricsContext.prevNackCount) /
        currentDuration;
    pSampleStreamingSession->canaryOutgoingRTPMetricsContext.prevFramesSent =
        pSampleStreamingSession->canaryMetrics.rtcStatsObject.outboundRtpStreamStats.framesSent;
    pSampleStreamingSession->canaryOutgoingRTPMetricsContext.prevTs = pSampleStreamingSession->canaryMetrics.timestamp;
    pSampleStreamingSession->canaryOutgoingRTPMetricsContext.prevFramesDiscardedOnSend =
        pSampleStreamingSession->canaryMetrics.rtcStatsObject.outboundRtpStreamStats.framesDiscardedOnSend;
    pSampleStreamingSession->canaryOutgoingRTPMetricsContext.prevNackCount =
        pSampleStreamingSession->canaryMetrics.rtcStatsObject.outboundRtpStreamStats.nackCount;
    pSampleStreamingSession->canaryOutgoingRTPMetricsContext.prevRetxBytesSent =
        pSampleStreamingSession->canaryMetrics.rtcStatsObject.outboundRtpStreamStats.retransmittedBytesSent;

    return STATUS_SUCCESS;
}

STATUS populateIncomingRtpMetricsContext(PSampleStreamingSession pSampleStreamingSession)
{
    DOUBLE currentDuration = 0;
    currentDuration = (DOUBLE) (pSampleStreamingSession->canaryMetrics.timestamp - pSampleStreamingSession->canaryIncomingRTPMetricsContext.prevTs) /
        HUNDREDS_OF_NANOS_IN_A_SECOND;
    pSampleStreamingSession->canaryIncomingRTPMetricsContext.packetReceiveRate =
        (DOUBLE) (pSampleStreamingSession->canaryMetrics.rtcStatsObject.inboundRtpStreamStats.received.packetsReceived -
                  pSampleStreamingSession->canaryIncomingRTPMetricsContext.prevPacketsReceived) /
        currentDuration;
    pSampleStreamingSession->canaryIncomingRTPMetricsContext.incomingBitRate =
        ((DOUBLE) (pSampleStreamingSession->canaryMetrics.rtcStatsObject.inboundRtpStreamStats.bytesReceived -
                   pSampleStreamingSession->canaryIncomingRTPMetricsContext.prevBytesReceived) /
         currentDuration) /
        0.008;
    pSampleStreamingSession->canaryIncomingRTPMetricsContext.framesDroppedPerSecond =
        ((DOUBLE) pSampleStreamingSession->canaryMetrics.rtcStatsObject.inboundRtpStreamStats.received.framesDropped -
         pSampleStreamingSession->canaryIncomingRTPMetricsContext.prevFramesDropped) /
        currentDuration;

    pSampleStreamingSession->canaryIncomingRTPMetricsContext.prevPacketsReceived =
        pSampleStreamingSession->canaryMetrics.rtcStatsObject.inboundRtpStreamStats.received.packetsReceived;
    pSampleStreamingSession->canaryIncomingRTPMetricsContext.prevBytesReceived =
        pSampleStreamingSession->canaryMetrics.rtcStatsObject.inboundRtpStreamStats.bytesReceived;
    pSampleStreamingSession->canaryIncomingRTPMetricsContext.prevFramesDropped =
        pSampleStreamingSession->canaryMetrics.rtcStatsObject.inboundRtpStreamStats.received.framesDropped;
    pSampleStreamingSession->canaryIncomingRTPMetricsContext.prevTs = pSampleStreamingSession->canaryMetrics.timestamp;

    return STATUS_SUCCESS;
}