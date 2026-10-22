#include "WebRTCClientTestFixture.h"
#include <vector>

namespace com {
namespace amazonaws {
namespace kinesis {
namespace video {
namespace webrtcclient {

#define TEST_PACKET_LEN       1200
#define TEST_PACKET_HEADER    12
#define TEST_MS               HUNDREDS_OF_NANOS_IN_A_MILLISECOND
#define TEST_LONG_QUEUE_DELAY (10 * HUNDREDS_OF_NANOS_IN_A_SECOND)

class PacerFunctionalityTest : public WebRtcClientTestBase {
  public:
    std::vector<UINT32> sentIds;
    std::vector<std::pair<UINT32, UINT64>> twccReports;
    STATUS sendStatus = STATUS_SUCCESS;
    UINT32 nextId = 0;

    // Each test packet carries its id in its first 4 bytes
    static STATUS sendPacket(UINT64 customData, PBYTE pPacket, UINT32 packetLen)
    {
        PacerFunctionalityTest* pTest = reinterpret_cast<PacerFunctionalityTest*>(customData);
        UINT32 id = 0;
        EXPECT_EQ(TEST_PACKET_LEN, packetLen);
        MEMCPY(&id, pPacket, SIZEOF(UINT32));
        pTest->sentIds.push_back(id);
        return pTest->sendStatus;
    }

    std::atomic<bool> blockingSendStarted{false};

    // Blocks like socketSendDataWithRetry polling a full socket buffer
    static STATUS blockingSend(UINT64 customData, PBYTE pPacket, UINT32 packetLen)
    {
        PacerFunctionalityTest* pTest = reinterpret_cast<PacerFunctionalityTest*>(customData);
        UNUSED_PARAM(pPacket);
        UNUSED_PARAM(packetLen);
        pTest->blockingSendStarted = true;
        THREAD_SLEEP(500 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
        return STATUS_SUCCESS;
    }

    static STATUS recordTimerFired(UINT32 timerId, UINT64 currentTime, UINT64 customData)
    {
        UNUSED_PARAM(timerId);
        UNUSED_PARAM(currentTime);
        ATOMIC_STORE((PSIZE_T) customData, (SIZE_T) GETTIME());
        return STATUS_SUCCESS;
    }

    static VOID packetSent(UINT64 customData, UINT32 twccExtPayload, UINT32 payloadLength, UINT64 sentTime)
    {
        PacerFunctionalityTest* pTest = reinterpret_cast<PacerFunctionalityTest*>(customData);
        EXPECT_EQ(TEST_PACKET_LEN - TEST_PACKET_HEADER, payloadLength);
        pTest->twccReports.push_back(std::make_pair(twccExtPayload, sentTime));
    }

    PPacer createTestPacer(UINT64 minPacingRateBps, UINT64 maxQueueDelay, UINT32 maxQueuePackets = PACER_MAX_QUEUE_PACKETS)
    {
        PacerConfig config;
        PPacer pPacer = NULL;
        config.pacingFactor = 1.0;
        config.minPacingRateBps = minPacingRateBps;
        config.maxQueueDelay = maxQueueDelay;
        config.maxQueuePackets = maxQueuePackets;
        EXPECT_EQ(STATUS_SUCCESS, createPacer(&config, sendPacket, packetSent, (UINT64) this, &pPacer));
        return pPacer;
    }

    STATUS enqueue(PPacer pPacer, UINT32 count, UINT64 now, BOOL hasTwcc = FALSE)
    {
        BYTE packet[TEST_PACKET_LEN];
        STATUS retStatus = STATUS_SUCCESS;
        MEMSET(packet, 0x00, SIZEOF(packet));
        for (UINT32 i = 0; i < count && STATUS_SUCCEEDED(retStatus); i++) {
            UINT32 id = nextId++;
            MEMCPY(packet, &id, SIZEOF(UINT32));
            retStatus = pacerEnqueuePacket(pPacer, packet, SIZEOF(packet), hasTwcc, id, TEST_PACKET_LEN - TEST_PACKET_HEADER, now);
        }
        return retStatus;
    }

    // Runs the pacer every PACER_PROCESS_INTERVAL from start to end (inclusive)
    VOID processUntil(PPacer pPacer, UINT64 start, UINT64 end)
    {
        for (UINT64 now = start; now <= end; now += PACER_PROCESS_INTERVAL) {
            EXPECT_EQ(STATUS_SUCCESS, pacerProcess(pPacer, now));
        }
    }
};

TEST_F(PacerFunctionalityTest, createAndFreeInvalidArgs)
{
    PacerConfig config;
    PPacer pPacer = NULL;

    config.pacingFactor = 1.0;
    config.minPacingRateBps = PACER_MIN_PACING_RATE_BPS;
    config.maxQueueDelay = TEST_LONG_QUEUE_DELAY;
    config.maxQueuePackets = PACER_MAX_QUEUE_PACKETS;

    EXPECT_EQ(STATUS_NULL_ARG, createPacer(NULL, sendPacket, packetSent, 0, &pPacer));
    EXPECT_EQ(STATUS_NULL_ARG, createPacer(&config, NULL, packetSent, 0, &pPacer));
    EXPECT_EQ(STATUS_NULL_ARG, createPacer(&config, sendPacket, packetSent, 0, NULL));

    config.pacingFactor = 0;
    EXPECT_EQ(STATUS_INVALID_ARG, createPacer(&config, sendPacket, packetSent, 0, &pPacer));
    config.pacingFactor = 1.0;
    config.maxQueueDelay = 0;
    EXPECT_EQ(STATUS_INVALID_ARG, createPacer(&config, sendPacket, packetSent, 0, &pPacer));
    config.maxQueueDelay = TEST_LONG_QUEUE_DELAY;
    config.maxQueuePackets = 0;
    EXPECT_EQ(STATUS_INVALID_ARG, createPacer(&config, sendPacket, packetSent, 0, &pPacer));
    EXPECT_TRUE(pPacer == NULL);

    EXPECT_EQ(STATUS_NULL_ARG, freePacer(NULL));
    EXPECT_EQ(STATUS_SUCCESS, freePacer(&pPacer));
    EXPECT_EQ(STATUS_NULL_ARG, pacerProcess(NULL, 0));
}

TEST_F(PacerFunctionalityTest, spreadsABurstOverTime)
{
    // 960 kbps is 600 bytes per 5 ms tick, so one 1200-byte packet every other tick until the media rate is measured
    PPacer pPacer = createTestPacer(960000, TEST_LONG_QUEUE_DELAY);
    UINT32 i;

    EXPECT_EQ(STATUS_SUCCESS, enqueue(pPacer, 50, 0));

    processUntil(pPacer, 5 * TEST_MS, 20 * TEST_MS);
    EXPECT_GE(sentIds.size(), 1U);
    EXPECT_LE(sentIds.size(), 3U);

    processUntil(pPacer, 25 * TEST_MS, 400 * TEST_MS);
    ASSERT_EQ(50U, sentIds.size());
    for (i = 0; i < sentIds.size(); i++) {
        EXPECT_EQ(i, sentIds[i]);
    }

    EXPECT_EQ(STATUS_SUCCESS, freePacer(&pPacer));
}

TEST_F(PacerFunctionalityTest, packetLargerThanBudgetIsNotStalled)
{
    // 8 kbps is 5 bytes per tick: far less than a packet, which a "wait for enough tokens" pacer would never send
    PPacer pPacer = createTestPacer(8000, TEST_LONG_QUEUE_DELAY);

    EXPECT_EQ(STATUS_SUCCESS, enqueue(pPacer, 2, 0));

    EXPECT_EQ(STATUS_SUCCESS, pacerProcess(pPacer, 5 * TEST_MS));
    EXPECT_EQ(1U, sentIds.size());

    // The second packet waits until the budget recovers from the first one
    EXPECT_EQ(STATUS_SUCCESS, pacerProcess(pPacer, 10 * TEST_MS));
    EXPECT_EQ(1U, sentIds.size());

    processUntil(pPacer, 15 * TEST_MS, 2000 * TEST_MS);
    EXPECT_EQ(2U, sentIds.size());

    EXPECT_EQ(STATUS_SUCCESS, freePacer(&pPacer));
}

TEST_F(PacerFunctionalityTest, queueDrainsWithinMaxQueueDelay)
{
    PPacer pPacer = createTestPacer(8000, 50 * TEST_MS);

    EXPECT_EQ(STATUS_SUCCESS, enqueue(pPacer, 100, 0));

    // Still paced: the first tick sends only part of the queue
    EXPECT_EQ(STATUS_SUCCESS, pacerProcess(pPacer, 5 * TEST_MS));
    EXPECT_GT(sentIds.size(), 0U);
    EXPECT_LT(sentIds.size(), 100U);

    // Everything is out by the time the oldest packet has waited 50 ms
    processUntil(pPacer, 10 * TEST_MS, 50 * TEST_MS);
    EXPECT_EQ(100U, sentIds.size());

    EXPECT_EQ(STATUS_SUCCESS, freePacer(&pPacer));
}

TEST_F(PacerFunctionalityTest, fullQueueRejectsPackets)
{
    PPacer pPacer = createTestPacer(100000000, TEST_LONG_QUEUE_DELAY, 2);

    EXPECT_EQ(STATUS_SUCCESS, enqueue(pPacer, 2, 0));
    EXPECT_EQ(STATUS_PEERCONNECTION_PACER_QUEUE_FULL, enqueue(pPacer, 1, 0));

    EXPECT_EQ(STATUS_SUCCESS, pacerProcess(pPacer, 5 * TEST_MS));
    EXPECT_EQ(2U, sentIds.size());
    EXPECT_EQ(STATUS_SUCCESS, enqueue(pPacer, 1, 5 * TEST_MS));

    EXPECT_EQ(STATUS_SUCCESS, freePacer(&pPacer));
}

TEST_F(PacerFunctionalityTest, twccSendTimeIsWhenThePacketLeaves)
{
    PPacer pPacer = createTestPacer(100000000, TEST_LONG_QUEUE_DELAY);

    EXPECT_EQ(STATUS_SUCCESS, enqueue(pPacer, 3, 0, TRUE));
    EXPECT_EQ(STATUS_SUCCESS, enqueue(pPacer, 1, 0, FALSE));

    EXPECT_EQ(STATUS_SUCCESS, pacerProcess(pPacer, 5 * TEST_MS));
    EXPECT_EQ(4U, sentIds.size());

    // Only the packets with a TWCC sequence number are reported, with the time they were sent rather than queued
    ASSERT_EQ(3U, twccReports.size());
    for (UINT32 i = 0; i < twccReports.size(); i++) {
        EXPECT_EQ(i, twccReports[i].first);
        EXPECT_EQ(5 * TEST_MS, twccReports[i].second);
    }

    EXPECT_EQ(STATUS_SUCCESS, freePacer(&pPacer));
}

TEST_F(PacerFunctionalityTest, failedSendIsNotReportedToTwcc)
{
    PPacer pPacer = createTestPacer(100000000, TEST_LONG_QUEUE_DELAY);

    sendStatus = STATUS_SEND_DATA_FAILED;
    EXPECT_EQ(STATUS_SUCCESS, enqueue(pPacer, 2, 0, TRUE));

    EXPECT_EQ(STATUS_SUCCESS, pacerProcess(pPacer, 5 * TEST_MS));
    EXPECT_EQ(2U, sentIds.size());
    EXPECT_EQ(0U, twccReports.size());

    // Failed packets are not retried
    EXPECT_EQ(STATUS_SUCCESS, pacerProcess(pPacer, 10 * TEST_MS));
    EXPECT_EQ(2U, sentIds.size());

    EXPECT_EQ(STATUS_SUCCESS, freePacer(&pPacer));
}

TEST_F(PacerFunctionalityTest, idleTimeDoesNotBuildUpABurst)
{
    PPacer pPacer = createTestPacer(960000, TEST_LONG_QUEUE_DELAY);

    // A second with nothing to send must not bank a second's worth of budget
    processUntil(pPacer, 5 * TEST_MS, 1000 * TEST_MS);

    EXPECT_EQ(STATUS_SUCCESS, enqueue(pPacer, 20, 1000 * TEST_MS));
    EXPECT_EQ(STATUS_SUCCESS, pacerProcess(pPacer, 1005 * TEST_MS));
    EXPECT_LE(sentIds.size(), 2U);

    EXPECT_EQ(STATUS_SUCCESS, freePacer(&pPacer));
}

TEST_F(PacerFunctionalityTest, freeDropsQueuedPackets)
{
    PPacer pPacer = createTestPacer(960000, TEST_LONG_QUEUE_DELAY);

    EXPECT_EQ(STATUS_SUCCESS, enqueue(pPacer, 10, 0));
    EXPECT_EQ(STATUS_SUCCESS, freePacer(&pPacer));
    EXPECT_TRUE(pPacer == NULL);
    EXPECT_EQ(0U, sentIds.size());
}

TEST_F(PacerFunctionalityTest, peerConnectionCreatesPacerOnlyWhenEnabled)
{
    RtcConfiguration configuration;
    PRtcPeerConnection pRtcPeerConnection = NULL;

    MEMSET(&configuration, 0x00, SIZEOF(RtcConfiguration));
    EXPECT_EQ(STATUS_SUCCESS, createPeerConnection(&configuration, &pRtcPeerConnection));
    EXPECT_TRUE(reinterpret_cast<PKvsPeerConnection>(pRtcPeerConnection)->pPacer == NULL);
    EXPECT_FALSE(IS_VALID_TIMER_QUEUE_HANDLE(reinterpret_cast<PKvsPeerConnection>(pRtcPeerConnection)->pacerTimerQueueHandle));
    EXPECT_EQ(STATUS_SUCCESS, freePeerConnection(&pRtcPeerConnection));

    configuration.kvsRtcConfiguration.enablePacer = TRUE;
    EXPECT_EQ(STATUS_SUCCESS, createPeerConnection(&configuration, &pRtcPeerConnection));
    EXPECT_TRUE(reinterpret_cast<PKvsPeerConnection>(pRtcPeerConnection)->pPacer != NULL);
    // The pacer gets its own timer queue, separate from the one ICE, DTLS and RTCP use
    EXPECT_TRUE(IS_VALID_TIMER_QUEUE_HANDLE(reinterpret_cast<PKvsPeerConnection>(pRtcPeerConnection)->pacerTimerQueueHandle));
    EXPECT_NE(reinterpret_cast<PKvsPeerConnection>(pRtcPeerConnection)->pacerTimerQueueHandle,
              reinterpret_cast<PKvsPeerConnection>(pRtcPeerConnection)->timerQueueHandle);
    EXPECT_EQ(STATUS_SUCCESS, freePeerConnection(&pRtcPeerConnection));

    // A pacing factor below 1 would pace slower than the media is produced
    configuration.kvsRtcConfiguration.pacingFactor = 0.5;
    EXPECT_EQ(STATUS_INVALID_ARG, createPeerConnection(&configuration, &pRtcPeerConnection));
}

TEST_F(PacerFunctionalityTest, blockedPacerSendDoesNotStallPeerConnectionTimers)
{
    RtcConfiguration configuration;
    PRtcPeerConnection pRtcPeerConnection = NULL;
    PKvsPeerConnection pKvsPeerConnection = NULL;
    BYTE packet[TEST_PACKET_LEN];
    volatile SIZE_T firedTime = 0;
    UINT32 timerId = 0, i;
    UINT64 start;

    MEMSET(&configuration, 0x00, SIZEOF(RtcConfiguration));
    configuration.kvsRtcConfiguration.enablePacer = TRUE;
    ASSERT_EQ(STATUS_SUCCESS, createPeerConnection(&configuration, &pRtcPeerConnection));
    pKvsPeerConnection = reinterpret_cast<PKvsPeerConnection>(pRtcPeerConnection);

    // Swap in a send that blocks for 500 ms. Done before enqueueing, so the pacer thread sees it once it takes the lock
    pKvsPeerConnection->pPacer->sendPacketFn = blockingSend;
    pKvsPeerConnection->pPacer->customData = (UINT64) this;
    MEMSET(packet, 0x00, SIZEOF(packet));
    EXPECT_EQ(STATUS_SUCCESS, pacerEnqueuePacket(pKvsPeerConnection->pPacer, packet, SIZEOF(packet), FALSE, 0, 0, GETTIME()));
    for (i = 0; i < 100 && !blockingSendStarted; i++) {
        THREAD_SLEEP(5 * TEST_MS);
    }
    ASSERT_TRUE(blockingSendStarted);

    // While the pacer is stuck in that send, a timer on the peer connection's main queue still runs on time
    start = GETTIME();
    EXPECT_EQ(STATUS_SUCCESS,
              timerQueueAddTimer(pKvsPeerConnection->timerQueueHandle, 10 * TEST_MS, TIMER_QUEUE_SINGLE_INVOCATION_PERIOD, recordTimerFired,
                                 (UINT64) &firedTime, &timerId));
    for (i = 0; i < 60 && ATOMIC_LOAD(&firedTime) == 0; i++) {
        THREAD_SLEEP(5 * TEST_MS);
    }
    ASSERT_NE(0U, ATOMIC_LOAD(&firedTime));
    EXPECT_LT(ATOMIC_LOAD(&firedTime) - start, 200 * TEST_MS);

    // Waits for the blocked send to finish before freeing the pacer
    EXPECT_EQ(STATUS_SUCCESS, freePeerConnection(&pRtcPeerConnection));
}

} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com
