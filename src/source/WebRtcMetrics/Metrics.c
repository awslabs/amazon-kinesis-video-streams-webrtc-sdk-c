/**
 * Kinesis WebRTC Metrics
 */
#define LOG_CLASS "WebRtcMetrics"
#include "../Include_i.h"


STATUS getIceCandidatePairStats(PRtcPeerConnection pRtcPeerConnection, PRtcIceCandidatePairStats pRtcIceCandidatePairStats)
{
    STATUS retStatus = STATUS_SUCCESS;
    PKvsPeerConnection pKvsPeerConnection = (PKvsPeerConnection) pRtcPeerConnection;
    CHK((pRtcPeerConnection != NULL || pRtcIceCandidatePairStats != NULL), STATUS_NULL_ARG);
    pRtcIceCandidatePairStats->requestsReceived = pKvsPeerConnection->pIceAgent->rtcIceMetrics.rtcIceCandidatePairStats.requestsReceived;
CleanUp:
    return retStatus;
}


STATUS getIceCandidateStats(PRtcPeerConnection pRtcPeerConnection, PRtcIceCandidateStats pRtcIceCandidatePairStats)
{
    STATUS retStatus = STATUS_SUCCESS;
    PKvsPeerConnection pKvsPeerConnection = (PKvsPeerConnection) pRtcPeerConnection;
    CHK((pRtcPeerConnection != NULL || pRtcIceCandidatePairStats != NULL), STATUS_NULL_ARG);
 //   pRtcIceCandidatePairStats->requestsReceived = pKvsPeerConnection->pIceAgent->rtcIceMetrics.rtcIceCandidatePairStats.requestsReceived;
CleanUp:
    return retStatus;
}

STATUS logWebRTCMetrics(PRtcPeerConnection pRtcPeerConnection, RTC_STATS_TYPE statsType) {
    STATUS retStatus = STATUS_SUCCESS;
    CHK(pRtcPeerConnection != NULL, STATUS_NULL_ARG);
CleanUp:
    return retStatus;
}

STATUS getRtcPeerConnectionStats(PRtcPeerConnection pRtcPeerConnection, PRTCStats pRtcMetrics)
{
    STATUS retStatus = STATUS_SUCCESS;
    CHK((pRtcPeerConnection != NULL || pRtcMetrics != NULL), STATUS_NULL_ARG);
    switch(pRtcMetrics->requestedTypeOfStats) {
        case RTC_STATS_TYPE_CANDIDATE_PAIR:
            getIceCandidatePairStats(pRtcPeerConnection, &pRtcMetrics->rtcStatsObject.iceCandidatePairStats);
            break;
        case RTC_STATS_TYPE_LOCAL_CANDIDATE:
            // fall-through
        case RTC_STATS_TYPE_REMOTE_CANDIDATE:
            getIceCandidateStats(pRtcPeerConnection, &pRtcMetrics->rtcStatsObject.iceCandidateStats);
            break;
        default:
            DLOGD("Invalid type of stats object requested");
    }
CleanUp:
    return retStatus;
}

STATUS getSignalingClientStats(SIGNALING_CLIENT_HANDLE signalingClientHandle, PSignalingClientMetrics pSignalingMetrics)
{
    STATUS retStatus = STATUS_SUCCESS;
    CHK(pSignalingMetrics != NULL, STATUS_NULL_ARG);
CleanUp:
    return retStatus;
}


