/*******************************************
WebRTC Client Metrics internal include file
*******************************************/
#ifndef __KINESIS_VIDEO_WEBRTCCLIENT_METRICS_INCLUDE__
#define __KINESIS_VIDEO_WEBRTCCLIENT_METRICS_INCLUDE__

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Get specific ICE candidate pair stats
 * @param [in] PRtcPeerConnection Contains the Ice agent object with diagnostics object
 * @param [in/out] PRtcIceCandidatePairStats Fill up the ICE candidate pair stats for application consumption
 * @return Pass/Fail
 *
 */
STATUS getIceCandidatePairStats(PRtcPeerConnection, PRtcIceCandidatePairStats);

/**
 * @brief Get specific ICE candidate stats
 * @param [in] PRtcPeerConnection Contains the Ice agent object with diagnostics object
 * @param [in/out] PRtcIceCandidateStats Fill up the ICE candidate stats for application consumption
 * @param [in] BOOL If TRUE, local candidate stats are extracted, else remote candidate stats are extracted
 * @return Pass/Fail
 *
 */
STATUS getIceCandidateStats(PRtcPeerConnection, PRtcIceCandidateStats, BOOL);

/**
 * @brief Get specific ICE server stats
 * getIceServerStats will return stats for a specific server. In a multi server configuration, it is upto
 * to the application to get Stats for every server being supported / desired server. The application is
 * expected to pass in the specific iceServerIndex for which the stats are desired
 *
 * @param [in] PRtcPeerConnection Contains the Ice agent object with diagnostics object
 * @param [in/out] PRtcIceServerStats Fill up the ICE candidate stats for application consumption
 * @return Pass/Fail
 *
 */
STATUS getIceServerStats(PRtcPeerConnection, PRtcIceServerStats);

/**
 * @brief Get specific transport stats
 * @param [in] PRtcPeerConnection
 * @param [in/out] PRtcIceCandidateStats Fill up the transport stats for application consumption
 * @return Pass/Fail
 *
 */
STATUS getTransportStats(PRtcPeerConnection, PRtcTransportStats);

/**
 * @brief Get remote RTP inbound stats
 * @param [in] PRtcPeerConnection
 * @param [in] PRtcRtpTransceiver transceiver to get stats for, first transceiver if NULL
 * @param [in/out] PRtcOutboundRtpStreamStats Fill up the RTP inbound stats for application consumption
 * @return Pass/Fail
 *
 */
STATUS getRtpRemoteInboundStats(PRtcPeerConnection, PRtcRtpTransceiver, PRtcRemoteInboundRtpStreamStats);

/**
 * @brief Get RTP outbound stats
 * @param [in] PRtcPeerConnection
 * @param [in] PRtcRtpTransceiver transceiver to get stats for, first transceiver if NULL
 * @param [in/out] PRtcOutboundRtpStreamStats Fill up the RTP outbound stats for application consumption
 * @return Pass/Fail
 *
 */
STATUS getRtpOutboundStats(PRtcPeerConnection, PRtcRtpTransceiver, PRtcOutboundRtpStreamStats);

#ifdef __cplusplus
}
#endif
#endif /* __KINESIS_VIDEO_WEBRTCCLIENT_STATS_INCLUDE__ */
