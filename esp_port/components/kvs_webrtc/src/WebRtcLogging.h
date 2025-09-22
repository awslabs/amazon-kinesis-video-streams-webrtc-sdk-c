/**
 * Header file for WebRTC logging functions
 */
#ifndef __WEBRTC_LOGGING_H__
#define __WEBRTC_LOGGING_H__

#include <com/amazonaws/kinesis/video/webrtcclient/Include.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Structure to track metrics history for RTC stats
 */
typedef struct {
    UINT64 prevNumberOfPacketsSent;
    UINT64 prevNumberOfPacketsReceived;
    UINT64 prevNumberOfBytesSent;
    UINT64 prevNumberOfBytesReceived;
    UINT64 prevPacketsDiscardedOnSend;
    UINT64 prevTs;
    UINT64 sessionStartTime;
} RtcMetricsHistory, *PRtcMetricsHistory;

/**
 * @brief Set the log level for the WebRTC client
 *
 * @param pLogLevelStr Log level as a string, or NULL to use default
 * @return UINT32 The log level that was set
 */
UINT32 setLogLevel(PCHAR pLogLevelStr);

/**
 * @brief Log signaling client statistics
 *
 * @param pSignalingClientMetrics The signaling client metrics to log
 * @return STATUS Status of the operation
 */
STATUS logSignalingClientStats(PSignalingClientMetrics pSignalingClientMetrics);

/**
 * @brief Log information about selected ICE candidates
 *
 * @param pPeerConnection The peer connection to get ICE candidate information from
 * @return STATUS Status of the operation
 */
STATUS logSelectedIceCandidatesInformation(PRtcPeerConnection pPeerConnection);

/**
 * @brief Gather ICE server statistics for a peer connection
 *
 * @param pPeerConnection The peer connection to get ICE server stats from
 * @param iceUriCount Number of ICE URIs to gather stats for
 * @return STATUS Status of the operation
 */
STATUS gatherIceServerStats(PRtcPeerConnection pPeerConnection, UINT32 iceUriCount);

/**
 * @brief Log ICE candidate pair statistics
 *
 * @param pPeerConnection The peer connection to get ICE candidate pair stats from
 * @param pRtcStats The RTC stats object to populate with ICE candidate pair stats
 * @param pRtcMetricsHistory The metrics history to update with current values
 * @return STATUS Status of the operation
 */
STATUS logIceCandidatePairStats(PRtcPeerConnection pPeerConnection, PRtcStats pRtcStats, PRtcMetricsHistory pRtcMetricsHistory);

#ifdef __cplusplus
}
#endif
#endif /* __WEBRTC_LOGGING_H__ */
