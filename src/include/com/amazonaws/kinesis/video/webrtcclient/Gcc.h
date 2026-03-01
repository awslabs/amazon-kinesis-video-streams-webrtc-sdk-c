/*
 * GCC (Google Congestion Control) public include file
 */
#ifndef __KINESIS_VIDEO_WEBRTCCLIENT_GCC_INCLUDE__
#define __KINESIS_VIDEO_WEBRTCCLIENT_GCC_INCLUDE__

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief GCC Rate Controller States
 */
typedef enum {
    GCC_STATE_HOLD,     //!< Wait for queues to stabilize
    GCC_STATE_INCREASE, //!< Increase rate (no congestion detected)
    GCC_STATE_DECREASE  //!< Decrease rate (congestion detected)
} GccState;

/**
 * @brief Forward declaration - opaque pointer for external use
 */
typedef struct __GccController GccController;
typedef GccController* PGccController;

/**
 * @brief GCC Configuration structure
 */
typedef struct {
    UINT64 minBitrateBps;     //!< Minimum bitrate (default: 100 kbps)
    UINT64 maxBitrateBps;     //!< Maximum bitrate (default: 2.5 Mbps)
    UINT64 initialBitrateBps; //!< Initial bitrate (default: 300 kbps)
} GccConfig, *PGccConfig;

/**
 * @brief Create a new GCC controller
 *
 * @param[out] ppController Pointer to receive the new controller
 * @param[in] pConfig Optional configuration (NULL for defaults)
 * @return STATUS code
 */
PUBLIC_API STATUS createGccController(PGccController* ppController, PGccConfig pConfig);

/**
 * @brief Free a GCC controller
 *
 * @param[in,out] ppController Pointer to controller to free
 * @return STATUS code
 */
PUBLIC_API STATUS freeGccController(PGccController* ppController);

/**
 * @brief Process TWCC packet reports from the callback
 *
 * This is the main entry point - call this from your RtcOnTwccPacketReport callback
 *
 * @param[in] pController GCC controller
 * @param[in] pReports Array of packet reports
 * @param[in] reportCount Number of reports
 * @param[in] rttEstimateKvs RTT estimate (0 if unknown)
 * @return STATUS code
 */
PUBLIC_API STATUS gccOnTwccPacketReports(PGccController pController, PTwccPacketReport pReports, UINT32 reportCount, UINT64 rttEstimateKvs);

/**
 * @brief Get the current target bitrate
 *
 * @param[in] pController GCC controller
 * @return Target bitrate in bits per second
 */
PUBLIC_API UINT64 gccGetTargetBitrate(PGccController pController);

/**
 * @brief Get the current GCC state for debugging
 *
 * @param[in] pController GCC controller
 * @return Current state (GCC_STATE_HOLD, GCC_STATE_INCREASE, or GCC_STATE_DECREASE)
 */
PUBLIC_API GccState gccGetState(PGccController pController);

/**
 * @brief Get the current loss ratio
 *
 * @param[in] pController GCC controller
 * @return Loss ratio (0.0 - 1.0)
 */
PUBLIC_API DOUBLE gccGetLossRatio(PGccController pController);

#ifdef __cplusplus
}
#endif
#endif /* __KINESIS_VIDEO_WEBRTCCLIENT_GCC_INCLUDE__ */
