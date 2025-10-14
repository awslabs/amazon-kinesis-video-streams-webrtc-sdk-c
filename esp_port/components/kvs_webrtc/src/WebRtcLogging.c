/**
 * Implementation of logging functions for KVS WebRTC
 */
#include <stdio.h>
#include <ctype.h>
#include <com/amazonaws/kinesis/video/webrtcclient/Include.h>
#include "WebRtcLogging.h"

#define TAG "WebRtcLogging"

/**
 * @brief Set the log level for the WebRTC client
 *
 * @return UINT32 The log level that was set
 */
UINT32 setLogLevel(PCHAR pLogLevelStr)
{
    UINT32 logLevel = LOG_LEVEL_DEBUG;
    if (NULL == pLogLevelStr || STATUS_SUCCESS != STRTOUI32(pLogLevelStr, NULL, 10, &logLevel) ||
        logLevel < LOG_LEVEL_VERBOSE || logLevel > LOG_LEVEL_SILENT) {
        logLevel = LOG_LEVEL_WARN;
    }
    SET_LOGGER_LOG_LEVEL(logLevel);
    return logLevel;
}

/**
 * @brief Log signaling client statistics
 *
 * @param pSignalingClientMetrics The signaling client metrics to log
 * @return STATUS Status of the operation
 */
STATUS logSignalingClientStats(PSignalingClientMetrics pSignalingClientMetrics)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    CHK(pSignalingClientMetrics != NULL, STATUS_NULL_ARG);
    DLOGD("Signaling client connection duration: %" PRIu64 " ms",
          (pSignalingClientMetrics->signalingClientStats.connectionDuration / HUNDREDS_OF_NANOS_IN_A_MILLISECOND));
    DLOGD("Number of signaling client API errors: %d", pSignalingClientMetrics->signalingClientStats.numberOfErrors);
    DLOGD("Number of runtime errors in the session: %d", pSignalingClientMetrics->signalingClientStats.numberOfRuntimeErrors);
    DLOGD("Signaling client uptime: %" PRIu64 " ms",
          (pSignalingClientMetrics->signalingClientStats.connectionDuration / HUNDREDS_OF_NANOS_IN_A_MILLISECOND));
    // This gives the EMA of the createChannel, describeChannel, getChannelEndpoint and deleteChannel calls
    DLOGD("Control Plane API call latency: %" PRIu64 " ms",
          (pSignalingClientMetrics->signalingClientStats.cpApiCallLatency / HUNDREDS_OF_NANOS_IN_A_MILLISECOND));
    // This gives the EMA of the getIceConfig() call.
    DLOGD("Data Plane API call latency: %" PRIu64 " ms",
          (pSignalingClientMetrics->signalingClientStats.dpApiCallLatency / HUNDREDS_OF_NANOS_IN_A_MILLISECOND));
    DLOGD("API call retry count: %d", pSignalingClientMetrics->signalingClientStats.apiCallRetryCount);
CleanUp:
    LEAVES();
    return retStatus;
}

/**
 * @brief Log information about selected ICE candidates
 *
 * @param pPeerConnection The peer connection to get ICE candidate information from
 * @return STATUS Status of the operation
 */
STATUS logSelectedIceCandidatesInformation(PRtcPeerConnection pPeerConnection)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    RtcStats rtcMetrics;

    CHK(pPeerConnection != NULL, STATUS_NULL_ARG);
    rtcMetrics.requestedTypeOfStats = RTC_STATS_TYPE_LOCAL_CANDIDATE;
    CHK_STATUS(rtcPeerConnectionGetMetrics(pPeerConnection, NULL, &rtcMetrics));
    DLOGD("Local Candidate IP Address: %s", rtcMetrics.rtcStatsObject.localIceCandidateStats.address);
    DLOGD("Local Candidate type: %s", rtcMetrics.rtcStatsObject.localIceCandidateStats.candidateType);
    DLOGD("Local Candidate port: %d", rtcMetrics.rtcStatsObject.localIceCandidateStats.port);
    DLOGD("Local Candidate priority: %d", rtcMetrics.rtcStatsObject.localIceCandidateStats.priority);
    DLOGD("Local Candidate transport protocol: %s", rtcMetrics.rtcStatsObject.localIceCandidateStats.protocol);
    DLOGD("Local Candidate relay protocol: %s", rtcMetrics.rtcStatsObject.localIceCandidateStats.relayProtocol);
    DLOGD("Local Candidate Ice server source: %s", rtcMetrics.rtcStatsObject.localIceCandidateStats.url);

    rtcMetrics.requestedTypeOfStats = RTC_STATS_TYPE_REMOTE_CANDIDATE;
    CHK_STATUS(rtcPeerConnectionGetMetrics(pPeerConnection, NULL, &rtcMetrics));
    DLOGD("Remote Candidate IP Address: %s", rtcMetrics.rtcStatsObject.remoteIceCandidateStats.address);
    DLOGD("Remote Candidate type: %s", rtcMetrics.rtcStatsObject.remoteIceCandidateStats.candidateType);
    DLOGD("Remote Candidate port: %d", rtcMetrics.rtcStatsObject.remoteIceCandidateStats.port);
    DLOGD("Remote Candidate priority: %d", rtcMetrics.rtcStatsObject.remoteIceCandidateStats.priority);
    DLOGD("Remote Candidate transport protocol: %s", rtcMetrics.rtcStatsObject.remoteIceCandidateStats.protocol);
CleanUp:
    LEAVES();
    return retStatus;
}

/**
 * @brief Gather ICE server statistics for a peer connection
 *
 * @param pPeerConnection The peer connection to get ICE server stats from
 * @param iceUriCount Number of ICE URIs to gather stats for
 * @return STATUS Status of the operation
 */
STATUS gatherIceServerStats(PRtcPeerConnection pPeerConnection, UINT32 iceUriCount)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    RtcStats rtcmetrics;
    UINT32 j = 0;

    CHK(pPeerConnection != NULL, STATUS_NULL_ARG);

    rtcmetrics.requestedTypeOfStats = RTC_STATS_TYPE_ICE_SERVER;
    for (; j < iceUriCount; j++) {
        rtcmetrics.rtcStatsObject.iceServerStats.iceServerIndex = j;
        CHK_STATUS(rtcPeerConnectionGetMetrics(pPeerConnection, NULL, &rtcmetrics));
        DLOGD("ICE Server URL: %s", rtcmetrics.rtcStatsObject.iceServerStats.url);
        DLOGD("ICE Server port: %d", rtcmetrics.rtcStatsObject.iceServerStats.port);
        DLOGD("ICE Server protocol: %s", rtcmetrics.rtcStatsObject.iceServerStats.protocol);
        DLOGD("Total requests sent:%" PRIu64, rtcmetrics.rtcStatsObject.iceServerStats.totalRequestsSent);
        DLOGD("Total responses received: %" PRIu64, rtcmetrics.rtcStatsObject.iceServerStats.totalResponsesReceived);
        DLOGD("Total round trip time: %" PRIu64 "ms",
              rtcmetrics.rtcStatsObject.iceServerStats.totalRoundTripTime / HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
    }
CleanUp:
    LEAVES();
    return retStatus;
}

/**
 * @brief Log ICE candidate pair statistics
 *
 * @param pPeerConnection The peer connection to get ICE candidate pair stats from
 * @param pRtcStats The RTC stats object to populate with ICE candidate pair stats
 * @param pRtcMetricsHistory The metrics history to update with current values
 * @return STATUS Status of the operation
 */
STATUS logIceCandidatePairStats(PRtcPeerConnection pPeerConnection, PRtcStats pRtcStats, PRtcMetricsHistory pRtcMetricsHistory)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT64 currentMeasureDuration = 0;
    DOUBLE averagePacketsDiscardedOnSend = 0.0;
    DOUBLE averageNumberOfPacketsSentPerSecond = 0.0;
    DOUBLE averageNumberOfPacketsReceivedPerSecond = 0.0;
    DOUBLE outgoingBitrate = 0.0;
    DOUBLE incomingBitrate = 0.0;

    CHK(pPeerConnection != NULL && pRtcStats != NULL && pRtcMetricsHistory != NULL, STATUS_NULL_ARG);

    // Set the requested type of stats
    pRtcStats->requestedTypeOfStats = RTC_STATS_TYPE_CANDIDATE_PAIR;

    // Get the metrics
    CHK_STATUS(rtcPeerConnectionGetMetrics(pPeerConnection, NULL, pRtcStats));

    // Calculate the time duration since the last measurement
    currentMeasureDuration = (pRtcStats->timestamp - pRtcMetricsHistory->prevTs) / HUNDREDS_OF_NANOS_IN_A_SECOND;
    DLOGD("Current duration: %" PRIu64 " seconds", currentMeasureDuration);

    if (currentMeasureDuration > 0) {
        // Log the candidate pair information
        DLOGD("Selected local candidate ID: %s", pRtcStats->rtcStatsObject.iceCandidatePairStats.localCandidateId);
        DLOGD("Selected remote candidate ID: %s", pRtcStats->rtcStatsObject.iceCandidatePairStats.remoteCandidateId);
        DLOGD("Ice Candidate Pair state: %d", pRtcStats->rtcStatsObject.iceCandidatePairStats.state);
        DLOGD("Nomination state: %s", pRtcStats->rtcStatsObject.iceCandidatePairStats.nominated ? "nominated" : "not nominated");

        // Calculate and log packet rates
        averageNumberOfPacketsSentPerSecond =
            (DOUBLE) (pRtcStats->rtcStatsObject.iceCandidatePairStats.packetsSent -
                      pRtcMetricsHistory->prevNumberOfPacketsSent) /
            (DOUBLE) currentMeasureDuration;
        DLOGD("Packet send rate: %lf pkts/sec", averageNumberOfPacketsSentPerSecond);

        averageNumberOfPacketsReceivedPerSecond =
            (DOUBLE) (pRtcStats->rtcStatsObject.iceCandidatePairStats.packetsReceived -
                      pRtcMetricsHistory->prevNumberOfPacketsReceived) /
            (DOUBLE) currentMeasureDuration;
        DLOGD("Packet receive rate: %lf pkts/sec", averageNumberOfPacketsReceivedPerSecond);

        // Calculate and log bitrates
        outgoingBitrate = (DOUBLE) ((pRtcStats->rtcStatsObject.iceCandidatePairStats.bytesSent -
                                     pRtcMetricsHistory->prevNumberOfBytesSent) * 8.0) /
            currentMeasureDuration;
        DLOGD("Outgoing bit rate: %lf bps", outgoingBitrate);

        incomingBitrate = (DOUBLE) ((pRtcStats->rtcStatsObject.iceCandidatePairStats.bytesReceived -
                                     pRtcMetricsHistory->prevNumberOfBytesReceived) * 8.0) /
            currentMeasureDuration;
        DLOGD("Incoming bit rate: %lf bps", incomingBitrate);

        // Calculate and log packet discard rate
        averagePacketsDiscardedOnSend =
            (DOUBLE) (pRtcStats->rtcStatsObject.iceCandidatePairStats.packetsDiscardedOnSend -
                      pRtcMetricsHistory->prevPacketsDiscardedOnSend) /
            (DOUBLE) currentMeasureDuration;
        DLOGD("Packet discard rate: %lf pkts/sec", averagePacketsDiscardedOnSend);

        // Log round trip time and STUN responses
        DLOGD("Current STUN request round trip time: %lf sec", pRtcStats->rtcStatsObject.iceCandidatePairStats.currentRoundTripTime);
        DLOGD("Number of STUN responses received: %llu", pRtcStats->rtcStatsObject.iceCandidatePairStats.responsesReceived);

        // Update the metrics history with the current values
        pRtcMetricsHistory->prevTs = pRtcStats->timestamp;
        pRtcMetricsHistory->prevNumberOfPacketsSent = pRtcStats->rtcStatsObject.iceCandidatePairStats.packetsSent;
        pRtcMetricsHistory->prevNumberOfPacketsReceived = pRtcStats->rtcStatsObject.iceCandidatePairStats.packetsReceived;
        pRtcMetricsHistory->prevNumberOfBytesSent = pRtcStats->rtcStatsObject.iceCandidatePairStats.bytesSent;
        pRtcMetricsHistory->prevNumberOfBytesReceived = pRtcStats->rtcStatsObject.iceCandidatePairStats.bytesReceived;
        pRtcMetricsHistory->prevPacketsDiscardedOnSend = pRtcStats->rtcStatsObject.iceCandidatePairStats.packetsDiscardedOnSend;
    }

CleanUp:
    LEAVES();
    return retStatus;
}
