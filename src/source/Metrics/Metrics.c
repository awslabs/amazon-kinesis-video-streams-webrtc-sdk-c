/**
 * Kinesis WebRTC Metrics
 */
#define LOG_CLASS "Metrics"
#include "../Include_i.h"

STATUS getIceCandidatePairStats(PRtcPeerConnection pRtcPeerConnection, PRtcIceCandidatePairStats pRtcIceCandidatePairStats)
{
    STATUS retStatus = STATUS_SUCCESS;
    BOOL locked = FALSE;
    PIceAgent pIceAgent = ((PKvsPeerConnection) pRtcPeerConnection)->pIceAgent;
    PKvsPeerConnection pKvsPeerConnection = (PKvsPeerConnection) pRtcPeerConnection;
    CHK((pRtcPeerConnection != NULL || pRtcIceCandidatePairStats != NULL), STATUS_NULL_ARG);
    MUTEX_LOCK(pIceAgent->lock);
    locked = TRUE;
    PRtcIceCandidatePairDiagnostics pRtcIceCandidatePairDiagnostics = &pIceAgent->dataSendingrtcIceCandidatePairDiagnostics;
    STRCPY(pRtcIceCandidatePairStats->localCandidateId, pRtcIceCandidatePairDiagnostics->localCandidateId);
    STRCPY(pRtcIceCandidatePairStats->remoteCandidateId, pRtcIceCandidatePairDiagnostics->remoteCandidateId);
    pRtcIceCandidatePairStats->state = pRtcIceCandidatePairDiagnostics->state;
    pRtcIceCandidatePairStats->packetsDiscardedOnSend = pRtcIceCandidatePairDiagnostics->packetsDiscardedOnSend;
    pRtcIceCandidatePairStats->lastPacketSentTimestamp = pRtcIceCandidatePairDiagnostics->lastPacketSentTimestamp;
    pRtcIceCandidatePairStats->bytesSent = pRtcIceCandidatePairDiagnostics->bytesSent;
    pRtcIceCandidatePairStats->packetsSent = pRtcIceCandidatePairDiagnostics->packetsSent;
CleanUp:
    if (locked) {
        MUTEX_UNLOCK(pIceAgent->lock);
    }
    return retStatus;
}

STATUS getIceCandidateStats(PRtcPeerConnection pRtcPeerConnection, BOOL isRemote, PRtcIceCandidateStats pRtcIceCandidateStats)
{
    STATUS retStatus = STATUS_SUCCESS;
    BOOL locked = FALSE;
    PIceAgent pIceAgent = ((PKvsPeerConnection) pRtcPeerConnection)->pIceAgent;
    CHK((pRtcPeerConnection != NULL || pRtcIceCandidateStats != NULL), STATUS_NULL_ARG);
    MUTEX_LOCK(pIceAgent->lock);
    locked = TRUE;
    PRtcIceCandidateDiagnostics pRtcIceCandidateDiagnostics = &pIceAgent->rtcSelectedRemoteIceCandidateDiagnostics;
    if (!isRemote) {
        pRtcIceCandidateDiagnostics = &pIceAgent->rtcSelectedLocalIceCandidateDiagnostics;
        STRCPY(pRtcIceCandidateStats->relayProtocol, pRtcIceCandidateDiagnostics->relayProtocol);
        STRCPY(pRtcIceCandidateStats->url, pRtcIceCandidateDiagnostics->url);
    }
    STRCPY(pRtcIceCandidateStats->address, pRtcIceCandidateDiagnostics->address);
    STRCPY(pRtcIceCandidateStats->candidateType, pRtcIceCandidateDiagnostics->candidateType);
    pRtcIceCandidateStats->port = pRtcIceCandidateDiagnostics->port;
    pRtcIceCandidateStats->priority = pRtcIceCandidateDiagnostics->priority;
    STRCPY(pRtcIceCandidateStats->protocol, pRtcIceCandidateDiagnostics->protocol);
CleanUp:
    if (locked) {
        MUTEX_UNLOCK(pIceAgent->lock);
    }
    return retStatus;
}

STATUS getIceServerStats(PRtcPeerConnection pRtcPeerConnection, PRtcIceServerStats pRtcIceServerStats)
{
    STATUS retStatus = STATUS_SUCCESS;
    BOOL locked = FALSE;
    PIceAgent pIceAgent = ((PKvsPeerConnection) pRtcPeerConnection)->pIceAgent;
    CHK((pRtcPeerConnection != NULL && pRtcIceServerStats != NULL), STATUS_NULL_ARG);
    CHK(pRtcIceServerStats->iceServerIndex < pIceAgent->iceServersCount, STATUS_ICE_SERVER_INDEX_INVALID);

    MUTEX_LOCK(pIceAgent->lock);
    locked = TRUE;
    pRtcIceServerStats->port = pIceAgent->rtcIceServerDiagnostics[pRtcIceServerStats->iceServerIndex].port;
    STRCPY(pRtcIceServerStats->protocol, pIceAgent->rtcIceServerDiagnostics[pRtcIceServerStats->iceServerIndex].protocol);
    STRCPY(pRtcIceServerStats->url, pIceAgent->rtcIceServerDiagnostics[pRtcIceServerStats->iceServerIndex].url);
    pRtcIceServerStats->totalRequestsSent = pIceAgent->rtcIceServerDiagnostics[pRtcIceServerStats->iceServerIndex].totalRequestsSent;
    pRtcIceServerStats->totalResponsesReceived = pIceAgent->rtcIceServerDiagnostics[pRtcIceServerStats->iceServerIndex].totalResponsesReceived;
    pRtcIceServerStats->totalRoundTripTime = pIceAgent->rtcIceServerDiagnostics[pRtcIceServerStats->iceServerIndex].totalRoundTripTime;
CleanUp:
    if (locked) {
        MUTEX_UNLOCK(pIceAgent->lock);
    }
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

STATUS getRtpRemoteInboundStats(PRtcPeerConnection pRtcPeerConnection, PRtcRtpTransceiver pTransceiver,
                                PRtcRemoteInboundRtpStreamStats pRtcRemoteInboundRtpStreamStats)
{
    STATUS retStatus = STATUS_SUCCESS;
    PDoubleListNode node = NULL;
    PKvsPeerConnection pKvsPeerConnection = (PKvsPeerConnection) pRtcPeerConnection;
    CHK(pRtcPeerConnection != NULL || pRtcRemoteInboundRtpStreamStats != NULL, STATUS_NULL_ARG);
    PKvsRtpTransceiver pKvsRtpTransceiver = (PKvsRtpTransceiver) pTransceiver;
    if (pKvsRtpTransceiver == NULL) {
        CHK_STATUS(doubleListGetHeadNode(pKvsPeerConnection->pTransceievers, &node));
        CHK_STATUS(doubleListGetNodeData(node, (PUINT64) &pKvsRtpTransceiver));
        CHK(pKvsRtpTransceiver != NULL, STATUS_NOT_FOUND);
    }
    // check if specified transceiver belongs to this connection
    CHK_STATUS(hasTransceiverWithSsrc(pKvsPeerConnection, pKvsRtpTransceiver->sender.ssrc));
    MUTEX_LOCK(pKvsRtpTransceiver->sender.statsLock);
    *pRtcRemoteInboundRtpStreamStats = pKvsRtpTransceiver->sender.remoteInboundStats;
    MUTEX_UNLOCK(pKvsRtpTransceiver->sender.statsLock);
CleanUp:
    return retStatus;
}

STATUS getRtpOutboundStats(PRtcPeerConnection pRtcPeerConnection, PRtcRtpTransceiver pTransceiver,
                           PRtcOutboundRtpStreamStats pRtcOutboundRtpStreamStats)
{
    STATUS retStatus = STATUS_SUCCESS;
    PDoubleListNode node = NULL;
    PKvsPeerConnection pKvsPeerConnection = (PKvsPeerConnection) pRtcPeerConnection;
    CHK(pRtcPeerConnection != NULL || pRtcOutboundRtpStreamStats != NULL, STATUS_NULL_ARG);
    PKvsRtpTransceiver pKvsRtpTransceiver = (PKvsRtpTransceiver) pTransceiver;
    if (pKvsRtpTransceiver == NULL) {
        CHK_STATUS(doubleListGetHeadNode(pKvsPeerConnection->pTransceievers, &node));
        CHK_STATUS(doubleListGetNodeData(node, (PUINT64) &pKvsRtpTransceiver));
        CHK(pKvsRtpTransceiver != NULL, STATUS_NOT_FOUND);
    }
    // check if specified transceiver belongs to this connection
    CHK_STATUS(hasTransceiverWithSsrc(pKvsPeerConnection, pKvsRtpTransceiver->sender.ssrc));
    MUTEX_LOCK(pKvsRtpTransceiver->sender.statsLock);
    *pRtcOutboundRtpStreamStats = pKvsRtpTransceiver->sender.outboundStats;
    MUTEX_UNLOCK(pKvsRtpTransceiver->sender.statsLock);
CleanUp:
    return retStatus;
}

STATUS rtcPeerConnectionGetMetrics(PRtcPeerConnection pRtcPeerConnection, PRtcRtpTransceiver pRtcRtpTransceiver, PRtcStats pRtcMetrics)
{
    STATUS retStatus = STATUS_SUCCESS;
    CHK(pRtcPeerConnection != NULL && pRtcMetrics != NULL, STATUS_NULL_ARG);
    switch (pRtcMetrics->requestedTypeOfStats) {
        case RTC_STATS_TYPE_CANDIDATE_PAIR:
            getIceCandidatePairStats(pRtcPeerConnection, &pRtcMetrics->rtcStatsObject.iceCandidatePairStats);
            break;
        case RTC_STATS_TYPE_LOCAL_CANDIDATE:
            CHK_STATUS(getIceCandidateStats(pRtcPeerConnection, FALSE, &pRtcMetrics->rtcStatsObject.localIceCandidateStats));
            DLOGD("ICE local candidate Stats requested at %" PRIu64, pRtcMetrics->timestamp);
            break;
        case RTC_STATS_TYPE_REMOTE_CANDIDATE:
            CHK_STATUS(getIceCandidateStats(pRtcPeerConnection, TRUE, &pRtcMetrics->rtcStatsObject.remoteIceCandidateStats));
            DLOGD("ICE remote candidate Stats requested at %" PRIu64, pRtcMetrics->timestamp);
            break;
        case RTC_STATS_TYPE_TRANSPORT:
            getTransportStats(pRtcPeerConnection, &pRtcMetrics->rtcStatsObject.transportStats);
            break;
        case RTC_STATS_TYPE_REMOTE_INBOUND_RTP:
            getRtpRemoteInboundStats(pRtcPeerConnection, pRtcRtpTransceiver, &pRtcMetrics->rtcStatsObject.remoteInboundRtpStreamStats);
            break;
        case RTC_STATS_TYPE_OUTBOUND_RTP:
            getRtpOutboundStats(pRtcPeerConnection, pRtcRtpTransceiver, &pRtcMetrics->rtcStatsObject.outboundRtpStreamStats);
            break;
        case RTC_STATS_TYPE_ICE_SERVER:
            pRtcMetrics->timestamp = GETTIME();
            CHK_STATUS(getIceServerStats(pRtcPeerConnection, &pRtcMetrics->rtcStatsObject.iceServerStats));
            DLOGD("ICE Server Stats requested at %" PRIu64, pRtcMetrics->timestamp);
            break;
        case RTC_STATS_TYPE_CERTIFICATE:
        case RTC_STATS_TYPE_CSRC:
        case RTC_STATS_TYPE_INBOUND_RTP:
        case RTC_STATS_TYPE_REMOTE_OUTBOUND_RTP:
        case RTC_STATS_TYPE_PEER_CONNECTION:
        case RTC_STATS_TYPE_DATA_CHANNEL:
        case RTC_STATS_TYPE_RECEIVER:
        case RTC_STATS_TYPE_SENDER:
        case RTC_STATS_TYPE_TRACK:
        case RTC_STATS_TYPE_CODEC:
        case RTC_STATS_TYPE_SCTP_TRANSPORT:
        case RTC_STATS_TYPE_TRANSCEIVER:
        case RTC_STATS_TYPE_RTC_ALL:
        default:
            CHK(FALSE, STATUS_NOT_IMPLEMENTED);
    }
CleanUp:
    CHK_LOG_ERR(retStatus);
    return retStatus;
}
