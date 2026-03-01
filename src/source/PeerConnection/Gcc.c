/*******************************************
GCC (Google Congestion Control) Implementation
Based on draft-ietf-rmcat-gcc-02
*******************************************/

#define LOG_CLASS "Gcc"
#include "../Include_i.h"

//
// Kalman Filter Implementation (RFC Section 5.3)
//

STATUS gccKalmanInit(PGccKalmanState pKalman)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pKalman != NULL, STATUS_NULL_ARG);

    pKalman->m_hat = 0.0;
    pKalman->e = GCC_INITIAL_ERROR_COV;
    pKalman->var_v_hat = 1.0; // Initial variance estimate
    pKalman->q = GCC_STATE_NOISE_COV;

CleanUp:
    return retStatus;
}

STATUS gccKalmanUpdate(PGccKalmanState pKalman, DOUBLE d_i)
{
    STATUS retStatus = STATUS_SUCCESS;
    DOUBLE z, z_clamped, threshold, k, alpha;

    CHK(pKalman != NULL, STATUS_NULL_ARG);

    // Innovation: z(i) = d(i) - m_hat(i-1)
    z = d_i - pKalman->m_hat;

    // Outlier filtering for variance estimate
    // If z(i) > 3*sqrt(var_v_hat), clamp it
    threshold = 3.0 * sqrt(pKalman->var_v_hat);
    z_clamped = z;
    if (fabs(z) > threshold) {
        z_clamped = (z > 0) ? threshold : -threshold;
    }

    // Update variance estimate using exponential moving average
    // alpha = (1 - chi)^(30 / (1000 * f_max))
    // For typical 30fps video, alpha ≈ 0.9999
    alpha = GCC_ALPHA_EMA;
    pKalman->var_v_hat = MAX(alpha * pKalman->var_v_hat + (1.0 - alpha) * z_clamped * z_clamped, 1.0);

    // Kalman gain: k(i) = (e(i-1) + q) / (var_v_hat + e(i-1) + q)
    k = (pKalman->e + pKalman->q) / (pKalman->var_v_hat + pKalman->e + pKalman->q);

    // State update: m_hat(i) = m_hat(i-1) + z(i) * k(i)
    pKalman->m_hat = pKalman->m_hat + z * k;

    // Error covariance update: e(i) = (1 - k(i)) * (e(i-1) + q)
    pKalman->e = (1.0 - k) * (pKalman->e + pKalman->q);

CleanUp:
    return retStatus;
}

//
// Over-use Detector Implementation (RFC Section 5.4)
//

STATUS gccOveruseDetectorInit(PGccOveruseDetector pOveruse)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pOveruse != NULL, STATUS_NULL_ARG);

    pOveruse->del_var_th = GCC_INITIAL_THRESHOLD_MS;
    pOveruse->overuseStartTimeKvs = 0;
    pOveruse->prevM_hat = 0.0;

CleanUp:
    return retStatus;
}

GccSignal gccDetectOveruse(PGccOveruseDetector pOveruse, DOUBLE m_hat, UINT64 dtKvs)
{
    GccSignal signal = GCC_SIGNAL_NORMAL;
    DOUBLE dt, K, absDiff;

    if (pOveruse == NULL) {
        return GCC_SIGNAL_NORMAL;
    }

    // Convert time delta to seconds
    dt = (DOUBLE) dtKvs / HUNDREDS_OF_NANOS_IN_A_SECOND;

    // Update adaptive threshold (RFC Section 5.4)
    // del_var_th(i) = del_var_th(i-1) + dt * K * (|m(i)| - del_var_th(i-1))
    K = (fabs(m_hat) < pOveruse->del_var_th) ? GCC_K_D : GCC_K_U;

    // Don't update if too far from threshold
    absDiff = fabs(m_hat) - pOveruse->del_var_th;
    if (absDiff <= GCC_THRESHOLD_UPDATE_LIMIT) {
        pOveruse->del_var_th = pOveruse->del_var_th + dt * K * (fabs(m_hat) - pOveruse->del_var_th);
        // Clamp threshold to valid range
        pOveruse->del_var_th = MAX(pOveruse->del_var_th, GCC_THRESHOLD_MIN_MS);
        pOveruse->del_var_th = MIN(pOveruse->del_var_th, GCC_THRESHOLD_MAX_MS);
    }

    // Detect over-use: m_hat > del_var_th for overuse_time_th
    if (m_hat > pOveruse->del_var_th) {
        if (pOveruse->overuseStartTimeKvs == 0) {
            pOveruse->overuseStartTimeKvs = GETTIME();
        } else {
            // Check if overuse has persisted for overuse_time_th
            UINT64 now = GETTIME();
            if ((now - pOveruse->overuseStartTimeKvs) >= GCC_MS_TO_KVS(GCC_OVERUSE_TIME_TH_MS)) {
                // But not if trend is decreasing (RFC: "if m(i) < m(i-1), over-use will not be signaled")
                if (m_hat >= pOveruse->prevM_hat) {
                    signal = GCC_SIGNAL_OVERUSE;
                }
            }
        }
    } else {
        pOveruse->overuseStartTimeKvs = 0;
    }

    // Detect under-use: m_hat < -del_var_th
    if (m_hat < -pOveruse->del_var_th) {
        signal = GCC_SIGNAL_UNDERUSE;
    }

    pOveruse->prevM_hat = m_hat;
    return signal;
}

//
// Rate Controller Implementation (RFC Section 5.5 and 6)
//

STATUS gccRateControllerInit(PGccRateController pRateCtrl, UINT64 initialBitrate)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pRateCtrl != NULL, STATUS_NULL_ARG);

    pRateCtrl->state = GCC_STATE_INCREASE;
    pRateCtrl->A_hat = initialBitrate;
    pRateCtrl->As_hat = initialBitrate;
    pRateCtrl->targetBitrate = initialBitrate;
    pRateCtrl->lastUpdateTimeKvs = GETTIME();
    pRateCtrl->avgMaxBitrate = 0.0;
    pRateCtrl->varMaxBitrate = 0.0;
    pRateCtrl->nearConvergence = FALSE;
    pRateCtrl->rttEstimateKvs = GCC_MS_TO_KVS(100); // Default 100ms RTT

CleanUp:
    return retStatus;
}

STATUS gccUpdateDelayController(PGccRateController pRateCtrl, GccSignal signal, UINT64 dtKvs, UINT64 incomingBitrate, UINT64 minBitrate,
                                UINT64 maxBitrate)
{
    STATUS retStatus = STATUS_SUCCESS;
    DOUBLE timeSinceLastUpdateSec, eta, responseTimeMs, alpha;
    UINT64 rttMs, bitsPerFrame, packetsPerFrame, avgPacketSizeBits;

    CHK(pRateCtrl != NULL, STATUS_NULL_ARG);

    timeSinceLastUpdateSec = (DOUBLE) dtKvs / HUNDREDS_OF_NANOS_IN_A_SECOND;

    // State transitions (RFC Section 5.5 table)
    switch (pRateCtrl->state) {
        case GCC_STATE_HOLD:
            if (signal == GCC_SIGNAL_OVERUSE) {
                pRateCtrl->state = GCC_STATE_DECREASE;
            } else if (signal == GCC_SIGNAL_NORMAL) {
                pRateCtrl->state = GCC_STATE_INCREASE;
            }
            break;
        case GCC_STATE_INCREASE:
            if (signal == GCC_SIGNAL_OVERUSE) {
                pRateCtrl->state = GCC_STATE_DECREASE;
            } else if (signal == GCC_SIGNAL_UNDERUSE) {
                pRateCtrl->state = GCC_STATE_HOLD;
            }
            break;
        case GCC_STATE_DECREASE:
            if (signal == GCC_SIGNAL_NORMAL) {
                pRateCtrl->state = GCC_STATE_HOLD;
            } else if (signal == GCC_SIGNAL_UNDERUSE) {
                pRateCtrl->state = GCC_STATE_HOLD;
            }
            break;
    }

    // Rate adjustment based on state
    switch (pRateCtrl->state) {
        case GCC_STATE_INCREASE:
            // Check if we're near convergence
            if (pRateCtrl->avgMaxBitrate > 0.0) {
                // Within 3 standard deviations of avg max bitrate
                DOUBLE stdDev = sqrt(pRateCtrl->varMaxBitrate);
                if (incomingBitrate > pRateCtrl->avgMaxBitrate + 3.0 * stdDev) {
                    // Congestion level changed, reset
                    pRateCtrl->avgMaxBitrate = 0.0;
                    pRateCtrl->varMaxBitrate = 0.0;
                    pRateCtrl->nearConvergence = FALSE;
                } else if (fabs((DOUBLE) incomingBitrate - pRateCtrl->avgMaxBitrate) < 3.0 * stdDev) {
                    pRateCtrl->nearConvergence = TRUE;
                }
            }

            if (pRateCtrl->nearConvergence) {
                // Additive increase: half a packet per response_time interval
                // response_time_ms = 100 + rtt_ms
                rttMs = pRateCtrl->rttEstimateKvs / HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
                responseTimeMs = 100.0 + (DOUBLE) rttMs;
                alpha = 0.5 * MIN(timeSinceLastUpdateSec * 1000.0 / responseTimeMs, 1.0);

                // expected_packet_size_bits calculation (assuming 30fps)
                bitsPerFrame = pRateCtrl->A_hat / 30;
                packetsPerFrame = (bitsPerFrame / (1200 * 8)) + 1;
                avgPacketSizeBits = bitsPerFrame / packetsPerFrame;

                pRateCtrl->A_hat += (UINT64) MAX(1000, alpha * avgPacketSizeBits);
            } else {
                // Multiplicative increase: at most 8% per second
                eta = pow(GCC_ETA_MULTIPLICATIVE, MIN(timeSinceLastUpdateSec, 1.0));
                pRateCtrl->A_hat = (UINT64) (eta * pRateCtrl->A_hat);
            }

            // Don't let estimate diverge too far from actual sending rate
            // A_hat(i) < 1.5 * R_hat(i)
            if (incomingBitrate > 0 && pRateCtrl->A_hat > (UINT64) (1.5 * incomingBitrate)) {
                pRateCtrl->A_hat = (UINT64) (1.5 * incomingBitrate);
            }
            break;

        case GCC_STATE_DECREASE:
            // Decrease to beta times the incoming rate
            if (incomingBitrate > 0) {
                pRateCtrl->A_hat = (UINT64) (GCC_BETA * incomingBitrate);
            } else {
                pRateCtrl->A_hat = (UINT64) (GCC_BETA * pRateCtrl->A_hat);
            }

            // Update convergence tracking
            if (pRateCtrl->avgMaxBitrate == 0.0) {
                pRateCtrl->avgMaxBitrate = (DOUBLE) pRateCtrl->A_hat;
                pRateCtrl->varMaxBitrate = 0.0;
            } else {
                // EMA update
                DOUBLE diff = (DOUBLE) pRateCtrl->A_hat - pRateCtrl->avgMaxBitrate;
                pRateCtrl->avgMaxBitrate =
                    GCC_CONVERGENCE_SMOOTHING * pRateCtrl->avgMaxBitrate + (1.0 - GCC_CONVERGENCE_SMOOTHING) * pRateCtrl->A_hat;
                pRateCtrl->varMaxBitrate = GCC_CONVERGENCE_SMOOTHING * pRateCtrl->varMaxBitrate + (1.0 - GCC_CONVERGENCE_SMOOTHING) * diff * diff;
            }
            break;

        case GCC_STATE_HOLD:
            // Keep rate steady
            break;
    }

    // Clamp to bounds
    pRateCtrl->A_hat = MAX(pRateCtrl->A_hat, minBitrate);
    pRateCtrl->A_hat = MIN(pRateCtrl->A_hat, maxBitrate);

    // Update target bitrate (minimum of delay-based and loss-based)
    pRateCtrl->targetBitrate = MIN(pRateCtrl->A_hat, pRateCtrl->As_hat);

CleanUp:
    return retStatus;
}

STATUS gccUpdateLossController(PGccRateController pRateCtrl, DOUBLE lossRatio, UINT64 minBitrate, UINT64 maxBitrate)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pRateCtrl != NULL, STATUS_NULL_ARG);

    // RFC Section 6: Loss-based control
    if (lossRatio > GCC_LOSS_THRESHOLD_HIGH) {
        // More than 10% loss: reduce by half the loss ratio
        pRateCtrl->As_hat = (UINT64) (pRateCtrl->As_hat * (1.0 - 0.5 * lossRatio));
    } else if (lossRatio > GCC_LOSS_THRESHOLD_LOW) {
        // 2-10% loss: hold steady
        // As_hat unchanged
    } else {
        // Less than 2% loss: increase by 5%
        pRateCtrl->As_hat = (UINT64) (pRateCtrl->As_hat * 1.05);
    }

    // Clamp to bounds
    pRateCtrl->As_hat = MAX(pRateCtrl->As_hat, minBitrate);
    pRateCtrl->As_hat = MIN(pRateCtrl->As_hat, maxBitrate);

    // Update target bitrate
    pRateCtrl->targetBitrate = MIN(pRateCtrl->A_hat, pRateCtrl->As_hat);

CleanUp:
    return retStatus;
}

//
// Packet Grouping / Pre-filtering (RFC Section 5.2)
//

STATUS gccProcessPacket(PGccController pController, UINT64 sendTimeKvs, UINT64 arrivalTimeKvs, UINT32 packetSize, UINT16 seqNum)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT64 interArrivalKvs;

    CHK(pController != NULL, STATUS_NULL_ARG);
    CHK(arrivalTimeKvs != 0 && arrivalTimeKvs != TWCC_PACKET_LOST_TIME, STATUS_SUCCESS); // Skip lost packets

    if (pController->currentGroup.packetCount == 0) {
        // First packet in group
        pController->currentGroup.departureTimeKvs = sendTimeKvs;
        pController->currentGroup.arrivalTimeKvs = arrivalTimeKvs;
        pController->currentGroup.totalBytes = packetSize;
        pController->currentGroup.firstSeqNum = seqNum;
        pController->currentGroup.packetCount = 1;
    } else {
        // Check if this packet belongs to the current group
        // Condition 1: packets sent within burst_time interval
        // Condition 2: inter-arrival time < burst_time AND d(i) < 0
        BOOL sameGroup = FALSE;

        if (sendTimeKvs <= pController->currentGroup.departureTimeKvs + GCC_MS_TO_KVS(GCC_BURST_TIME_MS)) {
            // Sent within burst_time of the group
            sameGroup = TRUE;
        } else {
            // Check if inter-arrival time < burst_time and d(i) < 0
            interArrivalKvs = arrivalTimeKvs - pController->currentGroup.arrivalTimeKvs;
            if (interArrivalKvs < GCC_MS_TO_KVS(GCC_BURST_TIME_MS)) {
                // Calculate instantaneous delay variation
                INT64 interDepartureKvs = (INT64) sendTimeKvs - (INT64) pController->currentGroup.departureTimeKvs;
                INT64 d_instant = (INT64) interArrivalKvs - interDepartureKvs;
                if (d_instant < 0) {
                    sameGroup = TRUE;
                }
            }
        }

        if (sameGroup) {
            // Add to current group
            pController->currentGroup.departureTimeKvs = sendTimeKvs;
            pController->currentGroup.arrivalTimeKvs = arrivalTimeKvs;
            pController->currentGroup.totalBytes += packetSize;
            pController->currentGroup.packetCount++;
        } else {
            // Finalize current group and start a new one
            pController->prevGroup = pController->currentGroup;
            pController->hasPrevGroup = TRUE;

            pController->currentGroup.departureTimeKvs = sendTimeKvs;
            pController->currentGroup.arrivalTimeKvs = arrivalTimeKvs;
            pController->currentGroup.totalBytes = packetSize;
            pController->currentGroup.firstSeqNum = seqNum;
            pController->currentGroup.packetCount = 1;
        }
    }

CleanUp:
    return retStatus;
}

BOOL gccFinalizeGroup(PGccController pController, DOUBLE* pD_i)
{
    INT64 interArrivalKvs, interDepartureKvs;

    if (pController == NULL || pD_i == NULL) {
        return FALSE;
    }

    if (!pController->hasPrevGroup || pController->currentGroup.packetCount == 0) {
        return FALSE;
    }

    // Calculate inter-group delay variation (RFC Section 5.1)
    // d(i) = t(i) - t(i-1) - (T(i) - T(i-1))
    interArrivalKvs = (INT64) pController->currentGroup.arrivalTimeKvs - (INT64) pController->prevGroup.arrivalTimeKvs;
    interDepartureKvs = (INT64) pController->currentGroup.departureTimeKvs - (INT64) pController->prevGroup.departureTimeKvs;

    // Convert to milliseconds
    *pD_i = GCC_KVS_TO_MS(interArrivalKvs - interDepartureKvs);

    // Prepare for next group
    pController->prevGroup = pController->currentGroup;
    MEMSET(&pController->currentGroup, 0, SIZEOF(GccPacketGroup));

    return TRUE;
}

//
// Public API Implementation
//

STATUS createGccController(PGccController* ppController, PGccConfig pConfig)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PGccController pController = NULL;

    CHK(ppController != NULL, STATUS_NULL_ARG);

    pController = (PGccController) MEMCALLOC(1, SIZEOF(GccController));
    CHK(pController != NULL, STATUS_NOT_ENOUGH_MEMORY);

    pController->lock = MUTEX_CREATE(FALSE);
    CHK(IS_VALID_MUTEX_VALUE(pController->lock), STATUS_INVALID_OPERATION);

    // Set configuration
    if (pConfig != NULL) {
        pController->minBitrate = pConfig->minBitrateBps > 0 ? pConfig->minBitrateBps : GCC_DEFAULT_MIN_BITRATE;
        pController->maxBitrate = pConfig->maxBitrateBps > 0 ? pConfig->maxBitrateBps : GCC_DEFAULT_MAX_BITRATE;
        pController->initialBitrate = pConfig->initialBitrateBps > 0 ? pConfig->initialBitrateBps : GCC_DEFAULT_INITIAL_BITRATE;
    } else {
        pController->minBitrate = GCC_DEFAULT_MIN_BITRATE;
        pController->maxBitrate = GCC_DEFAULT_MAX_BITRATE;
        pController->initialBitrate = GCC_DEFAULT_INITIAL_BITRATE;
    }

    // Initialize sub-components
    CHK_STATUS(gccKalmanInit(&pController->kalman));
    CHK_STATUS(gccOveruseDetectorInit(&pController->overuse));
    CHK_STATUS(gccRateControllerInit(&pController->rateCtrl, pController->initialBitrate));

    // Initialize packet group tracking
    MEMSET(&pController->currentGroup, 0, SIZEOF(GccPacketGroup));
    MEMSET(&pController->prevGroup, 0, SIZEOF(GccPacketGroup));
    pController->hasPrevGroup = FALSE;

    *ppController = pController;
    pController = NULL;

CleanUp:
    if (pController != NULL) {
        freeGccController(&pController);
    }

    LEAVES();
    return retStatus;
}

STATUS freeGccController(PGccController* ppController)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PGccController pController = NULL;

    CHK(ppController != NULL, STATUS_NULL_ARG);
    pController = *ppController;
    CHK(pController != NULL, retStatus);

    if (IS_VALID_MUTEX_VALUE(pController->lock)) {
        MUTEX_FREE(pController->lock);
    }

    SAFE_MEMFREE(pController);
    *ppController = NULL;

CleanUp:
    LEAVES();
    return retStatus;
}

STATUS gccOnTwccPacketReports(PGccController pController, PTwccPacketReport pReports, UINT32 reportCount, UINT64 rttEstimateKvs)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    BOOL locked = FALSE;
    UINT32 i;
    UINT64 receivedCount = 0, lostCount = 0;
    UINT64 totalBytes = 0, receivedBytes = 0;
    DOUBLE d_i = 0.0;
    GccSignal signal;
    UINT64 now, dtKvs;
    UINT64 incomingBitrate = 0;
    UINT64 duration = 0;

    CHK(pController != NULL && pReports != NULL && reportCount > 0, STATUS_NULL_ARG);

    MUTEX_LOCK(pController->lock);
    locked = TRUE;

    now = GETTIME();
    dtKvs = now - pController->rateCtrl.lastUpdateTimeKvs;
    pController->rateCtrl.lastUpdateTimeKvs = now;

    // Update RTT estimate if provided
    if (rttEstimateKvs > 0) {
        pController->rateCtrl.rttEstimateKvs = rttEstimateKvs;
        pController->lastRttKvs = rttEstimateKvs;
    }

    // Process all packets through the pre-filter/grouping
    for (i = 0; i < reportCount; i++) {
        totalBytes += pReports[i].packetSize;
        pController->totalPacketsSent++;

        if (pReports[i].received) {
            receivedCount++;
            receivedBytes += pReports[i].packetSize;

            // Process packet for grouping
            gccProcessPacket(pController, pReports[i].sendTimeKvs, pReports[i].arrivalTimeKvs, pReports[i].packetSize, pReports[i].seqNum);
        } else {
            lostCount++;
            pController->totalPacketsLost++;
        }
    }

    // Update loss ratio
    if (receivedCount + lostCount > 0) {
        pController->currentLossRatio = (DOUBLE) lostCount / (DOUBLE) (receivedCount + lostCount);
    }

    // Calculate incoming bitrate from received packets
    // Find duration from first to last received packet
    UINT64 firstSendTime = 0, lastSendTime = 0;
    for (i = 0; i < reportCount; i++) {
        if (pReports[i].received) {
            if (firstSendTime == 0 || pReports[i].sendTimeKvs < firstSendTime) {
                firstSendTime = pReports[i].sendTimeKvs;
            }
            if (pReports[i].sendTimeKvs > lastSendTime) {
                lastSendTime = pReports[i].sendTimeKvs;
            }
        }
    }
    if (lastSendTime > firstSendTime) {
        duration = lastSendTime - firstSendTime;
        // Convert to bps: bytes * 8 / seconds
        incomingBitrate = (UINT64) ((DOUBLE) receivedBytes * 8.0 * HUNDREDS_OF_NANOS_IN_A_SECOND / (DOUBLE) duration);
    }

    // Finalize current group and get inter-group delay variation
    while (gccFinalizeGroup(pController, &d_i)) {
        // Update Kalman filter with the inter-group delay variation
        CHK_STATUS(gccKalmanUpdate(&pController->kalman, d_i));

        // Run over-use detector
        signal = gccDetectOveruse(&pController->overuse, pController->kalman.m_hat, dtKvs);

        // Update delay-based controller
        CHK_STATUS(
            gccUpdateDelayController(&pController->rateCtrl, signal, dtKvs, incomingBitrate, pController->minBitrate, pController->maxBitrate));
    }

    // Update loss-based controller
    CHK_STATUS(gccUpdateLossController(&pController->rateCtrl, pController->currentLossRatio, pController->minBitrate, pController->maxBitrate));

CleanUp:
    if (locked) {
        MUTEX_UNLOCK(pController->lock);
    }

    LEAVES();
    return retStatus;
}

UINT64 gccGetTargetBitrate(PGccController pController)
{
    UINT64 targetBitrate = 0;

    if (pController == NULL) {
        return 0;
    }

    MUTEX_LOCK(pController->lock);
    targetBitrate = pController->rateCtrl.targetBitrate;
    MUTEX_UNLOCK(pController->lock);

    return targetBitrate;
}

GccState gccGetState(PGccController pController)
{
    GccState state = GCC_STATE_HOLD;

    if (pController == NULL) {
        return GCC_STATE_HOLD;
    }

    MUTEX_LOCK(pController->lock);
    state = pController->rateCtrl.state;
    MUTEX_UNLOCK(pController->lock);

    return state;
}

DOUBLE gccGetLossRatio(PGccController pController)
{
    DOUBLE lossRatio = 0.0;

    if (pController == NULL) {
        return 0.0;
    }

    MUTEX_LOCK(pController->lock);
    lossRatio = pController->currentLossRatio;
    MUTEX_UNLOCK(pController->lock);

    return lossRatio;
}
