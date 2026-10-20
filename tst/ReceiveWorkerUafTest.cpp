#include "WebRTCClientTestFixture.h"
#include <chrono>
#include <condition_variable>
#include <thread>

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
                                           PAwsCredentialProvider* ppCredentialProvider, PSignalingClient* ppSignalingClient,
                                           SignalingClientMessageReceivedFunc messageReceivedFn = blockingMessageReceivedFn)
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
    signalingClientCallbacks.messageReceivedFn = messageReceivedFn;

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

struct FreeFromCallbackContext {
    std::mutex lock;
    std::condition_variable cv;
    PSignalingClient pSignalingClient = NULL;
    bool done = false;
    STATUS freeStatus = STATUS_SUCCESS;
};

// messageReceivedFn that tries to free the signaling client from inside the callback.
static STATUS freeFromCallbackMessageReceivedFn(UINT64 customData, PReceivedSignalingMessage pReceivedSignalingMessage)
{
    UNUSED_PARAM(pReceivedSignalingMessage);
    FreeFromCallbackContext* pCtx = (FreeFromCallbackContext*) customData;
    PSignalingClient pSignalingClient = pCtx->pSignalingClient;
    STATUS status = freeSignaling(&pSignalingClient);

    std::lock_guard<std::mutex> guard(pCtx->lock);
    pCtx->freeStatus = status;
    pCtx->done = true;
    pCtx->cv.notify_all();
    return STATUS_SUCCESS;
}

// freeSignaling() waits for every receive worker to exit, so calling it from messageReceivedFn would wait on
// itself forever. It must refuse with STATUS_SIGNALING_FREE_FROM_CALLBACK and leave the client intact instead.
TEST_F(ReceiveWorkerUafTest, freeSignalingFromReceiveCallbackIsRejected)
{
    // Heap-allocated: if the callback never reports back, the worker may still write to this context after the
    // test body returns, so on that path it (and the client) is leaked on purpose rather than destroyed underneath
    // a live thread and corrupting a later test.
    FreeFromCallbackContext* pCtx = new FreeFromCallbackContext();
    PAwsCredentialProvider pCredentialProvider = NULL;
    PSignalingClient pSignalingClient = NULL;
    bool done;

    ASSERT_EQ(STATUS_SUCCESS,
              createOfflineSignalingClient(mChannelName, mRegion, mCaCertPath, mLogLevel, (UINT64) pCtx, &pCredentialProvider, &pSignalingClient,
                                           freeFromCallbackMessageReceivedFn));
    ASSERT_NE((PSignalingClient) NULL, pSignalingClient);
    pCtx->pSignalingClient = pSignalingClient;

    ASSERT_EQ(STATUS_SUCCESS, receiveLwsMessage(pSignalingClient, (PCHAR) kIceCandidateMessage, ARRAY_SIZE(kIceCandidateMessage)));

    {
        std::unique_lock<std::mutex> guard(pCtx->lock);
        done = pCtx->cv.wait_for(guard, std::chrono::seconds(5), [pCtx] { return pCtx->done; });
    }
    ASSERT_TRUE(done) << "freeSignaling() called from messageReceivedFn did not return (self-wait deadlock); leaking the client and context";
    EXPECT_EQ(STATUS_SIGNALING_FREE_FROM_CALLBACK, pCtx->freeStatus);

    // The client must still be usable and freeable from a non-signaling thread.
    EXPECT_FALSE(ATOMIC_LOAD_BOOL(&pSignalingClient->shutdown));
    EXPECT_EQ(STATUS_SUCCESS, freeSignaling(&pSignalingClient));
    EXPECT_EQ((PSignalingClient) NULL, pSignalingClient);
    freeStaticCredentialProvider(&pCredentialProvider);
    delete pCtx;
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

    // Wait until every worker is parked inside the callback. EXPECT, not ASSERT: on a slow host the rest of this
    // test must still run, because it is what releases the workers and frees the client. Returning early here would
    // destroy ctx under detached workers that are blocked on its condition variable.
    {
        std::unique_lock<std::mutex> guard(ctx.lock);
        EXPECT_TRUE(ctx.cv.wait_for(guard, std::chrono::seconds(5), [&ctx] { return ctx.entered == kReceiveWorkerCount; }))
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

    // Wait for all callbacks to finish. EXPECT so the client below is always freed.
    {
        std::unique_lock<std::mutex> guard(ctx.lock);
        EXPECT_TRUE(ctx.cv.wait_for(guard, std::chrono::seconds(5), [&ctx] { return ctx.finished == kReceiveWorkerCount; }))
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

// Counter stress: many short-lived workers in quick succession. A worker that runs to completion before the
// spawner has counted it would decrement first and wrap the SIZE_T counter to its maximum until the late increment
// brings it back; a shutdown drain that samples the counter in that window waits for a phantom worker. The wrap is
// transient, so a watcher samples the counter throughout dispatch: it must never exceed the number of messages
// sent (a wrapped value reads as SIZE_T max). Eight parked workers cannot hit that window; a thousand fast ones
// give it a chance, and the watcher reliably catches the wrap once the window is a millisecond or more wide.
TEST_F(ReceiveWorkerUafTest, receiveWorkerCountSurvivesManyShortLivedWorkers)
{
    static const UINT32 kStressMessageCount = 1000;
    struct CountingContext {
        std::atomic<UINT32> finished{0};
    };
    CountingContext* pCtx = new CountingContext(); // leaked only if a worker is still alive at the end
    PAwsCredentialProvider pCredentialProvider = NULL;
    PSignalingClient pSignalingClient = NULL;
    std::atomic<bool> stopWatching{false};
    std::atomic<SIZE_T> maxObserved{0};
    UINT64 beforeFree, elapsedMs;
    UINT32 i, attempts = 0;

    auto countingMessageReceivedFn = [](UINT64 customData, PReceivedSignalingMessage) -> STATUS {
        ((CountingContext*) customData)->finished.fetch_add(1);
        return STATUS_SUCCESS;
    };

    ASSERT_EQ(STATUS_SUCCESS,
              createOfflineSignalingClient(mChannelName, mRegion, mCaCertPath, mLogLevel, (UINT64) pCtx, &pCredentialProvider, &pSignalingClient,
                                           countingMessageReceivedFn));
    ASSERT_NE((PSignalingClient) NULL, pSignalingClient);

    std::thread watcher([&] {
        while (!stopWatching.load()) {
            SIZE_T count = ATOMIC_LOAD(&pSignalingClient->receiveWorkerCount);
            SIZE_T seen = maxObserved.load();
            while (count > seen && !maxObserved.compare_exchange_weak(seen, count)) {
            }
            std::this_thread::yield();
        }
    });

    for (i = 0; i < kStressMessageCount; i++) {
        ASSERT_EQ(STATUS_SUCCESS, receiveLwsMessage(pSignalingClient, (PCHAR) kIceCandidateMessage, ARRAY_SIZE(kIceCandidateMessage)));
    }

    // Every callback must run, and the counter must settle at exactly zero (not wrap, not leak).
    while ((pCtx->finished.load() < kStressMessageCount || ATOMIC_LOAD(&pSignalingClient->receiveWorkerCount) != 0) && attempts++ < 500) {
        THREAD_SLEEP(20 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
    }
    stopWatching = true;
    watcher.join();

    EXPECT_EQ(kStressMessageCount, pCtx->finished.load());
    EXPECT_EQ((SIZE_T) 0, ATOMIC_LOAD(&pSignalingClient->receiveWorkerCount))
        << "receiveWorkerCount is " << ATOMIC_LOAD(&pSignalingClient->receiveWorkerCount) << " after " << kStressMessageCount << " workers completed";
    EXPECT_LE(maxObserved.load(), (SIZE_T) kStressMessageCount)
        << "receiveWorkerCount read " << maxObserved.load() << " during dispatch: a worker decremented before it was counted (wrap)";

    beforeFree = GETTIME();
    EXPECT_EQ(STATUS_SUCCESS, freeSignaling(&pSignalingClient));
    elapsedMs = (GETTIME() - beforeFree) / HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
    EXPECT_LT(elapsedMs, 2000) << "freeSignaling took " << elapsedMs << " ms after all workers completed; the counter is likely stuck > 0";
    EXPECT_EQ((PSignalingClient) NULL, pSignalingClient);

    freeStaticCredentialProvider(&pCredentialProvider);
    if (pCtx->finished.load() == kStressMessageCount) {
        delete pCtx;
    }
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
    // Same bookkeeping as reconnectThreadExited(): this is the only reconnect thread, so it publishes terminated.
    pSignalingClient->reconnectThreadCount--;
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

    // Same bookkeeping as startReconnectHandler(): count the thread and clear terminated under the tracker lock.
    MUTEX_LOCK(pSignalingClient->reconnecterTracker.lock);
    pSignalingClient->reconnectThreadCount++;
    ATOMIC_STORE_BOOL(&pSignalingClient->reconnecterTracker.terminated, FALSE);
    MUTEX_UNLOCK(pSignalingClient->reconnecterTracker.lock);
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

// Shared by the two "API call from an SDK thread while another thread frees" tests below.
//
// The SDK thread waits until freeSignaling() has started on the test's free thread (shutdown == TRUE), then calls
// the public APIs that share terminateOngoingOperations() with freeSignaling(). Before the fix those calls read
// shutdown == TRUE, took the unbounded wait meant for freeSignaling(), and waited for their own thread to exit while
// freeSignaling() waited for the same thread: neither returned. They must now return
// STATUS_SIGNALING_FREE_IN_PROGRESS at once, and freeSignaling() must then complete.
static const UINT64 kFreeStartWaitTimeout = 5 * HUNDREDS_OF_NANOS_IN_A_SECOND;
static const std::chrono::seconds kApiDuringFreeBudget(10);

struct ApiDuringFreeContext {
    std::mutex lock;
    std::condition_variable cv;
    PSignalingClient pSignalingClient = NULL;
    // Set by the SDK thread.
    bool callbackEntered = false;
    bool freeStarted = false;
    bool apiCallsReturned = false;
    STATUS deleteStatus = STATUS_SUCCESS;
    STATUS fetchStatus = STATUS_SUCCESS;
    STATUS disconnectStatus = STATUS_SUCCESS;
    // Set by the free thread.
    bool freeReturned = false;
    STATUS freeStatus = STATUS_SUCCESS;
};

// Blocks until freeSignaling() has set shutdown on another thread. Returns FALSE if that never happens, so a test
// whose free thread failed to start does not hang here.
static BOOL waitForFreeToStart(PSignalingClient pSignalingClient)
{
    UINT64 deadline = GETTIME() + kFreeStartWaitTimeout;
    while (!ATOMIC_LOAD_BOOL(&pSignalingClient->shutdown)) {
        if (GETTIME() >= deadline) {
            return FALSE;
        }
        THREAD_SLEEP(HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
    }
    return TRUE;
}

// Runs on the SDK thread. deleteSync first: it is the one call that always reaches terminateOngoingOperations(), so
// it is the call that hung before the fix. fetchSync and disconnectSync are checked afterwards for the same status.
static void callApisDuringFree(ApiDuringFreeContext* pCtx)
{
    PSignalingClient pSignalingClient = pCtx->pSignalingClient;
    SIGNALING_CLIENT_HANDLE handle = TO_SIGNALING_CLIENT_HANDLE(pSignalingClient);
    BOOL freeStarted = waitForFreeToStart(pSignalingClient);
    STATUS deleteStatus = STATUS_SUCCESS, fetchStatus = STATUS_SUCCESS, disconnectStatus = STATUS_SUCCESS;

    if (freeStarted) {
        deleteStatus = signalingClientDeleteSync(handle);
        fetchStatus = signalingClientFetchSync(handle);
        disconnectStatus = signalingClientDisconnectSync(handle);
    }

    std::lock_guard<std::mutex> guard(pCtx->lock);
    pCtx->freeStarted = freeStarted;
    pCtx->deleteStatus = deleteStatus;
    pCtx->fetchStatus = fetchStatus;
    pCtx->disconnectStatus = disconnectStatus;
    pCtx->apiCallsReturned = true;
    pCtx->cv.notify_all();
}

static void freeOnOwnThread(ApiDuringFreeContext* pCtx)
{
    PSignalingClient pSignalingClient = pCtx->pSignalingClient;
    STATUS status = freeSignaling(&pSignalingClient);

    std::lock_guard<std::mutex> guard(pCtx->lock);
    pCtx->freeStatus = status;
    pCtx->freeReturned = true;
    pCtx->cv.notify_all();
}

// Waits for both the API calls and freeSignaling() to return, then checks the contract. On the pre-fix code both
// threads are deadlocked: the wait times out, the test fails, and the context, the client and the free thread are
// leaked on purpose rather than destroyed underneath two live threads.
static void expectApiCallsAndFreeToReturn(ApiDuringFreeContext* pCtx, std::thread& freeThread, PAwsCredentialProvider* ppCredentialProvider)
{
    bool bothReturned;
    {
        std::unique_lock<std::mutex> guard(pCtx->lock);
        bothReturned = pCtx->cv.wait_for(guard, kApiDuringFreeBudget, [pCtx] { return pCtx->apiCallsReturned && pCtx->freeReturned; });
    }
    if (!bothReturned) {
        std::lock_guard<std::mutex> guard(pCtx->lock);
        freeThread.detach();
        FAIL() << "did not return within " << kApiDuringFreeBudget.count() << " s: apiCallsReturned=" << pCtx->apiCallsReturned
               << " freeReturned=" << pCtx->freeReturned << "; leaking the client, the context and the free thread";
    }
    freeThread.join();

    EXPECT_TRUE(pCtx->freeStarted) << "freeSignaling() never set shutdown; the API calls were not attempted";
    EXPECT_EQ(STATUS_SIGNALING_FREE_IN_PROGRESS, pCtx->deleteStatus);
    EXPECT_EQ(STATUS_SIGNALING_FREE_IN_PROGRESS, pCtx->fetchStatus);
    EXPECT_EQ(STATUS_SIGNALING_FREE_IN_PROGRESS, pCtx->disconnectStatus);
    EXPECT_EQ(STATUS_SUCCESS, pCtx->freeStatus);

    freeStaticCredentialProvider(ppCredentialProvider);
    delete pCtx;
}

static STATUS apiDuringFreeMessageReceivedFn(UINT64 customData, PReceivedSignalingMessage pReceivedSignalingMessage)
{
    ApiDuringFreeContext* pCtx = (ApiDuringFreeContext*) customData;
    UNUSED_PARAM(pReceivedSignalingMessage);
    {
        std::lock_guard<std::mutex> guard(pCtx->lock);
        pCtx->callbackEntered = true;
        pCtx->cv.notify_all();
    }
    callApisDuringFree(pCtx);
    return STATUS_SUCCESS;
}

// For tests that never inject a message: a delivery would mean the test is doing something it does not intend.
static STATUS unexpectedMessageReceivedFn(UINT64 customData, PReceivedSignalingMessage pReceivedSignalingMessage)
{
    UNUSED_PARAM(customData);
    UNUSED_PARAM(pReceivedSignalingMessage);
    ADD_FAILURE() << "messageReceivedFn was invoked, but this test injects no message";
    return STATUS_SUCCESS;
}

// The samples' setting (samples/common/Common.c): signalingClientFetchSync() retries a failed fetch through the
// exponential back-off schedule. With the default of 0 the public wrapper returns after the first failure, and a
// refusal that is wrongly retried would go unnoticed.
static void useSamplesFetchRetryPolicy(PSignalingClient pSignalingClient)
{
    pSignalingClient->clientInfo.signalingClientInfo.signalingClientCreationMaxRetryAttempts = CREATE_SIGNALING_CLIENT_RETRY_ATTEMPTS_SENTINEL_VALUE;
}

// Variant 1: the SDK thread is a receive worker running messageReceivedFn. Its receiveWorkerCount decrement happens
// after the callback returns, so a deleteSync() that waited for receiveWorkerCount == 0 would wait for itself.
TEST_F(ReceiveWorkerUafTest, apiCallFromReceiveCallbackDuringFreeReturnsFreeInProgress)
{
    ApiDuringFreeContext* pCtx = new ApiDuringFreeContext();
    PAwsCredentialProvider pCredentialProvider = NULL;
    PSignalingClient pSignalingClient = NULL;

    ASSERT_EQ(STATUS_SUCCESS,
              createOfflineSignalingClient(mChannelName, mRegion, mCaCertPath, mLogLevel, (UINT64) pCtx, &pCredentialProvider, &pSignalingClient,
                                           apiDuringFreeMessageReceivedFn));
    ASSERT_NE((PSignalingClient) NULL, pSignalingClient);
    pCtx->pSignalingClient = pSignalingClient;
    useSamplesFetchRetryPolicy(pSignalingClient);

    // One worker, parked in the callback waiting for the free to start. Start the free only once the worker is in
    // the callback: a worker that has not reached it yet drops its message once shutdown is set.
    ASSERT_EQ(STATUS_SUCCESS, receiveLwsMessage(pSignalingClient, (PCHAR) kIceCandidateMessage, ARRAY_SIZE(kIceCandidateMessage)));
    {
        std::unique_lock<std::mutex> guard(pCtx->lock);
        ASSERT_TRUE(pCtx->cv.wait_for(guard, std::chrono::seconds(5), [pCtx] { return pCtx->callbackEntered; }))
            << "the receive worker did not enter the callback";
    }

    // freeSignaling() blocks until the worker exits, which it only does after its API calls return.
    std::thread freeThread(freeOnOwnThread, pCtx);
    expectApiCallsAndFreeToReturn(pCtx, freeThread, &pCredentialProvider);
}

// Mimics reconnectHandler(): it delivers errorReportFn/stateChangeFn (here: the API calls) and only afterwards
// publishes its own exit under reconnecterTracker.lock, the same way reconnectThreadExited() does.
static PVOID fakeReconnectThreadCallingApisDuringFree(PVOID args)
{
    ApiDuringFreeContext* pCtx = (ApiDuringFreeContext*) args;
    PSignalingClient pSignalingClient = pCtx->pSignalingClient;

    callApisDuringFree(pCtx);

    MUTEX_LOCK(pSignalingClient->reconnecterTracker.lock);
    pSignalingClient->reconnectThreadCount--;
    ATOMIC_STORE_BOOL(&pSignalingClient->reconnecterTracker.terminated, TRUE);
    CVAR_BROADCAST(pSignalingClient->reconnecterTracker.await);
    MUTEX_UNLOCK(pSignalingClient->reconnecterTracker.lock);
    return NULL;
}

// Variant 2: the SDK thread is a reconnect thread. It publishes terminated only after its callbacks return, so a
// deleteSync() that waited for reconnecterTracker.terminated would wait for itself.
TEST_F(ReceiveWorkerUafTest, apiCallFromReconnectThreadDuringFreeReturnsFreeInProgress)
{
    ApiDuringFreeContext* pCtx = new ApiDuringFreeContext();
    PAwsCredentialProvider pCredentialProvider = NULL;
    PSignalingClient pSignalingClient = NULL;
    TID threadId;

    ASSERT_EQ(STATUS_SUCCESS,
              createOfflineSignalingClient(mChannelName, mRegion, mCaCertPath, mLogLevel, (UINT64) pCtx, &pCredentialProvider, &pSignalingClient,
                                           unexpectedMessageReceivedFn));
    ASSERT_NE((PSignalingClient) NULL, pSignalingClient);
    pCtx->pSignalingClient = pSignalingClient;
    useSamplesFetchRetryPolicy(pSignalingClient);

    // Same bookkeeping as startReconnectHandler(): count the thread and clear terminated under the tracker lock.
    MUTEX_LOCK(pSignalingClient->reconnecterTracker.lock);
    pSignalingClient->reconnectThreadCount++;
    ATOMIC_STORE_BOOL(&pSignalingClient->reconnecterTracker.terminated, FALSE);
    MUTEX_UNLOCK(pSignalingClient->reconnecterTracker.lock);
    ASSERT_EQ(STATUS_SUCCESS, THREAD_CREATE(&threadId, fakeReconnectThreadCallingApisDuringFree, (PVOID) pCtx));
    ASSERT_EQ(STATUS_SUCCESS, THREAD_DETACH(threadId));

    // freeSignaling() blocks in the reconnect drain until the fake thread publishes terminated, which it only does
    // after its API calls return.
    std::thread freeThread(freeOnOwnThread, pCtx);
    expectApiCallsAndFreeToReturn(pCtx, freeThread, &pCredentialProvider);
}

// The two tests above refuse at the API entry, before terminateOngoingOperations() runs, so they never reach the
// wait-selection with shutdown already set. This one does: it is what a fetch/disconnect/delete sees when it passed
// its own shutdown check an instant before freeSignaling() set the flag. The non-free quiesce must still take the
// bounded path and must not wait for the parked receive worker; selecting the path from the flag instead of the
// caller would wait for receiveWorkerCount == 0 here.
TEST_F(ReceiveWorkerUafTest, apiQuiesceDoesNotWaitForReceiveWorkersOnceShutdownIsSet)
{
    BlockingCallbackContext ctx;
    PAwsCredentialProvider pCredentialProvider = NULL;
    PSignalingClient pSignalingClient = NULL;
    std::atomic<bool> quiesceReturned{false};
    bool returnedInTime;

    ASSERT_EQ(STATUS_SUCCESS,
              createOfflineSignalingClient(mChannelName, mRegion, mCaCertPath, mLogLevel, (UINT64) &ctx, &pCredentialProvider, &pSignalingClient));
    ASSERT_NE((PSignalingClient) NULL, pSignalingClient);

    // One receive worker parked in messageReceivedFn keeps receiveWorkerCount at 1. EXPECT, not ASSERT, so the
    // release below always runs and the worker never outlives ctx.
    ASSERT_EQ(STATUS_SUCCESS, receiveLwsMessage(pSignalingClient, (PCHAR) kIceCandidateMessage, ARRAY_SIZE(kIceCandidateMessage)));
    {
        std::unique_lock<std::mutex> guard(ctx.lock);
        EXPECT_TRUE(ctx.cv.wait_for(guard, std::chrono::seconds(5), [&ctx] { return ctx.entered == 1; }))
            << "the receive worker did not enter the callback";
    }

    // What freeSignaling() does first, as seen by an API call that is already past its own shutdown check.
    ATOMIC_STORE_BOOL(&pSignalingClient->shutdown, TRUE);

    std::thread caller([pSignalingClient, &quiesceReturned] {
        terminateOngoingOperations(pSignalingClient, FALSE);
        quiesceReturned.store(true);
    });
    // The bounded path has nothing to wait for here (no listener, reconnecterTracker.terminated is TRUE from
    // construction), so it returns in milliseconds; 2 s is far above that and far below the 9 s a wrongly-selected
    // CVAR_WAIT on receiveWorkerCount would take before its first retry.
    for (UINT32 i = 0; i < 200 && !quiesceReturned.load(); i++) {
        THREAD_SLEEP(10 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
    }
    returnedInTime = quiesceReturned.load();

    // Release the worker either way, so neither thread outlives the client.
    {
        std::lock_guard<std::mutex> guard(ctx.lock);
        ctx.release = true;
        ctx.cv.notify_all();
    }
    caller.join();

    EXPECT_TRUE(returnedInTime) << "terminateOngoingOperations(FALSE) waited for a receive worker because shutdown was set";
    EXPECT_EQ(STATUS_SUCCESS, freeSignaling(&pSignalingClient));
    freeStaticCredentialProvider(&pCredentialProvider);
}

// Ported from PR #2397 (receiveMessageWorkerDoesNotAccessFreedClient) and adapted to the wait-based lifetime model:
// a worker that was dispatched before freeSignalingClient() but has not yet reached messageReceivedFn must not
// deliver its message to the application once the free has started.
struct ParkedWorkerHookContext {
    std::mutex lock;
    std::condition_variable cv;
    bool workerAtCallbackGate = false;
    bool release = false;
    std::atomic<UINT32> callbackCount{0};
};

// receiveMessagePreCallbackHookFn: park the worker right before the SDK decides whether to invoke messageReceivedFn.
static STATUS parkReceiveWorkerBeforeCallbackHook(UINT64 customData)
{
    ParkedWorkerHookContext* pCtx = (ParkedWorkerHookContext*) customData;
    std::unique_lock<std::mutex> guard(pCtx->lock);
    pCtx->workerAtCallbackGate = true;
    pCtx->cv.notify_all();
    pCtx->cv.wait(guard, [pCtx] { return pCtx->release; });
    return STATUS_SUCCESS;
}

static STATUS countingMessageReceivedFn(UINT64 customData, PReceivedSignalingMessage pReceivedSignalingMessage)
{
    UNUSED_PARAM(pReceivedSignalingMessage);
    ((ParkedWorkerHookContext*) customData)->callbackCount.fetch_add(1);
    return STATUS_SUCCESS;
}

TEST_F(ReceiveWorkerUafTest, receiveWorkerDoesNotDeliverMessageOnceFreeHasStarted)
{
    ParkedWorkerHookContext ctx;
    PAwsCredentialProvider pCredentialProvider = NULL;
    PSignalingClient pSignalingClient = NULL, pClient;
    STATUS freeStatus = STATUS_SUCCESS;

    ASSERT_EQ(STATUS_SUCCESS,
              createOfflineSignalingClient(mChannelName, mRegion, mCaCertPath, mLogLevel, (UINT64) &ctx, &pCredentialProvider, &pSignalingClient,
                                           countingMessageReceivedFn));
    ASSERT_NE((PSignalingClient) NULL, pSignalingClient);
    pClient = pSignalingClient;

    // The worker reads these when it starts, so setting them after create but before the first message is enough.
    pSignalingClient->clientInfo.hookCustomData = (UINT64) &ctx;
    pSignalingClient->clientInfo.receiveMessagePreCallbackHookFn = parkReceiveWorkerBeforeCallbackHook;

    // One worker, parked at the callback gate: counted, but messageReceivedFn has not run.
    ASSERT_EQ(STATUS_SUCCESS, receiveLwsMessage(pSignalingClient, (PCHAR) kIceCandidateMessage, ARRAY_SIZE(kIceCandidateMessage)));
    {
        std::unique_lock<std::mutex> guard(ctx.lock);
        ASSERT_TRUE(ctx.cv.wait_for(guard, std::chrono::seconds(5), [&ctx] { return ctx.workerAtCallbackGate; }))
            << "receive worker never reached the pre-callback hook";
    }
    EXPECT_EQ((SIZE_T) 1, ATOMIC_LOAD(&pSignalingClient->receiveWorkerCount));
    EXPECT_EQ(0U, ctx.callbackCount.load());

    // Free from the application thread while the worker is parked. It blocks until the worker exits
    // (freeSignalingWaitsForAllInFlightReceiveWorkers covers that); here we only need shutdown to be set.
    std::thread freeThread([&] { freeStatus = freeSignaling(&pSignalingClient); });
    EXPECT_TRUE(waitForFreeToStart(pClient)) << "freeSignaling() never set shutdown";

    // Release the worker. shutdown is set, so the SDK must skip messageReceivedFn and let the worker exit.
    {
        std::lock_guard<std::mutex> guard(ctx.lock);
        ctx.release = true;
        ctx.cv.notify_all();
    }
    freeThread.join();

    EXPECT_EQ(STATUS_SUCCESS, freeStatus);
    EXPECT_EQ((PSignalingClient) NULL, pSignalingClient);
    EXPECT_EQ(0U, ctx.callbackCount.load()) << "messageReceivedFn was invoked after freeSignalingClient() had started";

    freeStaticCredentialProvider(&pCredentialProvider);
}

} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com
