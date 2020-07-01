/**
 * Kinesis WebRTC Metrics
 */
#define LOG_CLASS "Metrics"
#include "../Include_i.h"

STATUS getIceCandidatePairStats(PRtcPeerConnection pRtcPeerConnection, PRtcIceCandidatePairStats pRtcIceCandidatePairStats)
{
    STATUS retStatus = STATUS_SUCCESS;
    PKvsPeerConnection pKvsPeerConnection = (PKvsPeerConnection) pRtcPeerConnection;
    UNUSED_PARAM(pKvsPeerConnection);
    CHK((pRtcPeerConnection != NULL || pRtcIceCandidatePairStats != NULL), STATUS_NULL_ARG);
    CHK(FALSE, STATUS_NOT_IMPLEMENTED);
CleanUp:
    return retStatus;
}


STATUS getIceCandidateStats(PRtcPeerConnection pRtcPeerConnection, PRtcIceCandidateStats pRtcIceCandidateStats)
{
    STATUS retStatus = STATUS_SUCCESS;
    PKvsPeerConnection pKvsPeerConnection = (PKvsPeerConnection) pRtcPeerConnection;
    UNUSED_PARAM(pKvsPeerConnection);
    CHK((pRtcPeerConnection != NULL || pRtcIceCandidateStats != NULL), STATUS_NULL_ARG);
    CHK(FALSE, STATUS_NOT_IMPLEMENTED);
CleanUp:
    return retStatus;
}

STATUS getIceServerStats(PRtcPeerConnection pRtcPeerConnection, PRtcIceServerStats pRtcIceServerStats)
{
    STATUS retStatus = STATUS_SUCCESS;
    PKvsPeerConnection pKvsPeerConnection = (PKvsPeerConnection) pRtcPeerConnection;
    UNUSED_PARAM(pKvsPeerConnection);
    CHK((pRtcPeerConnection != NULL || pRtcIceServerStats != NULL), STATUS_NULL_ARG);
    CHK(FALSE, STATUS_NOT_IMPLEMENTED);
CleanUp:
    return retStatus;
}

STATUS getTransportStats(PRtcPeerConnection pRtcPeerConnection, PRtcTransportStats pRtcTransportStats)
{
    STATUS retStatus = STATUS_SUCCESS;
    PKvsPeerConnection pKvsPeerConnection = (PKvsPeerConnection) pRtcPeerConnection;
    UNUSED_PARAM(pKvsPeerConnection);
    CHK(pRtcPeerConnection != NULL || pRtcTransportStats != NULL, STATUS_NULL_ARG);
    CHK(FALSE, STATUS_NOT_IMPLEMENTED);
CleanUp:
    return retStatus;
}

STATUS getRtpRemoteInboundStats(PRtcPeerConnection pRtcPeerConnection, PRtcRemoteInboundRtpStreamStats pRtcRemoteInboundRtpStreamStats)
{
    STATUS retStatus = STATUS_SUCCESS;
    PKvsPeerConnection pKvsPeerConnection = (PKvsPeerConnection) pRtcPeerConnection;
    UNUSED_PARAM(pKvsPeerConnection);
    CHK(pRtcPeerConnection != NULL || pRtcRemoteInboundRtpStreamStats != NULL, STATUS_NULL_ARG);
    CHK(FALSE, STATUS_NOT_IMPLEMENTED);
CleanUp:
    return retStatus;
}

STATUS getRtpOutboundStats(PRtcPeerConnection pRtcPeerConnection, PRtcOutboundRtpStreamStats pRtcOutboundRtpStreamStats)
{
    STATUS retStatus = STATUS_SUCCESS;
    PKvsPeerConnection pKvsPeerConnection = (PKvsPeerConnection) pRtcPeerConnection;
    UNUSED_PARAM(pKvsPeerConnection);
    CHK(pRtcPeerConnection != NULL || pRtcOutboundRtpStreamStats != NULL, STATUS_NULL_ARG);
    CHK(FALSE, STATUS_NOT_IMPLEMENTED);
CleanUp:
    return retStatus;
}

STATUS logWebRTCMetrics(PRtcPeerConnection pRtcPeerConnection, RTC_STATS_TYPE statsType) {
    UNUSED_PARAM(statsType);
    STATUS retStatus = STATUS_SUCCESS;
    CHK(pRtcPeerConnection != NULL, STATUS_NULL_ARG);
    DLOGW("logWebRTCMetrics not supported currently");
CleanUp:
    return retStatus;
}

STATUS rtcPeerConnectionGetMetrics(PRtcPeerConnection pRtcPeerConnection, PRtcStats pRtcMetrics)
{
    STATUS retStatus = STATUS_SUCCESS;
    CHK(pRtcPeerConnection != NULL || pRtcMetrics != NULL, STATUS_NULL_ARG);
    switch(pRtcMetrics->requestedTypeOfStats) {
        case RTC_STATS_TYPE_CANDIDATE_PAIR:
            getIceCandidatePairStats(pRtcPeerConnection, &pRtcMetrics->rtcStatsObject.iceCandidatePairStats);
            break;
        case RTC_STATS_TYPE_LOCAL_CANDIDATE:
            getIceCandidateStats(pRtcPeerConnection, &pRtcMetrics->rtcStatsObject.localIceCandidateStats);
            break;
        case RTC_STATS_TYPE_REMOTE_CANDIDATE:
            getIceCandidateStats(pRtcPeerConnection, &pRtcMetrics->rtcStatsObject.remoteIceCandidateStats);
            break;
        case  RTC_STATS_TYPE_TRANSPORT:
            getTransportStats(pRtcPeerConnection, &pRtcMetrics->rtcStatsObject.transportStats);
            break;
        case  RTC_STATS_TYPE_REMOTE_INBOUND_RTP:
            getRtpRemoteInboundStats(pRtcPeerConnection, &pRtcMetrics->rtcStatsObject.remoteInboundRtpStreamStats);
            break;
        case  RTC_STATS_TYPE_OUTBOUND_RTP:
            getRtpOutboundStats(pRtcPeerConnection, &pRtcMetrics->rtcStatsObject.outboundRtpStreamStats);
            break;
        case  RTC_STATS_TYPE_ICE_SERVER:
            getIceServerStats(pRtcPeerConnection, &pRtcMetrics->rtcStatsObject.iceServerStats);
            break;
        case  RTC_STATS_TYPE_CERTIFICATE:
        case  RTC_STATS_TYPE_CSRC:
        case  RTC_STATS_TYPE_INBOUND_RTP:
        case  RTC_STATS_TYPE_REMOTE_OUTBOUND_RTP:
        case  RTC_STATS_TYPE_PEER_CONNECTION:
        case  RTC_STATS_TYPE_DATA_CHANNEL:
        case  RTC_STATS_TYPE_RECEIVER:
        case  RTC_STATS_TYPE_SENDER:
        case  RTC_STATS_TYPE_TRACK:
        case  RTC_STATS_TYPE_CODEC:
        case  RTC_STATS_TYPE_SCTP_TRANSPORT:
        case  RTC_STATS_TYPE_TRANSCEIVER:
        case  RTC_STATS_TYPE_RTC_ALL:
        default:
            CHK(FALSE, STATUS_NOT_IMPLEMENTED);
    }
CleanUp:
    return retStatus;
}
