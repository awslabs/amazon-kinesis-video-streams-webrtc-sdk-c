/*******************************************
GCC (Google Congestion Control) Internal Implementation
Based on draft-ietf-rmcat-gcc-02
*******************************************/
#ifndef __KINESIS_VIDEO_WEBRTC_CLIENT_PEERCONNECTION_GCC_I__
#define __KINESIS_VIDEO_WEBRTC_CLIENT_PEERCONNECTION_GCC_I__

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Public API types (GccState, GccConfig, GccController typedef)
// GCC is experimental and not included in Include.h by default
#include <com/amazonaws/kinesis/video/webrtcclient/Gcc.h>

// GCC Parameters from RFC (Section 5.6)
#define GCC_BURST_TIME_MS          5       // Time limit for packet grouping (ms)
#define GCC_STATE_NOISE_COV        0.001   // q - State noise covariance
#define GCC_INITIAL_ERROR_COV      0.1     // e(0) - Initial system error covariance
#define GCC_INITIAL_THRESHOLD_MS   12.5    // del_var_th(0) - Initial adaptive threshold (ms)
#define GCC_OVERUSE_TIME_TH_MS     10      // Time to trigger over-use signal (ms)
#define GCC_K_U                    0.01    // Threshold increase coefficient
#define GCC_K_D                    0.00018 // Threshold decrease coefficient
#define GCC_BETA                   0.85    // Decrease rate factor
#define GCC_ALPHA_EMA              0.9999  // Exponential moving average coefficient for variance
#define GCC_THRESHOLD_MIN_MS       6.0     // Minimum threshold value (ms)
#define GCC_THRESHOLD_MAX_MS       600.0   // Maximum threshold value (ms)
#define GCC_THRESHOLD_UPDATE_LIMIT 15.0    // Don't update threshold if too far (ms)
#define GCC_ETA_MULTIPLICATIVE     1.08    // Multiplicative increase per second (8%)
#define GCC_CONVERGENCE_SMOOTHING  0.95    // Smoothing factor for convergence tracking
#define GCC_LOSS_THRESHOLD_LOW     0.02    // 2% loss threshold
#define GCC_LOSS_THRESHOLD_HIGH    0.10    // 10% loss threshold

// Default bitrate bounds
#define GCC_DEFAULT_MIN_BITRATE     100000  // 100 kbps
#define GCC_DEFAULT_MAX_BITRATE     2500000 // 2.5 Mbps
#define GCC_DEFAULT_INITIAL_BITRATE 300000  // 300 kbps

// Convert ms to 100ns units (KVS time format)
#define GCC_MS_TO_KVS(ms)  ((UINT64) (ms) * HUNDREDS_OF_NANOS_IN_A_MILLISECOND)
#define GCC_KVS_TO_MS(kvs) ((DOUBLE) (kvs) / HUNDREDS_OF_NANOS_IN_A_MILLISECOND)

/**
 * GCC Over-use Signal (internal)
 */
typedef enum {
    GCC_SIGNAL_NORMAL,  //!< No congestion detected
    GCC_SIGNAL_OVERUSE, //!< Congestion detected (queue building)
    GCC_SIGNAL_UNDERUSE //!< Under-use detected (queue draining)
} GccSignal;

/**
 * Packet group for pre-filtering (RFC Section 5.2)
 * Packets within GCC_BURST_TIME_MS are grouped together
 */
typedef struct {
    UINT64 departureTimeKvs; //!< Send time of last packet in group
    UINT64 arrivalTimeKvs;   //!< Arrival time of last packet in group
    UINT32 totalBytes;       //!< Total bytes in group
    UINT16 firstSeqNum;      //!< First sequence number in group
    UINT16 packetCount;      //!< Number of packets in group
} GccPacketGroup, *PGccPacketGroup;

/**
 * Kalman filter state for delay estimation (RFC Section 5.3)
 */
typedef struct {
    DOUBLE m_hat;     //!< Estimated queuing delay trend (ms)
    DOUBLE e;         //!< System error covariance
    DOUBLE var_v_hat; //!< Estimated measurement noise variance
    DOUBLE q;         //!< State noise covariance (constant: GCC_STATE_NOISE_COV)
} GccKalmanState, *PGccKalmanState;

/**
 * Over-use detector state (RFC Section 5.4)
 */
typedef struct {
    DOUBLE del_var_th;          //!< Adaptive threshold (ms)
    UINT64 overuseStartTimeKvs; //!< Time when overuse condition started
    DOUBLE prevM_hat;           //!< Previous m_hat value (for trend detection)
} GccOveruseDetector, *PGccOveruseDetector;

/**
 * Rate controller state (RFC Section 5.5)
 */
typedef struct {
    GccState state;           //!< Current state: HOLD/INCREASE/DECREASE
    UINT64 A_hat;             //!< Delay-based available bandwidth estimate (bps)
    UINT64 As_hat;            //!< Loss-based available bandwidth estimate (bps)
    UINT64 targetBitrate;     //!< Final target bitrate (min of A_hat, As_hat)
    UINT64 lastUpdateTimeKvs; //!< Time of last rate update
    DOUBLE avgMaxBitrate;     //!< EMA of max bitrate for convergence detection
    DOUBLE varMaxBitrate;     //!< Variance of max bitrate for convergence detection
    BOOL nearConvergence;     //!< TRUE if we're near max capacity
    UINT64 rttEstimateKvs;    //!< Estimated RTT in 100ns units
} GccRateController, *PGccRateController;

/**
 * Main GCC Controller structure (full definition for internal use)
 * The typedef GccController and PGccController are in the public header.
 */
struct __GccController {
    MUTEX lock; //!< Thread safety

    // Sub-components
    GccKalmanState kalman;      //!< Kalman filter state
    GccOveruseDetector overuse; //!< Over-use detector state
    GccRateController rateCtrl; //!< Rate controller state

    // Packet group tracking
    GccPacketGroup currentGroup; //!< Current packet group being built
    GccPacketGroup prevGroup;    //!< Previous completed group
    BOOL hasPrevGroup;           //!< TRUE if prevGroup is valid

    // Configuration
    UINT64 minBitrate;     //!< Minimum allowed bitrate (bps)
    UINT64 maxBitrate;     //!< Maximum allowed bitrate (bps)
    UINT64 initialBitrate; //!< Starting bitrate (bps)

    // Statistics
    UINT64 totalPacketsSent; //!< Total packets processed
    UINT64 totalPacketsLost; //!< Total packets marked as lost
    DOUBLE currentLossRatio; //!< Current loss ratio (0.0 - 1.0)
    UINT64 lastRttKvs;       //!< Last RTT observation
};

// NOTE: GccConfig is defined in the public header <com/amazonaws/kinesis/video/webrtcclient/Gcc.h>
// NOTE: Public API functions (createGccController, freeGccController, gccOnTwccPacketReports,
//       gccGetTargetBitrate, gccGetState, gccGetLossRatio) are declared in the public header

//
// Internal functions (exposed for unit testing)
//

/**
 * Initialize Kalman filter state
 */
STATUS gccKalmanInit(PGccKalmanState pKalman);

/**
 * Update Kalman filter with new inter-group delay variation
 *
 * @param[in,out] pKalman Kalman filter state
 * @param[in] d_i Inter-group delay variation (ms)
 * @return STATUS code
 */
STATUS gccKalmanUpdate(PGccKalmanState pKalman, DOUBLE d_i);

/**
 * Initialize over-use detector state
 */
STATUS gccOveruseDetectorInit(PGccOveruseDetector pOveruse);

/**
 * Detect over-use condition and update adaptive threshold
 *
 * @param[in,out] pOveruse Over-use detector state
 * @param[in] m_hat Current queuing delay estimate (ms)
 * @param[in] dtKvs Time since last update (100ns units)
 * @return GCC signal (NORMAL, OVERUSE, or UNDERUSE)
 */
GccSignal gccDetectOveruse(PGccOveruseDetector pOveruse, DOUBLE m_hat, UINT64 dtKvs);

/**
 * Initialize rate controller state
 */
STATUS gccRateControllerInit(PGccRateController pRateCtrl, UINT64 initialBitrate);

/**
 * Update delay-based rate controller
 *
 * @param[in,out] pRateCtrl Rate controller state
 * @param[in] signal Over-use signal
 * @param[in] dtKvs Time since last update (100ns units)
 * @param[in] incomingBitrate Current measured incoming bitrate
 * @param[in] minBitrate Minimum allowed bitrate
 * @param[in] maxBitrate Maximum allowed bitrate
 * @return STATUS code
 */
STATUS gccUpdateDelayController(PGccRateController pRateCtrl, GccSignal signal, UINT64 dtKvs, UINT64 incomingBitrate, UINT64 minBitrate,
                                UINT64 maxBitrate);

/**
 * Update loss-based rate controller
 *
 * @param[in,out] pRateCtrl Rate controller state
 * @param[in] lossRatio Current loss ratio (0.0 - 1.0)
 * @param[in] minBitrate Minimum allowed bitrate
 * @param[in] maxBitrate Maximum allowed bitrate
 * @return STATUS code
 */
STATUS gccUpdateLossController(PGccRateController pRateCtrl, DOUBLE lossRatio, UINT64 minBitrate, UINT64 maxBitrate);

/**
 * Process a single packet and add to current group if appropriate
 *
 * @param[in,out] pController GCC controller
 * @param[in] sendTimeKvs Packet send time
 * @param[in] arrivalTimeKvs Packet arrival time
 * @param[in] packetSize Packet size in bytes
 * @param[in] seqNum Sequence number
 * @return STATUS code
 */
STATUS gccProcessPacket(PGccController pController, UINT64 sendTimeKvs, UINT64 arrivalTimeKvs, UINT32 packetSize, UINT16 seqNum);

/**
 * Finalize current packet group and compute inter-group delay variation
 *
 * @param[in,out] pController GCC controller
 * @param[out] pD_i Inter-group delay variation in ms (only valid if return is TRUE)
 * @return TRUE if a new d(i) value was computed
 */
BOOL gccFinalizeGroup(PGccController pController, DOUBLE* pD_i);

#ifdef __cplusplus
}
#endif
#endif /* __KINESIS_VIDEO_WEBRTC_CLIENT_PEERCONNECTION_GCC_I__ */
