/*******************************************
WebRTC Client Metrics internal include file
*******************************************/
#ifndef __KINESIS_VIDEO_WEBRTCCLIENT_METRICS_INCLUDE__
#define __KINESIS_VIDEO_WEBRTCCLIENT_METRICS_INCLUDE__

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

STATUS getIceCandidatePairStats(PRtcPeerConnection, PRtcIceCandidatePairStats);
STATUS getIceCandidateStats(PRtcPeerConnection, PRtcIceCandidateStats);
STATUS getIceServerStats(PRtcPeerConnection, PRtcIceServerStats);
STATUS getTransportStats(PRtcPeerConnection, PRtcTransportStats);
STATUS logWebRTCMetrics(PRtcPeerConnection, RTC_STATS_TYPE);
STATUS getRtpRemoteInboundStats(PRtcPeerConnection, PRtcRemoteInboundRtpStreamStats);
STATUS getRtpOutboundStats(PRtcPeerConnection, PRtcOutboundRtpStreamStats);

#ifdef __cplusplus
}
#endif
#endif /* __KINESIS_VIDEO_WEBRTCCLIENT_STATS_INCLUDE__ */
