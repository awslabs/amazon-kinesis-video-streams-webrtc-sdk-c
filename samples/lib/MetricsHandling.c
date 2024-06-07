#include "../Samples.h"

STATUS setupMetricsCtx(PSampleStreamingSession pSampleStreamingSession)
{
    STATUS retStatus = STATUS_SUCCESS;
    if (pSampleStreamingSession == NULL) {
        DLOGI("NUll");
    }
    if (pSampleStreamingSession->pSampleConfiguration == NULL) {
        DLOGI("Null config");
    }
    CHK(pSampleStreamingSession != NULL && pSampleStreamingSession->pSampleConfiguration != NULL, STATUS_NULL_ARG);
    if (pSampleStreamingSession->pSampleConfiguration->enableMetrics) {
        CHK(NULL != (pSampleStreamingSession->pStatsCtx = (PStatsCtx) MEMCALLOC(1, SIZEOF(StatsCtx))), STATUS_NOT_ENOUGH_MEMORY);
    }
CleanUp:
    return retStatus;
}

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

// Return ICE server stats for a specific streaming session
STATUS gatherIceServerStats(PSampleStreamingSession pSampleStreamingSession)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 j = 0;
    BOOL locked = TRUE;
    CHK_WARN(pSampleStreamingSession->pStatsCtx != NULL, STATUS_NULL_ARG, "Stats object not set up. Nothing to report");
    MUTEX_LOCK(pSampleStreamingSession->pStatsCtx->statsUpdateLock);
    locked = TRUE;
    pSampleStreamingSession->pStatsCtx->kvsRtcStats.requestedTypeOfStats = RTC_STATS_TYPE_ICE_SERVER;
    for (; j < pSampleStreamingSession->pSampleConfiguration->iceUriCount; j++) {
        pSampleStreamingSession->pStatsCtx->kvsRtcStats.rtcStatsObject.iceServerStats.iceServerIndex = j;
        CHK_STATUS(rtcPeerConnectionGetMetrics(pSampleStreamingSession->pPeerConnection, NULL, &pSampleStreamingSession->pStatsCtx->kvsRtcStats));
        DLOGD("ICE Server URL: %s", pSampleStreamingSession->pStatsCtx->kvsRtcStats.rtcStatsObject.iceServerStats.url);
        DLOGD("ICE Server port: %d", pSampleStreamingSession->pStatsCtx->kvsRtcStats.rtcStatsObject.iceServerStats.port);
        DLOGD("ICE Server protocol: %s", pSampleStreamingSession->pStatsCtx->kvsRtcStats.rtcStatsObject.iceServerStats.protocol);
        DLOGD("Total requests sent:%" PRIu64, pSampleStreamingSession->pStatsCtx->kvsRtcStats.rtcStatsObject.iceServerStats.totalRequestsSent);
        DLOGD("Total responses received: %" PRIu64,
              pSampleStreamingSession->pStatsCtx->kvsRtcStats.rtcStatsObject.iceServerStats.totalResponsesReceived);
        DLOGD("Total round trip time: %" PRIu64 "ms",
              pSampleStreamingSession->pStatsCtx->kvsRtcStats.rtcStatsObject.iceServerStats.totalRoundTripTime / HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
    }
CleanUp:
    if (locked) {
        MUTEX_UNLOCK(pSampleStreamingSession->pStatsCtx->statsUpdateLock);
    }
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
                                      pSampleConfiguration->sampleStreamingSessionList[i]->rtpMetricsHistory.prevTs) /
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
                              pSampleConfiguration->sampleStreamingSessionList[i]->rtpMetricsHistory.prevNumberOfPacketsSent) /
                    (DOUBLE) currentMeasureDuration;
                DLOGD("Packet send rate: %lf pkts/sec", averageNumberOfPacketsSentPerSecond);

                averageNumberOfPacketsReceivedPerSecond =
                    (DOUBLE) (pSampleConfiguration->rtcIceCandidatePairMetrics.rtcStatsObject.iceCandidatePairStats.packetsReceived -
                              pSampleConfiguration->sampleStreamingSessionList[i]->rtpMetricsHistory.prevNumberOfPacketsReceived) /
                    (DOUBLE) currentMeasureDuration;
                DLOGD("Packet receive rate: %lf pkts/sec", averageNumberOfPacketsReceivedPerSecond);

                outgoingBitrate = (DOUBLE) ((pSampleConfiguration->rtcIceCandidatePairMetrics.rtcStatsObject.iceCandidatePairStats.bytesSent -
                                             pSampleConfiguration->sampleStreamingSessionList[i]->rtpMetricsHistory.prevNumberOfBytesSent) *
                                            8.0) /
                    currentMeasureDuration;
                DLOGD("Outgoing bit rate: %lf bps", outgoingBitrate);

                incomingBitrate = (DOUBLE) ((pSampleConfiguration->rtcIceCandidatePairMetrics.rtcStatsObject.iceCandidatePairStats.bytesReceived -
                                             pSampleConfiguration->sampleStreamingSessionList[i]->rtpMetricsHistory.prevNumberOfBytesReceived) *
                                            8.0) /
                    currentMeasureDuration;
                DLOGD("Incoming bit rate: %lf bps", incomingBitrate);

                averagePacketsDiscardedOnSend =
                    (DOUBLE) (pSampleConfiguration->rtcIceCandidatePairMetrics.rtcStatsObject.iceCandidatePairStats.packetsDiscardedOnSend -
                              pSampleConfiguration->sampleStreamingSessionList[i]->rtpMetricsHistory.prevPacketsDiscardedOnSend) /
                    (DOUBLE) currentMeasureDuration;
                DLOGD("Packet discard rate: %lf pkts/sec", averagePacketsDiscardedOnSend);

                DLOGD("Current STUN request round trip time: %lf sec",
                      pSampleConfiguration->rtcIceCandidatePairMetrics.rtcStatsObject.iceCandidatePairStats.currentRoundTripTime);
                DLOGD("Number of STUN responses received: %llu",
                      pSampleConfiguration->rtcIceCandidatePairMetrics.rtcStatsObject.iceCandidatePairStats.responsesReceived);

                pSampleConfiguration->sampleStreamingSessionList[i]->rtpMetricsHistory.prevTs =
                    pSampleConfiguration->rtcIceCandidatePairMetrics.timestamp;
                pSampleConfiguration->sampleStreamingSessionList[i]->rtpMetricsHistory.prevNumberOfPacketsSent =
                    pSampleConfiguration->rtcIceCandidatePairMetrics.rtcStatsObject.iceCandidatePairStats.packetsSent;
                pSampleConfiguration->sampleStreamingSessionList[i]->rtpMetricsHistory.prevNumberOfPacketsReceived =
                    pSampleConfiguration->rtcIceCandidatePairMetrics.rtcStatsObject.iceCandidatePairStats.packetsReceived;
                pSampleConfiguration->sampleStreamingSessionList[i]->rtpMetricsHistory.prevNumberOfBytesSent =
                    pSampleConfiguration->rtcIceCandidatePairMetrics.rtcStatsObject.iceCandidatePairStats.bytesSent;
                pSampleConfiguration->sampleStreamingSessionList[i]->rtpMetricsHistory.prevNumberOfBytesReceived =
                    pSampleConfiguration->rtcIceCandidatePairMetrics.rtcStatsObject.iceCandidatePairStats.bytesReceived;
                pSampleConfiguration->sampleStreamingSessionList[i]->rtpMetricsHistory.prevPacketsDiscardedOnSend =
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

    currentDuration =
        (DOUBLE) (pSampleStreamingSession->pStatsCtx->kvsRtcStats.timestamp - pSampleStreamingSession->pStatsCtx->outgoingRTPStatsCtx.prevTs) /
        HUNDREDS_OF_NANOS_IN_A_SECOND;
    pSampleStreamingSession->pStatsCtx->outgoingRTPStatsCtx.framesPercentageDiscarded =
        ((DOUBLE) (pSampleStreamingSession->pStatsCtx->kvsRtcStats.rtcStatsObject.outboundRtpStreamStats.framesDiscardedOnSend -
                   pSampleStreamingSession->pStatsCtx->outgoingRTPStatsCtx.prevFramesDiscardedOnSend) /
         (DOUBLE) pSampleStreamingSession->pStatsCtx->outgoingRTPStatsCtx.videoFramesGenerated) *
        100.0;
    pSampleStreamingSession->pStatsCtx->outgoingRTPStatsCtx.retxBytesPercentage =
        (((DOUBLE) pSampleStreamingSession->pStatsCtx->kvsRtcStats.rtcStatsObject.outboundRtpStreamStats.retransmittedBytesSent -
          (DOUBLE) (pSampleStreamingSession->pStatsCtx->outgoingRTPStatsCtx.prevRetxBytesSent)) /
         (DOUBLE) pSampleStreamingSession->pStatsCtx->outgoingRTPStatsCtx.videoBytesGenerated) *
        100.0;

    // This flag ensures the reset of video bytes count is done only when this flag is set
    pSampleStreamingSession->pStatsCtx->outgoingRTPStatsCtx.recorded = TRUE;
    pSampleStreamingSession->pStatsCtx->outgoingRTPStatsCtx.averageFramesSentPerSecond =
        ((DOUBLE) (pSampleStreamingSession->pStatsCtx->kvsRtcStats.rtcStatsObject.outboundRtpStreamStats.framesSent -
                   (DOUBLE) pSampleStreamingSession->pStatsCtx->outgoingRTPStatsCtx.prevFramesSent)) /
        currentDuration;
    pSampleStreamingSession->pStatsCtx->outgoingRTPStatsCtx.nacksPerSecond =
        ((DOUBLE) pSampleStreamingSession->pStatsCtx->kvsRtcStats.rtcStatsObject.outboundRtpStreamStats.nackCount -
         pSampleStreamingSession->pStatsCtx->outgoingRTPStatsCtx.prevNackCount) /
        currentDuration;
    pSampleStreamingSession->pStatsCtx->outgoingRTPStatsCtx.prevFramesSent =
        pSampleStreamingSession->pStatsCtx->kvsRtcStats.rtcStatsObject.outboundRtpStreamStats.framesSent;
    pSampleStreamingSession->pStatsCtx->outgoingRTPStatsCtx.prevTs = pSampleStreamingSession->pStatsCtx->kvsRtcStats.timestamp;
    pSampleStreamingSession->pStatsCtx->outgoingRTPStatsCtx.prevFramesDiscardedOnSend =
        pSampleStreamingSession->pStatsCtx->kvsRtcStats.rtcStatsObject.outboundRtpStreamStats.framesDiscardedOnSend;
    pSampleStreamingSession->pStatsCtx->outgoingRTPStatsCtx.prevNackCount =
        pSampleStreamingSession->pStatsCtx->kvsRtcStats.rtcStatsObject.outboundRtpStreamStats.nackCount;
    pSampleStreamingSession->pStatsCtx->outgoingRTPStatsCtx.prevRetxBytesSent =
        pSampleStreamingSession->pStatsCtx->kvsRtcStats.rtcStatsObject.outboundRtpStreamStats.retransmittedBytesSent;

    return STATUS_SUCCESS;
}

STATUS populateIncomingRtpMetricsContext(PSampleStreamingSession pSampleStreamingSession)
{
    DOUBLE currentDuration = 0;
    STATUS retStatus = STATUS_SUCCESS;
    CHK_WARN(pSampleStreamingSession->pStatsCtx != NULL, STATUS_NULL_ARG, "Stats object not set up. Nothing to report");
    currentDuration =
        (DOUBLE) (pSampleStreamingSession->pStatsCtx->kvsRtcStats.timestamp - pSampleStreamingSession->pStatsCtx->incomingRTPStatsCtx.prevTs) /
        HUNDREDS_OF_NANOS_IN_A_SECOND;
    pSampleStreamingSession->pStatsCtx->incomingRTPStatsCtx.packetReceiveRate =
        (DOUBLE) (pSampleStreamingSession->pStatsCtx->kvsRtcStats.rtcStatsObject.inboundRtpStreamStats.received.packetsReceived -
                  pSampleStreamingSession->pStatsCtx->incomingRTPStatsCtx.prevPacketsReceived) /
        currentDuration;
    pSampleStreamingSession->pStatsCtx->incomingRTPStatsCtx.incomingBitRate =
        ((DOUBLE) (pSampleStreamingSession->pStatsCtx->kvsRtcStats.rtcStatsObject.inboundRtpStreamStats.bytesReceived -
                   pSampleStreamingSession->pStatsCtx->incomingRTPStatsCtx.prevBytesReceived) /
         currentDuration) /
        0.008;
    pSampleStreamingSession->pStatsCtx->incomingRTPStatsCtx.framesDroppedPerSecond =
        ((DOUBLE) pSampleStreamingSession->pStatsCtx->kvsRtcStats.rtcStatsObject.inboundRtpStreamStats.received.framesDropped -
         pSampleStreamingSession->pStatsCtx->incomingRTPStatsCtx.prevFramesDropped) /
        currentDuration;

    pSampleStreamingSession->pStatsCtx->incomingRTPStatsCtx.prevPacketsReceived =
        pSampleStreamingSession->pStatsCtx->kvsRtcStats.rtcStatsObject.inboundRtpStreamStats.received.packetsReceived;
    pSampleStreamingSession->pStatsCtx->incomingRTPStatsCtx.prevBytesReceived =
        pSampleStreamingSession->pStatsCtx->kvsRtcStats.rtcStatsObject.inboundRtpStreamStats.bytesReceived;
    pSampleStreamingSession->pStatsCtx->incomingRTPStatsCtx.prevFramesDropped =
        pSampleStreamingSession->pStatsCtx->kvsRtcStats.rtcStatsObject.inboundRtpStreamStats.received.framesDropped;
    pSampleStreamingSession->pStatsCtx->incomingRTPStatsCtx.prevTs = pSampleStreamingSession->pStatsCtx->kvsRtcStats.timestamp;

CleanUp:
    return retStatus;
}

STATUS getSdkTimeProfile(PSampleStreamingSession* ppSampleStreamingSession)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleStreamingSession pSampleStreamingSession = *ppSampleStreamingSession;
    CHK_WARN(pSampleStreamingSession->pStatsCtx != NULL, STATUS_NULL_ARG, "Stats object not set up. Nothing to report");
    CHK(!pSampleStreamingSession->firstFrame, STATUS_WAITING_ON_FIRST_FRAME);

    pSampleStreamingSession->pSampleConfiguration->signalingClientMetrics.version = SIGNALING_CLIENT_METRICS_CURRENT_VERSION;
    CHK_STATUS(signalingClientGetMetrics(pSampleStreamingSession->pSampleConfiguration->signalingClientHandle,
                                         &pSampleStreamingSession->pSampleConfiguration->signalingClientMetrics));
    CHK_STATUS(peerConnectionGetMetrics(pSampleStreamingSession->pPeerConnection, &pSampleStreamingSession->pStatsCtx->peerConnectionMetrics));
    DLOGI("Stats here: %d, %d, %d, %d", pSampleStreamingSession->pStatsCtx->peerConnectionMetrics.peerConnectionStats.dtlsSessionSetupTime, pSampleStreamingSession->pStatsCtx->peerConnectionMetrics.peerConnectionStats.peerConnectionCreationTime, pSampleStreamingSession->pStatsCtx->peerConnectionMetrics.peerConnectionStats.stunDnsResolutionTime, pSampleStreamingSession->pStatsCtx->peerConnectionMetrics.peerConnectionStats.iceHolePunchingTime);
    CHK_STATUS(iceAgentGetMetrics(pSampleStreamingSession->pPeerConnection, &pSampleStreamingSession->pStatsCtx->iceMetrics));
CleanUp:
    return retStatus;
}
