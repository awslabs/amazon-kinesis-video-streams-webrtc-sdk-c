/*******************************************
GCC (Google Congestion Control) Unit Tests
Based on draft-ietf-rmcat-gcc-02
*******************************************/

#include "WebRTCClientTestFixture.h"

namespace com {
namespace amazonaws {
namespace kinesis {
namespace video {
namespace webrtcclient {

class GccFunctionalityTest : public WebRtcClientTestBase {
  public:
    PGccController pController = nullptr;

    void SetUp() override
    {
        WebRtcClientTestBase::SetUp();
    }

    void TearDown() override
    {
        if (pController != nullptr) {
            freeGccController(&pController);
        }
        WebRtcClientTestBase::TearDown();
    }

    // Helper to create mock TwccPacketReports
    void createMockReports(TwccPacketReport* pReports, UINT32 count, UINT64 baseTimeSendKvs, UINT64 baseTimeArriveKvs, UINT32 packetSize,
                           DOUBLE delayVariationMs, DOUBLE lossRatio)
    {
        UINT64 sendInterval = 33 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND; // ~30fps
        UINT64 arrivalDrift = (UINT64)(delayVariationMs * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);

        for (UINT32 i = 0; i < count; i++) {
            pReports[i].seqNum = (UINT16) i;
            pReports[i].sendTimeKvs = baseTimeSendKvs + i * sendInterval;
            pReports[i].packetSize = packetSize;

            // Simulate loss based on ratio
            if (lossRatio > 0 && ((DOUBLE) rand() / RAND_MAX) < lossRatio) {
                pReports[i].received = FALSE;
                pReports[i].arrivalTimeKvs = 0;
            } else {
                pReports[i].received = TRUE;
                // Add progressive delay drift to simulate congestion
                pReports[i].arrivalTimeKvs = baseTimeArriveKvs + i * sendInterval + (i * arrivalDrift);
            }
        }
    }
};

//
// Kalman Filter Tests
//

TEST_F(GccFunctionalityTest, kalmanInit)
{
    GccKalmanState kalman;

    EXPECT_EQ(STATUS_SUCCESS, gccKalmanInit(&kalman));

    EXPECT_DOUBLE_EQ(0.0, kalman.m_hat);
    EXPECT_DOUBLE_EQ(GCC_INITIAL_ERROR_COV, kalman.e);
    EXPECT_DOUBLE_EQ(1.0, kalman.var_v_hat);
    EXPECT_DOUBLE_EQ(GCC_STATE_NOISE_COV, kalman.q);
}

TEST_F(GccFunctionalityTest, kalmanInitNullArg)
{
    EXPECT_EQ(STATUS_NULL_ARG, gccKalmanInit(nullptr));
}

TEST_F(GccFunctionalityTest, kalmanUpdateBasic)
{
    GccKalmanState kalman;

    EXPECT_EQ(STATUS_SUCCESS, gccKalmanInit(&kalman));

    // Initial m_hat is 0, if we observe d_i = 5ms, m_hat should move toward 5
    EXPECT_EQ(STATUS_SUCCESS, gccKalmanUpdate(&kalman, 5.0));
    EXPECT_GT(kalman.m_hat, 0.0);
    EXPECT_LT(kalman.m_hat, 5.0); // Should not fully jump to 5
}

TEST_F(GccFunctionalityTest, kalmanConvergence)
{
    GccKalmanState kalman;

    EXPECT_EQ(STATUS_SUCCESS, gccKalmanInit(&kalman));

    // Feed the same value multiple times - m_hat should converge toward it
    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(STATUS_SUCCESS, gccKalmanUpdate(&kalman, 10.0));
    }

    // Should be close to 10 after many iterations
    EXPECT_GT(kalman.m_hat, 9.0);
    EXPECT_LT(kalman.m_hat, 11.0);
}

TEST_F(GccFunctionalityTest, kalmanOutlierFiltering)
{
    GccKalmanState kalman;

    EXPECT_EQ(STATUS_SUCCESS, gccKalmanInit(&kalman));

    // First, establish a baseline
    for (int i = 0; i < 20; i++) {
        EXPECT_EQ(STATUS_SUCCESS, gccKalmanUpdate(&kalman, 5.0));
    }

    DOUBLE m_hat_before = kalman.m_hat;

    // Now inject a large outlier
    EXPECT_EQ(STATUS_SUCCESS, gccKalmanUpdate(&kalman, 500.0));

    // m_hat should not jump dramatically due to outlier filtering in variance
    EXPECT_LT(kalman.m_hat, m_hat_before + 50.0); // Should be damped
}

TEST_F(GccFunctionalityTest, kalmanVarianceUpdate)
{
    GccKalmanState kalman;

    EXPECT_EQ(STATUS_SUCCESS, gccKalmanInit(&kalman));

    DOUBLE initialVar = kalman.var_v_hat;

    // Feed noisy data - variance should update
    for (int i = 0; i < 50; i++) {
        DOUBLE noise = (i % 2 == 0) ? 10.0 : -10.0;
        EXPECT_EQ(STATUS_SUCCESS, gccKalmanUpdate(&kalman, noise));
    }

    // Variance should have changed from initial
    EXPECT_NE(kalman.var_v_hat, initialVar);
    EXPECT_GE(kalman.var_v_hat, 1.0); // Should be clamped to minimum of 1.0
}

//
// Over-use Detector Tests
//

TEST_F(GccFunctionalityTest, overuseDetectorInit)
{
    GccOveruseDetector overuse;

    EXPECT_EQ(STATUS_SUCCESS, gccOveruseDetectorInit(&overuse));

    EXPECT_DOUBLE_EQ(GCC_INITIAL_THRESHOLD_MS, overuse.del_var_th);
    EXPECT_EQ(0ULL, overuse.overuseStartTimeKvs);
    EXPECT_DOUBLE_EQ(0.0, overuse.prevM_hat);
}

TEST_F(GccFunctionalityTest, overuseDetectorInitNullArg)
{
    EXPECT_EQ(STATUS_NULL_ARG, gccOveruseDetectorInit(nullptr));
}

TEST_F(GccFunctionalityTest, overuseDetectorNormalSignal)
{
    GccOveruseDetector overuse;
    EXPECT_EQ(STATUS_SUCCESS, gccOveruseDetectorInit(&overuse));

    // m_hat within threshold should give NORMAL signal
    GccSignal signal = gccDetectOveruse(&overuse, 5.0, HUNDREDS_OF_NANOS_IN_A_SECOND);

    EXPECT_EQ(GCC_SIGNAL_NORMAL, signal);
}

TEST_F(GccFunctionalityTest, overuseDetectorUnderuseSignal)
{
    GccOveruseDetector overuse;
    EXPECT_EQ(STATUS_SUCCESS, gccOveruseDetectorInit(&overuse));

    // m_hat below -threshold should give UNDERUSE signal
    GccSignal signal = gccDetectOveruse(&overuse, -20.0, HUNDREDS_OF_NANOS_IN_A_SECOND);

    EXPECT_EQ(GCC_SIGNAL_UNDERUSE, signal);
}

TEST_F(GccFunctionalityTest, overuseDetectorOveruseSignalRequiresTime)
{
    GccOveruseDetector overuse;
    EXPECT_EQ(STATUS_SUCCESS, gccOveruseDetectorInit(&overuse));

    // m_hat above threshold once should NOT immediately give OVERUSE
    // because it needs to persist for overuse_time_th
    GccSignal signal = gccDetectOveruse(&overuse, 20.0, HUNDREDS_OF_NANOS_IN_A_MILLISECOND);

    // Should start tracking overuse but not signal yet
    EXPECT_NE(0ULL, overuse.overuseStartTimeKvs);
    EXPECT_EQ(GCC_SIGNAL_NORMAL, signal); // Not enough time elapsed
}

TEST_F(GccFunctionalityTest, overuseDetectorThresholdAdaptation)
{
    GccOveruseDetector overuse;
    EXPECT_EQ(STATUS_SUCCESS, gccOveruseDetectorInit(&overuse));

    DOUBLE initialThreshold = overuse.del_var_th;

    // Feed values below threshold - should decrease threshold (slowly)
    for (int i = 0; i < 100; i++) {
        gccDetectOveruse(&overuse, 2.0, HUNDREDS_OF_NANOS_IN_A_SECOND);
    }

    EXPECT_LT(overuse.del_var_th, initialThreshold);
}

TEST_F(GccFunctionalityTest, overuseDetectorThresholdClamping)
{
    GccOveruseDetector overuse;
    EXPECT_EQ(STATUS_SUCCESS, gccOveruseDetectorInit(&overuse));

    // Try to push threshold below minimum
    for (int i = 0; i < 1000; i++) {
        gccDetectOveruse(&overuse, 0.1, HUNDREDS_OF_NANOS_IN_A_SECOND);
    }

    EXPECT_GE(overuse.del_var_th, GCC_THRESHOLD_MIN_MS);

    // Reset and try to push above maximum
    EXPECT_EQ(STATUS_SUCCESS, gccOveruseDetectorInit(&overuse));
    for (int i = 0; i < 1000; i++) {
        gccDetectOveruse(&overuse, 100.0, HUNDREDS_OF_NANOS_IN_A_SECOND);
    }

    EXPECT_LE(overuse.del_var_th, GCC_THRESHOLD_MAX_MS);
}

TEST_F(GccFunctionalityTest, overuseDetectorNullReturnsNormal)
{
    GccSignal signal = gccDetectOveruse(nullptr, 10.0, HUNDREDS_OF_NANOS_IN_A_SECOND);
    EXPECT_EQ(GCC_SIGNAL_NORMAL, signal);
}

//
// Rate Controller Tests
//

TEST_F(GccFunctionalityTest, rateControllerInit)
{
    GccRateController rateCtrl;

    EXPECT_EQ(STATUS_SUCCESS, gccRateControllerInit(&rateCtrl, 500000));

    EXPECT_EQ(GCC_STATE_INCREASE, rateCtrl.state);
    EXPECT_EQ(500000ULL, rateCtrl.A_hat);
    EXPECT_EQ(500000ULL, rateCtrl.As_hat);
    EXPECT_EQ(500000ULL, rateCtrl.targetBitrate);
    EXPECT_FALSE(rateCtrl.nearConvergence);
}

TEST_F(GccFunctionalityTest, rateControllerInitNullArg)
{
    EXPECT_EQ(STATUS_NULL_ARG, gccRateControllerInit(nullptr, 500000));
}

TEST_F(GccFunctionalityTest, rateControllerStateTransitions)
{
    GccRateController rateCtrl;
    EXPECT_EQ(STATUS_SUCCESS, gccRateControllerInit(&rateCtrl, 500000));

    // Initial state is INCREASE
    EXPECT_EQ(GCC_STATE_INCREASE, rateCtrl.state);

    // INCREASE + OVERUSE -> DECREASE
    gccUpdateDelayController(&rateCtrl, GCC_SIGNAL_OVERUSE, HUNDREDS_OF_NANOS_IN_A_SECOND, 500000, GCC_DEFAULT_MIN_BITRATE, GCC_DEFAULT_MAX_BITRATE);
    EXPECT_EQ(GCC_STATE_DECREASE, rateCtrl.state);

    // DECREASE + NORMAL -> HOLD
    gccUpdateDelayController(&rateCtrl, GCC_SIGNAL_NORMAL, HUNDREDS_OF_NANOS_IN_A_SECOND, 500000, GCC_DEFAULT_MIN_BITRATE, GCC_DEFAULT_MAX_BITRATE);
    EXPECT_EQ(GCC_STATE_HOLD, rateCtrl.state);

    // HOLD + NORMAL -> INCREASE
    gccUpdateDelayController(&rateCtrl, GCC_SIGNAL_NORMAL, HUNDREDS_OF_NANOS_IN_A_SECOND, 500000, GCC_DEFAULT_MIN_BITRATE, GCC_DEFAULT_MAX_BITRATE);
    EXPECT_EQ(GCC_STATE_INCREASE, rateCtrl.state);

    // INCREASE + UNDERUSE -> HOLD
    gccUpdateDelayController(&rateCtrl, GCC_SIGNAL_UNDERUSE, HUNDREDS_OF_NANOS_IN_A_SECOND, 500000, GCC_DEFAULT_MIN_BITRATE, GCC_DEFAULT_MAX_BITRATE);
    EXPECT_EQ(GCC_STATE_HOLD, rateCtrl.state);
}

TEST_F(GccFunctionalityTest, rateControllerIncreaseBehavior)
{
    GccRateController rateCtrl;
    EXPECT_EQ(STATUS_SUCCESS, gccRateControllerInit(&rateCtrl, 500000));

    UINT64 initialRate = rateCtrl.A_hat;

    // In INCREASE state with NORMAL signal, rate should increase
    gccUpdateDelayController(&rateCtrl, GCC_SIGNAL_NORMAL, HUNDREDS_OF_NANOS_IN_A_SECOND, 500000, GCC_DEFAULT_MIN_BITRATE, GCC_DEFAULT_MAX_BITRATE);

    EXPECT_GT(rateCtrl.A_hat, initialRate);
}

TEST_F(GccFunctionalityTest, rateControllerDecreaseBehavior)
{
    GccRateController rateCtrl;
    EXPECT_EQ(STATUS_SUCCESS, gccRateControllerInit(&rateCtrl, 500000));

    // Trigger DECREASE state
    gccUpdateDelayController(&rateCtrl, GCC_SIGNAL_OVERUSE, HUNDREDS_OF_NANOS_IN_A_SECOND, 500000, GCC_DEFAULT_MIN_BITRATE, GCC_DEFAULT_MAX_BITRATE);

    EXPECT_EQ(GCC_STATE_DECREASE, rateCtrl.state);

    // In DECREASE state, rate should be reduced to beta * incomingBitrate
    EXPECT_LT(rateCtrl.A_hat, 500000ULL);
    EXPECT_EQ((UINT64)(GCC_BETA * 500000), rateCtrl.A_hat);
}

TEST_F(GccFunctionalityTest, rateControllerBoundsEnforcement)
{
    GccRateController rateCtrl;
    EXPECT_EQ(STATUS_SUCCESS, gccRateControllerInit(&rateCtrl, 500000));

    // Try to increase beyond max
    for (int i = 0; i < 100; i++) {
        gccUpdateDelayController(&rateCtrl, GCC_SIGNAL_NORMAL, HUNDREDS_OF_NANOS_IN_A_SECOND, GCC_DEFAULT_MAX_BITRATE, GCC_DEFAULT_MIN_BITRATE,
                                 GCC_DEFAULT_MAX_BITRATE);
    }

    EXPECT_LE(rateCtrl.A_hat, GCC_DEFAULT_MAX_BITRATE);

    // Try to decrease below min
    EXPECT_EQ(STATUS_SUCCESS, gccRateControllerInit(&rateCtrl, 500000));
    for (int i = 0; i < 100; i++) {
        gccUpdateDelayController(&rateCtrl, GCC_SIGNAL_OVERUSE, HUNDREDS_OF_NANOS_IN_A_SECOND, GCC_DEFAULT_MIN_BITRATE, GCC_DEFAULT_MIN_BITRATE,
                                 GCC_DEFAULT_MAX_BITRATE);
    }

    EXPECT_GE(rateCtrl.A_hat, GCC_DEFAULT_MIN_BITRATE);
}

TEST_F(GccFunctionalityTest, rateControllerConvergenceTracking)
{
    GccRateController rateCtrl;
    EXPECT_EQ(STATUS_SUCCESS, gccRateControllerInit(&rateCtrl, 1000000));

    // Initially not near convergence
    EXPECT_FALSE(rateCtrl.nearConvergence);

    // Multiple decrease events should establish avgMaxBitrate
    gccUpdateDelayController(&rateCtrl, GCC_SIGNAL_OVERUSE, HUNDREDS_OF_NANOS_IN_A_SECOND, 1000000, GCC_DEFAULT_MIN_BITRATE, GCC_DEFAULT_MAX_BITRATE);

    EXPECT_GT(rateCtrl.avgMaxBitrate, 0.0);
}

//
// Loss Controller Tests
//

TEST_F(GccFunctionalityTest, lossControllerLowLossIncrease)
{
    GccRateController rateCtrl;
    EXPECT_EQ(STATUS_SUCCESS, gccRateControllerInit(&rateCtrl, 500000));

    UINT64 initialAsHat = rateCtrl.As_hat;

    // Less than 2% loss should increase rate by 5%
    gccUpdateLossController(&rateCtrl, 0.01, GCC_DEFAULT_MIN_BITRATE, GCC_DEFAULT_MAX_BITRATE);

    EXPECT_GT(rateCtrl.As_hat, initialAsHat);
    EXPECT_EQ((UINT64)(initialAsHat * 1.05), rateCtrl.As_hat);
}

TEST_F(GccFunctionalityTest, lossControllerMidLossHold)
{
    GccRateController rateCtrl;
    EXPECT_EQ(STATUS_SUCCESS, gccRateControllerInit(&rateCtrl, 500000));

    UINT64 initialAsHat = rateCtrl.As_hat;

    // 2-10% loss should hold steady
    gccUpdateLossController(&rateCtrl, 0.05, GCC_DEFAULT_MIN_BITRATE, GCC_DEFAULT_MAX_BITRATE);

    EXPECT_EQ(initialAsHat, rateCtrl.As_hat);
}

TEST_F(GccFunctionalityTest, lossControllerHighLossDecrease)
{
    GccRateController rateCtrl;
    EXPECT_EQ(STATUS_SUCCESS, gccRateControllerInit(&rateCtrl, 500000));

    UINT64 initialAsHat = rateCtrl.As_hat;

    // More than 10% loss should decrease by half the loss ratio
    gccUpdateLossController(&rateCtrl, 0.20, GCC_DEFAULT_MIN_BITRATE, GCC_DEFAULT_MAX_BITRATE);

    EXPECT_LT(rateCtrl.As_hat, initialAsHat);
    EXPECT_EQ((UINT64)(initialAsHat * (1.0 - 0.5 * 0.20)), rateCtrl.As_hat);
}

TEST_F(GccFunctionalityTest, lossControllerBoundsEnforcement)
{
    GccRateController rateCtrl;
    EXPECT_EQ(STATUS_SUCCESS, gccRateControllerInit(&rateCtrl, 500000));

    // Try to increase beyond max
    for (int i = 0; i < 200; i++) {
        gccUpdateLossController(&rateCtrl, 0.0, GCC_DEFAULT_MIN_BITRATE, GCC_DEFAULT_MAX_BITRATE);
    }

    EXPECT_LE(rateCtrl.As_hat, GCC_DEFAULT_MAX_BITRATE);

    // Try to decrease below min
    EXPECT_EQ(STATUS_SUCCESS, gccRateControllerInit(&rateCtrl, 500000));
    for (int i = 0; i < 50; i++) {
        gccUpdateLossController(&rateCtrl, 0.50, GCC_DEFAULT_MIN_BITRATE, GCC_DEFAULT_MAX_BITRATE);
    }

    EXPECT_GE(rateCtrl.As_hat, GCC_DEFAULT_MIN_BITRATE);
}

//
// Packet Grouping / Pre-filtering Tests
//

TEST_F(GccFunctionalityTest, packetGroupingBasic)
{
    EXPECT_EQ(STATUS_SUCCESS, createGccController(&pController, nullptr));

    UINT64 baseTime = GETTIME();

    // First packet
    EXPECT_EQ(STATUS_SUCCESS, gccProcessPacket(pController, baseTime, baseTime + 50 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND, 1200, 0));

    EXPECT_EQ(1, pController->currentGroup.packetCount);
    EXPECT_EQ(1200U, pController->currentGroup.totalBytes);
    EXPECT_EQ(0, pController->currentGroup.firstSeqNum);
}

TEST_F(GccFunctionalityTest, packetGroupingBurstTime)
{
    EXPECT_EQ(STATUS_SUCCESS, createGccController(&pController, nullptr));

    UINT64 baseTime = GETTIME();

    // First packet
    gccProcessPacket(pController, baseTime, baseTime + 50 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND, 1200, 0);

    // Second packet within burst time (5ms)
    gccProcessPacket(pController, baseTime + 2 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND, baseTime + 52 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND, 1200, 1);

    // Should be in same group
    EXPECT_EQ(2, pController->currentGroup.packetCount);
    EXPECT_EQ(2400U, pController->currentGroup.totalBytes);
}

TEST_F(GccFunctionalityTest, packetGroupingNewGroup)
{
    EXPECT_EQ(STATUS_SUCCESS, createGccController(&pController, nullptr));

    UINT64 baseTime = GETTIME();

    // First packet
    gccProcessPacket(pController, baseTime, baseTime + 50 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND, 1200, 0);

    // Second packet outside burst time (>5ms)
    gccProcessPacket(pController, baseTime + 33 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND, baseTime + 83 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND, 1200, 1);

    // Should start new group
    EXPECT_EQ(1, pController->currentGroup.packetCount);
    EXPECT_TRUE(pController->hasPrevGroup);
    EXPECT_EQ(1, pController->prevGroup.packetCount);
}

TEST_F(GccFunctionalityTest, packetGroupingFinalizeGroup)
{
    EXPECT_EQ(STATUS_SUCCESS, createGccController(&pController, nullptr));

    UINT64 baseTime = GETTIME();

    // First group
    gccProcessPacket(pController, baseTime, baseTime + 50 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND, 1200, 0);

    // Second group (different time)
    gccProcessPacket(pController, baseTime + 33 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND, baseTime + 83 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND, 1200, 1);

    // Third group to trigger finalization of second
    gccProcessPacket(pController, baseTime + 66 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND, baseTime + 116 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND, 1200, 2);

    DOUBLE d_i = 0.0;
    BOOL hasValue = gccFinalizeGroup(pController, &d_i);

    EXPECT_TRUE(hasValue);
    // d_i should be close to 0 if delays are consistent
}

TEST_F(GccFunctionalityTest, packetGroupingSkipsLostPackets)
{
    EXPECT_EQ(STATUS_SUCCESS, createGccController(&pController, nullptr));

    UINT64 baseTime = GETTIME();

    // First packet received
    gccProcessPacket(pController, baseTime, baseTime + 50 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND, 1200, 0);

    // Lost packet (arrivalTime = 0)
    gccProcessPacket(pController, baseTime + 33 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND, 0, 1200, 1);

    // Should still have only 1 packet
    EXPECT_EQ(1, pController->currentGroup.packetCount);
}

//
// GCC Controller Integration Tests
//

TEST_F(GccFunctionalityTest, createFreeLifecycle)
{
    // Create with defaults
    EXPECT_EQ(STATUS_SUCCESS, createGccController(&pController, nullptr));

    EXPECT_NE(nullptr, pController);
    EXPECT_EQ(GCC_DEFAULT_MIN_BITRATE, pController->minBitrate);
    EXPECT_EQ(GCC_DEFAULT_MAX_BITRATE, pController->maxBitrate);
    EXPECT_EQ(GCC_DEFAULT_INITIAL_BITRATE, pController->initialBitrate);

    // Verify initial state
    EXPECT_EQ(GCC_STATE_INCREASE, pController->rateCtrl.state);
    EXPECT_EQ(GCC_DEFAULT_INITIAL_BITRATE, pController->rateCtrl.targetBitrate);

    // Free
    EXPECT_EQ(STATUS_SUCCESS, freeGccController(&pController));
    EXPECT_EQ(nullptr, pController);

    // Free again should be idempotent
    EXPECT_EQ(STATUS_SUCCESS, freeGccController(&pController));
}

TEST_F(GccFunctionalityTest, createWithConfig)
{
    GccConfig config;
    config.minBitrateBps = 200000;
    config.maxBitrateBps = 5000000;
    config.initialBitrateBps = 1000000;

    EXPECT_EQ(STATUS_SUCCESS, createGccController(&pController, &config));

    EXPECT_EQ(200000ULL, pController->minBitrate);
    EXPECT_EQ(5000000ULL, pController->maxBitrate);
    EXPECT_EQ(1000000ULL, pController->initialBitrate);
    EXPECT_EQ(1000000ULL, pController->rateCtrl.targetBitrate);
}

TEST_F(GccFunctionalityTest, createNullArg)
{
    EXPECT_EQ(STATUS_NULL_ARG, createGccController(nullptr, nullptr));
}

TEST_F(GccFunctionalityTest, fullPipelineWithReports)
{
    EXPECT_EQ(STATUS_SUCCESS, createGccController(&pController, nullptr));

    static constexpr UINT32 reportCount = 30;
    TwccPacketReport reports[reportCount];
    UINT64 baseTime = GETTIME();

    // Create reports with no delay variation and no loss
    createMockReports(reports, reportCount, baseTime, baseTime + 50 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND, 1200, 0.0, 0.0);

    EXPECT_EQ(STATUS_SUCCESS, gccOnTwccPacketReports(pController, reports, reportCount, 100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND));

    // With no congestion, rate should stay or increase
    UINT64 targetBitrate = gccGetTargetBitrate(pController);
    EXPECT_GE(targetBitrate, GCC_DEFAULT_INITIAL_BITRATE);

    // Check loss ratio should be 0
    DOUBLE lossRatio = gccGetLossRatio(pController);
    EXPECT_DOUBLE_EQ(0.0, lossRatio);

    // State should be INCREASE or HOLD
    GccState state = gccGetState(pController);
    EXPECT_TRUE(state == GCC_STATE_INCREASE || state == GCC_STATE_HOLD);
}

TEST_F(GccFunctionalityTest, pipelineWithPacketLoss)
{
    EXPECT_EQ(STATUS_SUCCESS, createGccController(&pController, nullptr));

    static constexpr UINT32 reportCount = 100;
    TwccPacketReport reports[reportCount];
    UINT64 baseTime = GETTIME();

    // Seed random for reproducible tests
    srand(42);

    // Create reports with 15% loss (high loss)
    createMockReports(reports, reportCount, baseTime, baseTime + 50 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND, 1200, 0.0, 0.15);

    EXPECT_EQ(STATUS_SUCCESS, gccOnTwccPacketReports(pController, reports, reportCount, 100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND));

    // With high loss, loss ratio should be detected
    DOUBLE lossRatio = gccGetLossRatio(pController);
    EXPECT_GT(lossRatio, 0.0);

    // Target bitrate might be reduced due to loss
    UINT64 targetBitrate = gccGetTargetBitrate(pController);
    EXPECT_GE(targetBitrate, GCC_DEFAULT_MIN_BITRATE);
    EXPECT_LE(targetBitrate, GCC_DEFAULT_MAX_BITRATE);
}

TEST_F(GccFunctionalityTest, pipelineWithDelayVariation)
{
    EXPECT_EQ(STATUS_SUCCESS, createGccController(&pController, nullptr));

    static constexpr UINT32 reportCount = 50;
    TwccPacketReport reports[reportCount];
    UINT64 baseTime = GETTIME();

    // Create reports with increasing delay (simulating congestion buildup)
    createMockReports(reports, reportCount, baseTime, baseTime + 50 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND, 1200, 2.0, // 2ms delay drift per packet
                      0.0);

    EXPECT_EQ(STATUS_SUCCESS, gccOnTwccPacketReports(pController, reports, reportCount, 100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND));

    // With progressive delay, the Kalman filter should detect the trend
    EXPECT_GT(pController->kalman.m_hat, 0.0);
}

TEST_F(GccFunctionalityTest, onTwccPacketReportsNullArgs)
{
    EXPECT_EQ(STATUS_SUCCESS, createGccController(&pController, nullptr));

    TwccPacketReport reports[10];

    EXPECT_EQ(STATUS_NULL_ARG, gccOnTwccPacketReports(nullptr, reports, 10, 0));
    EXPECT_EQ(STATUS_NULL_ARG, gccOnTwccPacketReports(pController, nullptr, 10, 0));
    EXPECT_EQ(STATUS_NULL_ARG, gccOnTwccPacketReports(pController, reports, 0, 0));
}

TEST_F(GccFunctionalityTest, getTargetBitrateNullArg)
{
    UINT64 bitrate = gccGetTargetBitrate(nullptr);
    EXPECT_EQ(0ULL, bitrate);
}

TEST_F(GccFunctionalityTest, getStateNullArg)
{
    GccState state = gccGetState(nullptr);
    EXPECT_EQ(GCC_STATE_HOLD, state);
}

TEST_F(GccFunctionalityTest, getLossRatioNullArg)
{
    DOUBLE loss = gccGetLossRatio(nullptr);
    EXPECT_DOUBLE_EQ(0.0, loss);
}

TEST_F(GccFunctionalityTest, threadSafetyMultipleAccess)
{
    EXPECT_EQ(STATUS_SUCCESS, createGccController(&pController, nullptr));

    // Simulate concurrent access by calling APIs multiple times
    // (actual thread safety would require spawning threads, but this tests the locking paths)
    for (int i = 0; i < 100; i++) {
        UINT64 bitrate = gccGetTargetBitrate(pController);
        GccState state = gccGetState(pController);
        DOUBLE loss = gccGetLossRatio(pController);

        EXPECT_GE(bitrate, GCC_DEFAULT_MIN_BITRATE);
        EXPECT_LE(bitrate, GCC_DEFAULT_MAX_BITRATE);
        EXPECT_TRUE(state == GCC_STATE_HOLD || state == GCC_STATE_INCREASE || state == GCC_STATE_DECREASE);
        EXPECT_GE(loss, 0.0);
        EXPECT_LE(loss, 1.0);
    }
}

TEST_F(GccFunctionalityTest, multipleFeedbackCycles)
{
    EXPECT_EQ(STATUS_SUCCESS, createGccController(&pController, nullptr));

    static constexpr UINT32 reportCount = 30;
    TwccPacketReport reports[reportCount];

    // Simulate multiple feedback cycles
    for (int cycle = 0; cycle < 10; cycle++) {
        UINT64 baseTime = GETTIME() + cycle * 100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND;

        // Alternate between good and bad conditions
        DOUBLE delayVariation = (cycle % 2 == 0) ? 0.0 : 1.0;
        DOUBLE lossRatio = (cycle % 3 == 0) ? 0.15 : 0.0;

        createMockReports(reports, reportCount, baseTime, baseTime + 50 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND, 1200, delayVariation, lossRatio);

        EXPECT_EQ(STATUS_SUCCESS, gccOnTwccPacketReports(pController, reports, reportCount, 100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND));

        UINT64 targetBitrate = gccGetTargetBitrate(pController);
        EXPECT_GE(targetBitrate, GCC_DEFAULT_MIN_BITRATE);
        EXPECT_LE(targetBitrate, GCC_DEFAULT_MAX_BITRATE);
    }
}

} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com
