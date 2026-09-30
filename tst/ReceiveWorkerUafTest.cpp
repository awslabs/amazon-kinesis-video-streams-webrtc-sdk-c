#include "WebRTCClientTestFixture.h"
#include <chrono>
#include <condition_variable>

namespace com {
namespace amazonaws {
namespace kinesis {
namespace video {
namespace webrtcclient {

// Tests for the receiveLwsMessageWrapper() worker lifetime fix (issue #2240).
//
// Every incoming signaling message is handed to a detached worker thread that
// runs the application's messageReceivedFn callback. freeSignaling() must not
// return (and must not free pSignalingClient) while any of those workers is
// still alive. These tests run fully offline: they build a signaling client
// with a static (fake) credential provider and inject messages directly via
// receiveLwsMessage(), so no AWS credentials or network access are needed.
class ReceiveWorkerUafTest : public WebRtcClientTestBase {};

static const UINT32 kReceiveWorkerCount = 8;

static const CHAR kIceCandidateMessage[] = "{\n"
                                           "    \"messageType\": \"ICE_CANDIDATE\",\n"
                                           "    \"senderClientId\": \"ClientA\",\n"
                                           "    \"messagePayload\": \"SGVsbG8=\"\n"
                                           "}";

struct BlockingCallbackContext {
    std::mutex lock;
    std::condition_variable cv;
    UINT32 entered = 0;
    UINT32 finished = 0;
    bool release = false;
    std::atomic<bool> freeFinished{false};
    // Set by a callback if it observes that freeSignaling() already returned
    // while the callback was still executing. That is exactly the UAF window.
    std::atomic<bool> freeReturnedWhileCallbackRunning{false};
};

// messageReceivedFn that parks every worker until the test releases it.
static STATUS blockingMessageReceivedFn(UINT64 customData, PReceivedSignalingMessage pReceivedSignalingMessage)
{
    UNUSED_PARAM(pReceivedSignalingMessage);
    BlockingCallbackContext* pCtx = (BlockingCallbackContext*) customData;

    {
        std::unique_lock<std::mutex> guard(pCtx->lock);
        pCtx->entered++;
        pCtx->cv.notify_all();
        pCtx->cv.wait(guard, [pCtx] { return pCtx->release; });
    }

    if (pCtx->freeFinished.load()) {
        pCtx->freeReturnedWhileCallbackRunning.store(true);
    }

    {
        std::lock_guard<std::mutex> guard(pCtx->lock);
        pCtx->finished++;
        pCtx->cv.notify_all();
    }

    return STATUS_SUCCESS;
}

// Builds a signaling client without touching the network. createSignalingSync()
// only drives the state machine to GET_TOKEN, which the static provider serves locally.
static STATUS createOfflineSignalingClient(PCHAR channelName, PCHAR region, PCHAR certPath, UINT32 logLevel, UINT64 callbackCustomData,
                                           PAwsCredentialProvider* ppCredentialProvider, PSignalingClient* ppSignalingClient)
{
    STATUS retStatus = STATUS_SUCCESS;
    SignalingClientInfoInternal clientInfoInternal;
    SignalingClientCallbacks signalingClientCallbacks;
    ChannelInfo channelInfo;

    MEMSET(&clientInfoInternal, 0x00, SIZEOF(SignalingClientInfoInternal));
    clientInfoInternal.signalingClientInfo.version = SIGNALING_CLIENT_INFO_CURRENT_VERSION;
    clientInfoInternal.signalingClientInfo.loggingLevel = logLevel;
    STRCPY(clientInfoInternal.signalingClientInfo.clientId, TEST_SIGNALING_MASTER_CLIENT_ID);

    MEMSET(&signalingClientCallbacks, 0x00, SIZEOF(SignalingClientCallbacks));
    signalingClientCallbacks.version = SIGNALING_CLIENT_CALLBACKS_CURRENT_VERSION;
    signalingClientCallbacks.customData = callbackCustomData;
    signalingClientCallbacks.messageReceivedFn = blockingMessageReceivedFn;

    MEMSET(&channelInfo, 0x00, SIZEOF(ChannelInfo));
    channelInfo.version = CHANNEL_INFO_CURRENT_VERSION;
    channelInfo.pChannelName = channelName;
    channelInfo.channelType = SIGNALING_CHANNEL_TYPE_SINGLE_MASTER;
    channelInfo.channelRoleType = SIGNALING_CHANNEL_ROLE_TYPE_MASTER;
    channelInfo.cachingPolicy = SIGNALING_API_CALL_CACHE_TYPE_NONE;
    channelInfo.retry = TRUE;
    channelInfo.reconnect = TRUE;
    channelInfo.pRegion = region;
    channelInfo.pCertPath = certPath;
    channelInfo.messageTtl = TEST_SIGNALING_MESSAGE_TTL;

    CHK_STATUS(createStaticCredentialProvider((PCHAR) "accessKey", 0, (PCHAR) "secretKey", 0, NULL, 0, MAX_UINT64, ppCredentialProvider));
    CHK_STATUS(createSignalingSync(&clientInfoInternal, &channelInfo, &signalingClientCallbacks, *ppCredentialProvider, ppSignalingClient));

CleanUp:
    if (STATUS_FAILED(retStatus)) {
        freeSignaling(ppSignalingClient);
        freeStaticCredentialProvider(ppCredentialProvider);
    }

    return retStatus;
}

// N receive workers are parked inside the application callback. freeSignaling()
// must:
//   - see receiveWorkerCount == N before it starts,
//   - block for as long as any worker is alive,
//   - return only after all N callbacks have finished.
// On the unpatched code freeSignaling() returns immediately and frees the
// client while the workers are still running (ASAN: heap-use-after-free).
TEST_F(ReceiveWorkerUafTest, freeSignalingWaitsForAllInFlightReceiveWorkers)
{
    BlockingCallbackContext ctx;
    PAwsCredentialProvider pCredentialProvider = NULL;
    PSignalingClient pSignalingClient = NULL;
    STATUS freeStatus = STATUS_SUCCESS;

    ASSERT_EQ(STATUS_SUCCESS,
              createOfflineSignalingClient(mChannelName, mRegion, mCaCertPath, mLogLevel, (UINT64) &ctx, &pCredentialProvider, &pSignalingClient));
    ASSERT_NE((PSignalingClient) NULL, pSignalingClient);

    // Spawn N workers.
    for (UINT32 i = 0; i < kReceiveWorkerCount; i++) {
        ASSERT_EQ(STATUS_SUCCESS, receiveLwsMessage(pSignalingClient, (PCHAR) kIceCandidateMessage, ARRAY_SIZE(kIceCandidateMessage)));
    }

    // Wait until every worker is parked inside the callback.
    {
        std::unique_lock<std::mutex> guard(ctx.lock);
        ASSERT_TRUE(ctx.cv.wait_for(guard, std::chrono::seconds(5), [&ctx] { return ctx.entered == kReceiveWorkerCount; }))
            << "Only " << ctx.entered << " of " << kReceiveWorkerCount << " receive workers entered the callback";
    }

    // The in-flight counter must account for every worker.
    EXPECT_EQ((SIZE_T) kReceiveWorkerCount, ATOMIC_LOAD(&pSignalingClient->receiveWorkerCount));

    // Free on a separate thread since it is expected to block.
    std::thread freeThread([&] {
        freeStatus = freeSignaling(&pSignalingClient);
        ctx.freeFinished.store(true);
    });

    // While the workers are parked, free must not complete.
    THREAD_SLEEP(500 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
    EXPECT_FALSE(ctx.freeFinished.load()) << "freeSignaling returned while " << kReceiveWorkerCount
                                          << " receive workers were still inside the callback (use-after-free on unpatched code)";

    // Release all workers and let free complete.
    {
        std::lock_guard<std::mutex> guard(ctx.lock);
        ctx.release = true;
        ctx.cv.notify_all();
    }
    freeThread.join();

    EXPECT_EQ(STATUS_SUCCESS, freeStatus);
    EXPECT_EQ((PSignalingClient) NULL, pSignalingClient);
    EXPECT_TRUE(ctx.freeFinished.load());

    // Every callback ran to completion, and none of them observed free finishing first.
    {
        std::lock_guard<std::mutex> guard(ctx.lock);
        EXPECT_EQ(kReceiveWorkerCount, ctx.finished);
    }
    EXPECT_FALSE(ctx.freeReturnedWhileCallbackRunning.load()) << "A receive callback was still running when freeSignaling returned";

    freeStaticCredentialProvider(&pCredentialProvider);
}

// Counter hygiene: when workers complete normally (no free racing them) the
// in-flight counter must return to exactly zero, and a subsequent free must
// not stall waiting on a phantom worker. Catches missed decrements or
// increment/decrement ordering bugs that the blocking test above cannot see.
TEST_F(ReceiveWorkerUafTest, receiveWorkerCountDrainsToZeroAfterWorkersComplete)
{
    BlockingCallbackContext ctx;
    PAwsCredentialProvider pCredentialProvider = NULL;
    PSignalingClient pSignalingClient = NULL;
    UINT64 beforeFree, elapsedMs;

    // Callbacks should not block in this test.
    ctx.release = true;

    ASSERT_EQ(STATUS_SUCCESS,
              createOfflineSignalingClient(mChannelName, mRegion, mCaCertPath, mLogLevel, (UINT64) &ctx, &pCredentialProvider, &pSignalingClient));
    ASSERT_NE((PSignalingClient) NULL, pSignalingClient);

    for (UINT32 i = 0; i < kReceiveWorkerCount; i++) {
        ASSERT_EQ(STATUS_SUCCESS, receiveLwsMessage(pSignalingClient, (PCHAR) kIceCandidateMessage, ARRAY_SIZE(kIceCandidateMessage)));
    }

    // Wait for all callbacks to finish.
    {
        std::unique_lock<std::mutex> guard(ctx.lock);
        ASSERT_TRUE(ctx.cv.wait_for(guard, std::chrono::seconds(5), [&ctx] { return ctx.finished == kReceiveWorkerCount; }))
            << "Only " << ctx.finished << " of " << kReceiveWorkerCount << " receive callbacks completed";
    }

    // The decrement happens after the callback returns, so give the workers a
    // brief moment to exit, then require the counter to be exactly zero.
    UINT32 attempts = 0;
    while (ATOMIC_LOAD(&pSignalingClient->receiveWorkerCount) != 0 && attempts++ < 50) {
        THREAD_SLEEP(20 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
    }
    EXPECT_EQ((SIZE_T) 0, ATOMIC_LOAD(&pSignalingClient->receiveWorkerCount)) << "receiveWorkerCount leaked after all workers exited";

    // With no workers alive, free must be quick (well under one shutdown timeout).
    beforeFree = GETTIME();
    EXPECT_EQ(STATUS_SUCCESS, freeSignaling(&pSignalingClient));
    elapsedMs = (GETTIME() - beforeFree) / HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
    EXPECT_LT(elapsedMs, 2000) << "freeSignaling took " << elapsedMs << " ms with no receive workers alive; the counter is likely stuck > 0";
    EXPECT_EQ((PSignalingClient) NULL, pSignalingClient);

    freeStaticCredentialProvider(&pCredentialProvider);
}

struct FakeReconnectContext {
    PSignalingClient pSignalingClient = NULL;
    std::atomic<bool> published{false};
    std::atomic<bool> releasedLock{false};
};

// Mimics reconnectHandler()'s exit: publish terminated and broadcast while holding reconnecterTracker.lock, but
// linger inside the lock so the gap between "terminated is TRUE" and "thread is done with pSignalingClient" is wide.
static PVOID fakeReconnectExitHoldingTrackerLock(PVOID args)
{
    FakeReconnectContext* pCtx = (FakeReconnectContext*) args;
    PSignalingClient pSignalingClient = pCtx->pSignalingClient;

    MUTEX_LOCK(pSignalingClient->reconnecterTracker.lock);
    ATOMIC_STORE_BOOL(&pSignalingClient->reconnecterTracker.terminated, TRUE);
    pCtx->published.store(true);
    THREAD_SLEEP(200 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
    CVAR_BROADCAST(pSignalingClient->reconnecterTracker.await);
    pCtx->releasedLock.store(true);
    MUTEX_UNLOCK(pSignalingClient->reconnecterTracker.lock);
    return NULL;
}

// terminated == TRUE alone does not mean the reconnect thread has stopped touching pSignalingClient. freeSignaling()
// must synchronize on reconnecterTracker.lock before freeing; otherwise the thread's broadcast/unlock is a UAF.
TEST_F(ReceiveWorkerUafTest, freeSignalingWaitsForReconnectThreadToReleaseTrackerLock)
{
    BlockingCallbackContext callbackCtx;
    FakeReconnectContext ctx;
    PAwsCredentialProvider pCredentialProvider = NULL;
    PSignalingClient pSignalingClient = NULL;
    TID threadId;

    ASSERT_EQ(STATUS_SUCCESS,
              createOfflineSignalingClient(mChannelName, mRegion, mCaCertPath, mLogLevel, (UINT64) &callbackCtx, &pCredentialProvider,
                                           &pSignalingClient));
    ASSERT_NE((PSignalingClient) NULL, pSignalingClient);
    ctx.pSignalingClient = pSignalingClient;

    ATOMIC_STORE_BOOL(&pSignalingClient->reconnecterTracker.terminated, FALSE);
    ASSERT_EQ(STATUS_SUCCESS, THREAD_CREATE(&threadId, fakeReconnectExitHoldingTrackerLock, (PVOID) &ctx));
    ASSERT_EQ(STATUS_SUCCESS, THREAD_DETACH(threadId));
    while (!ctx.published.load()) {
        THREAD_SLEEP(HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
    }

    // terminated is already TRUE here, but the fake thread still holds the tracker lock.
    EXPECT_EQ(STATUS_SUCCESS, freeSignaling(&pSignalingClient));
    EXPECT_TRUE(ctx.releasedLock.load()) << "freeSignaling() returned while the reconnect thread still held reconnecterTracker.lock";
    freeStaticCredentialProvider(&pCredentialProvider);
}

} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com
