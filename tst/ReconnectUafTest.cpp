#include "WebRTCClientTestFixture.h"

namespace com {
namespace amazonaws {
namespace kinesis {
namespace video {
namespace webrtcclient {

class ReconnectUafTest : public WebRtcClientTestBase {};

// Simulates a stuck reconnectHandler thread (blocked in getaddrinfo).
// The thread holds reconnecterTracker.terminated = FALSE for `stallSeconds`
// (or until the test sets `stopRequested`), then sets it to TRUE and
// broadcasts the cvar — mimicking the real reconnectHandler's CleanUp block.
//
// The thread must be joined by the test before the signaling client is
// freed and before `FakeReconnectArgs` goes out of scope: it dereferences
// both. A detached copy of this thread outliving its test was itself a
// use-after-free that crashed unrelated tests ~20s later in CI.
struct FakeReconnectArgs {
    PSignalingClient pSignalingClient;
    UINT32 stallSeconds;
    volatile ATOMIC_BOOL started;
    volatile ATOMIC_BOOL stopRequested;
};

static PVOID fakeStuckReconnectHandler(PVOID args)
{
    auto* ctx = (FakeReconnectArgs*) args;
    PSignalingClient pSignalingClient = ctx->pSignalingClient;

    ATOMIC_STORE_BOOL(&ctx->started, TRUE);

    // Simulate getaddrinfo blocking for stallSeconds. Poll every 100ms so a
    // test that no longer needs the stall can release the thread early via
    // `stopRequested`; otherwise run the full stall like a real blocked call.
    UINT64 stallTime = ctx->stallSeconds * HUNDREDS_OF_NANOS_IN_A_SECOND;
    UINT64 elapsed = 0;
    UINT64 sleepInterval = 100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
    while (elapsed < stallTime && !ATOMIC_LOAD_BOOL(&ctx->stopRequested)) {
        THREAD_SLEEP(sleepInterval);
        elapsed += sleepInterval;
    }

    // Mimic reconnectHandler's CleanUp: set terminated and broadcast.
    // Take the tracker lock so awaitForThreadTermination() cannot observe
    // terminated=TRUE, return, and let the client be freed while we are still
    // inside CVAR_BROADCAST on its cvar.
    MUTEX_LOCK(pSignalingClient->reconnecterTracker.lock);
    ATOMIC_STORE_BOOL(&pSignalingClient->reconnecterTracker.terminated, TRUE);
    CVAR_BROADCAST(pSignalingClient->reconnecterTracker.await);
    MUTEX_UNLOCK(pSignalingClient->reconnecterTracker.lock);

    // Do not touch pSignalingClient after this point.
    return NULL;
}

static void startFakeReconnectThread(FakeReconnectArgs* pArgs, PSignalingClient pSignalingClient, UINT32 stallSeconds, PTID pThreadId)
{
    pArgs->pSignalingClient = pSignalingClient;
    pArgs->stallSeconds = stallSeconds;
    ATOMIC_STORE_BOOL(&pArgs->started, FALSE);
    ATOMIC_STORE_BOOL(&pArgs->stopRequested, FALSE);

    ATOMIC_STORE_BOOL(&pSignalingClient->reconnecterTracker.terminated, FALSE);

    ASSERT_EQ(STATUS_SUCCESS, THREAD_CREATE(pThreadId, fakeStuckReconnectHandler, (PVOID) pArgs));

    // Wait for the fake thread to actually start
    while (!ATOMIC_LOAD_BOOL(&pArgs->started)) {
        THREAD_SLEEP(10 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
    }
}

// Test: freeSignaling must not return while a reconnect thread is alive.
// Without the patch, freeSignaling returns after a 9s timeout and frees
// the signaling client while the thread is still running → UAF/SIGSEGV.
// With the patch, freeSignaling blocks until the thread sets terminated=TRUE.
TEST_F(ReconnectUafTest, freeSignalingWaitsForStuckReconnectThread)
{
    if (!mAccessKeyIdSet) {
        return;
    }

    initializeSignalingClient();
    ASSERT_TRUE(IS_VALID_SIGNALING_CLIENT_HANDLE(mSignalingClientHandle));

    PSignalingClient pSignalingClient = FROM_SIGNALING_CLIENT_HANDLE(mSignalingClientHandle);

    // Verify the reconnect tracker starts in terminated state
    ASSERT_TRUE(ATOMIC_LOAD_BOOL(&pSignalingClient->reconnecterTracker.terminated));

    // Simulate a stuck reconnect thread: set terminated=FALSE and spawn
    // a thread that sleeps for 12s (longer than SIGNALING_CLIENT_SHUTDOWN_TIMEOUT = 9s)
    // before setting terminated=TRUE.
    FakeReconnectArgs fakeArgs;
    TID threadId;
    startFakeReconnectThread(&fakeArgs, pSignalingClient, 12, &threadId);

    // Record the time before free
    UINT64 beforeFree = GETTIME();

    // This calls freeSignaling → terminateOngoingOperations.
    // With the patch: blocks until fake thread sets terminated=TRUE (~12s)
    // Without the patch: returns after 9s timeout, then frees memory → UAF
    deleteChannelLws(pSignalingClient, 0);
    EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&mSignalingClientHandle));

    UINT64 afterFree = GETTIME();
    UINT64 elapsedMs = (afterFree - beforeFree) / HUNDREDS_OF_NANOS_IN_A_MILLISECOND;

    // The fake thread stalls for 12s. freeSignaling must have waited at least
    // that long (minus some tolerance for scheduling). If it returned in <10s,
    // the patch is not working — it gave up and freed while the thread was alive.
    EXPECT_GE(elapsedMs, 11000) << "freeSignaling returned too early (" << elapsedMs
                                << " ms) — it did not wait for the reconnect thread. "
                                   "This would be a use-after-free on the unpatched code.";

    // If we get here without crashing, the patch is working correctly.
    // The signaling client handle is already freed, so skip deinitializeSignalingClient.
    mSignalingClientHandle = INVALID_SIGNALING_CLIENT_HANDLE_VALUE;

    // The fake thread is done with pSignalingClient once freeSignaling returned;
    // join it so it cannot outlive `fakeArgs` (stack) or this test.
    ASSERT_EQ(STATUS_SUCCESS, THREAD_JOIN(threadId, NULL));
}

// Test: non-free callers (fetchSync, disconnectSync) should NOT block
// indefinitely when a reconnect thread is alive. They use the bounded
// wait path since shutdown=FALSE.
TEST_F(ReconnectUafTest, nonFreeCallersUseBoundedWait)
{
    if (!mAccessKeyIdSet) {
        return;
    }

    initializeSignalingClient();
    ASSERT_TRUE(IS_VALID_SIGNALING_CLIENT_HANDLE(mSignalingClientHandle));

    PSignalingClient pSignalingClient = FROM_SIGNALING_CLIENT_HANDLE(mSignalingClientHandle);

    // Connect so we have a valid WSS connection
    EXPECT_EQ(STATUS_SUCCESS, signalingClientConnectSync(mSignalingClientHandle));

    // Simulate a stuck reconnect thread (30s stall — much longer than timeout)
    FakeReconnectArgs fakeArgs;
    TID threadId;
    startFakeReconnectThread(&fakeArgs, pSignalingClient, 30, &threadId);

    UINT64 beforeFetch = GETTIME();

    // fetchSync calls terminateOngoingOperations with shutdown=FALSE.
    // It should use the bounded wait and return within ~9s (SIGNALING_CLIENT_SHUTDOWN_TIMEOUT),
    // NOT block for 30s waiting for the fake thread.
    signalingClientFetchSync(mSignalingClientHandle);

    UINT64 afterFetch = GETTIME();
    UINT64 elapsedMs = (afterFetch - beforeFetch) / HUNDREDS_OF_NANOS_IN_A_MILLISECOND;

    // Should return within roughly the shutdown timeout (9s) + some overhead,
    // NOT wait for the full 30s stall.
    EXPECT_LT(elapsedMs, 20000) << "fetchSync blocked for " << elapsedMs
                                << " ms — it should have used the bounded wait path, not the unbounded shutdown path.";

    // Clean up: release the fake thread from its stall and wait for it to
    // finish (it sets terminated=TRUE itself on the way out). Only then is it
    // safe to free the signaling client — a thread still alive here would
    // write into freed memory once its 30s stall expired.
    ATOMIC_STORE_BOOL(&fakeArgs.stopRequested, TRUE);
    ASSERT_EQ(STATUS_SUCCESS, THREAD_JOIN(threadId, NULL));
    ASSERT_TRUE(ATOMIC_LOAD_BOOL(&pSignalingClient->reconnecterTracker.terminated));

    deinitializeSignalingClient();
}

} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com
