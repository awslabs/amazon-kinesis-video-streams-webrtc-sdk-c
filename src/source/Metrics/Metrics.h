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
// TODO: docs
STATUS getIceCandidateStats(PRtcPeerConnection, PRtcIceCandidateStats);
#ifdef __cplusplus
}
#endif
#endif /* __KINESIS_VIDEO_WEBRTCCLIENT_STATS_INCLUDE__ */
