#include "WebRTCClientTestFixture.h"

namespace com {
namespace amazonaws {
namespace kinesis {
namespace video {
namespace webrtcclient {

class ReconnectUafTest : public WebRtcClientTestBase {};

// Simulates a stuck reconnectHandler thread (blocked in getaddrinfo).
// The thread holds reconnecterTracker.terminated = FALSE for `stallSeconds`,
// then sets it to TRUE and broadcasts the cvar — mimicking the real
// reconnectHandler's CleanUp block.
struct FakeReconnectArgs {
    PSignalingClient pSignalingClient;
    UINT32 stallSeconds;
    volatile ATOMIC_BOOL started;
};

static PVOID fakeStuckReconnectHandler(PVOID args)
{
    auto* ctx = (FakeReconnectArgs*) args;
    PSignalingClient pSignalingClient = ctx->pSignalingClient;

    ATOMIC_STORE_BOOL(&ctx->started, TRUE);

    // Simulate getaddrinfo blocking for stallSeconds.
    // Check shutdown every 100ms so we can verify the wait loop works
    // but don't exit early — the real getaddrinfo can't be interrupted.
    UINT64 stallTime = ctx->stallSeconds * HUNDREDS_OF_NANOS_IN_A_SECOND;
    UINT64 elapsed = 0;
    UINT64 sleepInterval = 100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
    while (elapsed < stallTime) {
        THREAD_SLEEP(sleepInterval);
        elapsed += sleepInterval;
    }

    // Mimic reconnectHandler's CleanUp: set terminated and broadcast
    ATOMIC_STORE_BOOL(&pSignalingClient->reconnecterTracker.terminated, TRUE);
    CVAR_BROADCAST(pSignalingClient->reconnecterTracker.await);

    return NULL;
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
    fakeArgs.pSignalingClient = pSignalingClient;
    fakeArgs.stallSeconds = 12;
    ATOMIC_STORE_BOOL(&fakeArgs.started, FALSE);

    ATOMIC_STORE_BOOL(&pSignalingClient->reconnecterTracker.terminated, FALSE);

    TID threadId;
    ASSERT_EQ(STATUS_SUCCESS, THREAD_CREATE(&threadId, fakeStuckReconnectHandler, (PVOID) &fakeArgs));
    ASSERT_EQ(STATUS_SUCCESS, THREAD_DETACH(threadId));

    // Wait for the fake thread to actually start
    while (!ATOMIC_LOAD_BOOL(&fakeArgs.started)) {
        THREAD_SLEEP(10 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
    }

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

    THREAD_SLEEP(100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
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
    fakeArgs.pSignalingClient = pSignalingClient;
    fakeArgs.stallSeconds = 30;
    ATOMIC_STORE_BOOL(&fakeArgs.started, FALSE);

    ATOMIC_STORE_BOOL(&pSignalingClient->reconnecterTracker.terminated, FALSE);

    TID threadId;
    ASSERT_EQ(STATUS_SUCCESS, THREAD_CREATE(&threadId, fakeStuckReconnectHandler, (PVOID) &fakeArgs));
    ASSERT_EQ(STATUS_SUCCESS, THREAD_DETACH(threadId));

    while (!ATOMIC_LOAD_BOOL(&fakeArgs.started)) {
        THREAD_SLEEP(10 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
    }

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

    // Clean up: let the fake thread finish, then free
    ATOMIC_STORE_BOOL(&pSignalingClient->reconnecterTracker.terminated, TRUE);
    CVAR_BROADCAST(pSignalingClient->reconnecterTracker.await);
    THREAD_SLEEP(1 * HUNDREDS_OF_NANOS_IN_A_SECOND);

    deinitializeSignalingClient();
    THREAD_SLEEP(100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
}

} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com
