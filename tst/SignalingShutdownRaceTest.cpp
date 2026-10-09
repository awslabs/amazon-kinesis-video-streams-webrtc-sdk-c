#include "WebRTCClientTestFixture.h"
#include <chrono>
#include <condition_variable>

#if defined(__has_feature)
#if __has_feature(address_sanitizer)
#define SIGNALING_SHUTDOWN_TEST_ASAN 1
#endif
#endif
#if defined(__SANITIZE_ADDRESS__)
#define SIGNALING_SHUTDOWN_TEST_ASAN 1
#endif

#ifdef SIGNALING_SHUTDOWN_TEST_ASAN
#include <sanitizer/asan_interface.h>
#include <libwebsockets.h>
#endif

namespace com {
namespace amazonaws {
namespace kinesis {
namespace video {
namespace webrtcclient {

// Regression tests for races between freeSignalingClient() and the signaling client's own threads (reconnect
// thread, WSS listener, receive workers) and lws callbacks. Each scenario reproduced a use-after-free, a hang or a
// wrong rejection before the corresponding fix and must pass now. All tests run offline unless noted: nothing
// listens on 127.0.0.1:443 (the SDK hard-codes port 443), so every HTTPS/WSS call the state machine makes fails
// fast. One test is kept DISABLED_ as executable documentation of an application-side pattern the SDK cannot detect
// (freeing from stateChangeFn on the caller's own thread); run it with --gtest_also_run_disabled_tests.
class SignalingShutdownRaceTest : public WebRtcClientTestBase {};

static const CHAR kShutdownRaceIceCandidateMessage[] = "{\n"
                                                 "    \"messageType\": \"ICE_CANDIDATE\",\n"
                                                 "    \"senderClientId\": \"ClientA\",\n"
                                                 "    \"messagePayload\": \"SGVsbG8=\"\n"
                                                 "}";

static const CHAR kShutdownRaceMalformedMessage[] = "{ this is not a signaling message";

#define SHUTDOWN_RACE_POLL_INTERVAL (5 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND)

// Polls pred every 5 ms until it returns true or timeoutMs elapses.
template <typename Pred> static bool waitFor(Pred pred, UINT32 timeoutMs)
{
    UINT64 deadline = GETTIME() + (UINT64) timeoutMs * HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
    while (!pred()) {
        if (GETTIME() > deadline) {
            return false;
        }
        THREAD_SLEEP(SHUTDOWN_RACE_POLL_INTERVAL);
    }
    return true;
}

static STATUS noopMessageReceivedFn(UINT64 customData, PReceivedSignalingMessage pReceivedSignalingMessage)
{
    UNUSED_PARAM(customData);
    UNUSED_PARAM(pReceivedSignalingMessage);
    return STATUS_SUCCESS;
}

struct ShutdownRaceClientOptions {
    UINT64 customData = 0;
    SignalingClientMessageReceivedFunc messageReceivedFn = noopMessageReceivedFn;
    SignalingClientStateChangedFunc stateChangeFn = NULL;
    SignalingClientErrorReportFunc errorReportFn = NULL;
    // NULL uses the real regional control plane endpoint.
    PCHAR pControlPlaneUrl = (PCHAR) "https://127.0.0.1";
};

// Same shape as createOfflineSignalingClient() in ReceiveWorkerUafTest.cpp: createSignalingSync() only drives the
// state machine to GET_TOKEN, which the static credential provider serves locally.
static STATUS createShutdownRaceClient(PCHAR channelName, PCHAR region, PCHAR certPath, UINT32 logLevel, const ShutdownRaceClientOptions& options,
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
    signalingClientCallbacks.customData = options.customData;
    signalingClientCallbacks.messageReceivedFn = options.messageReceivedFn;
    signalingClientCallbacks.stateChangeFn = options.stateChangeFn;
    signalingClientCallbacks.errorReportFn = options.errorReportFn;

    MEMSET(&channelInfo, 0x00, SIZEOF(ChannelInfo));
    channelInfo.version = CHANNEL_INFO_CURRENT_VERSION;
    channelInfo.pChannelName = channelName;
    channelInfo.channelType = SIGNALING_CHANNEL_TYPE_SINGLE_MASTER;
    channelInfo.channelRoleType = SIGNALING_CHANNEL_ROLE_TYPE_MASTER;
    channelInfo.cachingPolicy = SIGNALING_API_CALL_CACHE_TYPE_NONE;
    // No state machine retries: a failed call ends the iteration immediately.
    channelInfo.retry = FALSE;
    channelInfo.reconnect = TRUE;
    channelInfo.pRegion = region;
    channelInfo.pCertPath = certPath;
    channelInfo.messageTtl = TEST_SIGNALING_MESSAGE_TTL;
    channelInfo.pControlPlaneUrl = options.pControlPlaneUrl;

    CHK_STATUS(createStaticCredentialProvider((PCHAR) "accessKey", 0, (PCHAR) "secretKey", 0, NULL, 0, MAX_UINT64, ppCredentialProvider));
    CHK_STATUS(createSignalingSync(&clientInfoInternal, &channelInfo, &signalingClientCallbacks, *ppCredentialProvider, ppSignalingClient));

CleanUp:
    if (STATUS_FAILED(retStatus)) {
        freeSignaling(ppSignalingClient);
        freeStaticCredentialProvider(ppCredentialProvider);
    }

    return retStatus;
}

static UINT32 liveReconnectThreads(PSignalingClient pSignalingClient)
{
    UINT32 count;
    MUTEX_LOCK(pSignalingClient->reconnecterTracker.lock);
    count = pSignalingClient->reconnectThreadCount;
    MUTEX_UNLOCK(pSignalingClient->reconnecterTracker.lock);
    return count;
}

// Runs freeSignaling() on its own thread. Heap-allocated: a test leaks it when the free never returns.
struct AsyncFree {
    PSignalingClient pSignalingClient = NULL;
    std::atomic<bool> done{false};
    std::atomic<STATUS> status{STATUS_SUCCESS};
};

static void startAsyncFree(AsyncFree* pAsyncFree)
{
    std::thread([pAsyncFree] {
        PSignalingClient pSignalingClient = pAsyncFree->pSignalingClient;
        pAsyncFree->status = freeSignaling(&pSignalingClient);
        pAsyncFree->done = true;
    }).detach();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HTTPS call exit vs. a wsi lws has already freed: lwsCompleteSync()'s exit block must not call
// lws_set_opaque_user_data() on a wsi that lws freed inside the service loop.
//
// Every normal HTTPS completion (or connection error) sets terminating=TRUE from the CLOSED_CLIENT_HTTP /
// CLIENT_CONNECTION_ERROR callback, and lws frees the wsi right after that callback, inside the same lws_service()
// call. Nothing clears currentWsi on close, so the exit block of lwsCompleteSync() must not touch the wsi on a
// terminating exit (it did before the fix: a write-after-free on every HTTPS call).
//
// lws is not ASan-instrumented, so ASan does not flag the write itself. Instead a watcher thread records the wsi
// published in currentWsi[PROTOCOL_INDEX_HTTPS], and once describeChannelLws() returns we ask ASan whether that
// memory was already freed. After lwsCompleteSync()'s exit block the SDK sets currentWsi to NULL and nothing
// services that wsi again, so "freed when the call returns" means it was freed during the service loop, before
// the exit block wrote to it.
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct HttpsWsiWatcher {
    PSignalingClient pSignalingClient = NULL;
    std::atomic<bool> stop{false};
    std::atomic<PVOID> lastWsi{NULL};
};

static void watchHttpsWsi(HttpsWsiWatcher* pWatcher)
{
    volatile PVOID* pCurrentWsi = (volatile PVOID*) pWatcher->pSignalingClient->currentWsi;
    while (!pWatcher->stop.load()) {
        PVOID wsi = pCurrentWsi[PROTOCOL_INDEX_HTTPS];
        if (wsi != NULL) {
            pWatcher->lastWsi = wsi;
        }
        std::this_thread::yield();
    }
}

static void runHttpsWsiWriteAfterFreeProbe(PCHAR channelName, PCHAR region, PCHAR certPath, UINT32 logLevel, PCHAR pControlPlaneUrl)
{
#ifndef SIGNALING_SHUTDOWN_TEST_ASAN
    UNUSED_PARAM(channelName);
    UNUSED_PARAM(region);
    UNUSED_PARAM(certPath);
    UNUSED_PARAM(logLevel);
    UNUSED_PARAM(pControlPlaneUrl);
    GTEST_SKIP() << "Needs an AddressSanitizer build (-DADDRESS_SANITIZER=ON) to tell whether the wsi was freed";
#else
    ShutdownRaceClientOptions options;
    PAwsCredentialProvider pCredentialProvider = NULL;
    PSignalingClient pSignalingClient = NULL;
    HttpsWsiWatcher watcher;
    STATUS describeStatus;
    PVOID wsi;

    options.pControlPlaneUrl = pControlPlaneUrl;
    ASSERT_EQ(STATUS_SUCCESS, createShutdownRaceClient(channelName, region, certPath, logLevel, options, &pCredentialProvider, &pSignalingClient));

    watcher.pSignalingClient = pSignalingClient;
    std::thread watcherThread(watchHttpsWsi, &watcher);
    describeStatus = describeChannelLws(pSignalingClient, GETTIME());
    watcher.stop = true;
    watcherThread.join();

    wsi = watcher.lastWsi.load();
    DLOGI("describeChannelLws returned 0x%08x, last HTTPS wsi %p", describeStatus, wsi);
    EXPECT_EQ((PVOID) NULL, pSignalingClient->currentWsi[PROTOCOL_INDEX_HTTPS]);
    if (wsi == NULL) {
        // Either lws_client_connect_via_info() failed synchronously, or the connection was refused and closed
        // before the sampling thread ever saw the pointer published. Neither reaches the state the probe is about.
        EXPECT_EQ(STATUS_SUCCESS, freeSignaling(&pSignalingClient));
        freeStaticCredentialProvider(&pCredentialProvider);
        GTEST_SKIP() << "No HTTPS wsi was observed during the call; the write-after-free probe does not apply to this run";
    }

    // Precondition, not the property under test: lws frees the wsi inside lws_service() on the close callback, i.e. before
    // lwsCompleteSync()'s exit block runs. If this ever stops holding the probe below is not applicable.
    bool freedInsideServiceLoop = __asan_address_is_poisoned(wsi) != 0;
    if (!freedInsideServiceLoop) {
        EXPECT_EQ(STATUS_SUCCESS, freeSignaling(&pSignalingClient));
        freeStaticCredentialProvider(&pCredentialProvider);
        GTEST_SKIP() << "lws did not free the wsi inside the service loop; the write-after-free probe does not apply";
    }
    __asan_describe_address(wsi);

    // The property under test: the exit block must not write into that freed wsi. ASan keeps freed blocks poisoned in its
    // quarantine without reusing or scribbling them, so the wsi still holds whatever was last written to it.
    // lws stored the LwsCallInfo pointer in a.opaque_user_data at connect and never clears it on close; the
    // unfixed exit block overwrote it with NULL after the free. lws is not ASan-instrumented, so reading the
    // quarantined field through lws_get_opaque_user_data() is not itself reported.
    PVOID opaqueAfterFree = lws_get_opaque_user_data((struct lws*) wsi);
    EXPECT_NE((PVOID) NULL, opaqueAfterFree) << "currentWsi[HTTPS] (" << wsi
                                             << ") was freed by lws inside lws_service() and lwsCompleteSync()'s exit block then wrote "
                                                "NULL into its opaque_user_data: a heap write-after-free on every HTTPS call";

    EXPECT_EQ(STATUS_SUCCESS, freeSignaling(&pSignalingClient));
    freeStaticCredentialProvider(&pCredentialProvider);
#endif
}

// Offline: the connection to 127.0.0.1:443 is refused, so lws delivers CLIENT_CONNECTION_ERROR and frees the wsi.
TEST_F(SignalingShutdownRaceTest, httpsCallDoesNotWriteToFreedWsiOnConnectionRefused)
{
    runHttpsWsiWriteAfterFreeProbe(mChannelName, mRegion, mCaCertPath, mLogLevel, (PCHAR) "https://127.0.0.1");
}

// Needs network access, not credentials: the fake keys get a 403 from the real endpoint. That is a normal HTTP
// completion (CLOSED_CLIENT_HTTP), which is the path every successful API call takes.
TEST_F(SignalingShutdownRaceTest, httpsCallDoesNotWriteToFreedWsiOnNormalCompletion)
{
    runHttpsWsiWriteAfterFreeProbe(mChannelName, mRegion, mCaCertPath, mLogLevel, NULL);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Application holds a lock across free that its message callback also takes. freeSignaling() waits with no time
// limit for in-flight messageReceivedFn calls, so if the caller holds a lock the callback needs, free can only
// return once the callback gives up on that lock. The SDK's own sample used to do exactly this on its recreate path:
//   sessionCleanupWait():       MUTEX_LOCK(sampleConfigurationObjLock) ... freeSignalingClient()   (recreate path)
//   signalingMessageReceived(): MUTEX_LOCK(sampleConfigurationObjLock)
// A plain MUTEX_LOCK in the callback would hang forever (that is the deadlock an application sees), so here the
// callback gives up after kAppLockTimeoutSeconds. The test asserts the SDK side of the contract: free does NOT
// return early and free the client under the blocked callback; it returns only after the callback has finished,
// i.e. after it gave up. Returning any sooner would be the use-after-free this file guards against. The
// application side of the contract is documented in Include.h: do not hold a lock across freeSignalingClient()
// that a signaling callback also takes. The sample's recreate path was fixed accordingly.
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static const UINT32 kAppLockTimeoutSeconds = 5;

struct AppLockContext {
    std::timed_mutex appLock; // stands in for sampleConfigurationObjLock
    std::atomic<bool> callbackEntered{false};
    std::atomic<bool> callbackGaveUp{false};
};

static STATUS appLockingMessageReceivedFn(UINT64 customData, PReceivedSignalingMessage pReceivedSignalingMessage)
{
    UNUSED_PARAM(pReceivedSignalingMessage);
    AppLockContext* pCtx = (AppLockContext*) customData;

    pCtx->callbackEntered = true;
    if (!pCtx->appLock.try_lock_for(std::chrono::seconds(kAppLockTimeoutSeconds))) {
        pCtx->callbackGaveUp = true;
        return STATUS_SUCCESS;
    }
    pCtx->appLock.unlock();
    return STATUS_SUCCESS;
}

TEST_F(SignalingShutdownRaceTest, freeWaitsForCallbackBlockedOnApplicationLock)
{
    AppLockContext ctx;
    ShutdownRaceClientOptions options;
    PAwsCredentialProvider pCredentialProvider = NULL;
    PSignalingClient pSignalingClient = NULL;
    UINT64 start, elapsedMs;
    STATUS freeStatus;
    bool gaveUp;

    options.customData = (UINT64) &ctx;
    options.messageReceivedFn = appLockingMessageReceivedFn;
    ASSERT_EQ(STATUS_SUCCESS, createShutdownRaceClient(mChannelName, mRegion, mCaCertPath, mLogLevel, options, &pCredentialProvider, &pSignalingClient));

    // Like sessionCleanupWait(): hold the app lock, then free the client.
    ctx.appLock.lock();
    ASSERT_EQ(STATUS_SUCCESS, receiveLwsMessage(pSignalingClient, (PCHAR) kShutdownRaceIceCandidateMessage, ARRAY_SIZE(kShutdownRaceIceCandidateMessage)));
    ASSERT_TRUE(waitFor([&ctx] { return ctx.callbackEntered.load(); }, 5000));

    start = GETTIME();
    freeStatus = freeSignaling(&pSignalingClient);
    elapsedMs = (GETTIME() - start) / HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
    gaveUp = ctx.callbackGaveUp.load();
    ctx.appLock.unlock();

    EXPECT_EQ(STATUS_SUCCESS, freeStatus);
    EXPECT_TRUE(gaveUp) << "freeSignaling() returned after " << elapsedMs << " ms while messageReceivedFn was still blocked on the application lock";
    EXPECT_GE(elapsedMs, (UINT64) kAppLockTimeoutSeconds * 1000 - 500)
        << "freeSignaling() returned after " << elapsedMs << " ms, before the blocked callback could have finished";
    freeStaticCredentialProvider(&pCredentialProvider);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// free from errorReportFn on the WSS listener thread: Include.h promises STATUS_INVALID_OPERATION when
// freeSignalingClient() is called from errorReportFn,
// but isSignalingOwnedThread() only knows the reconnect thread and receive workers. errorReportFn also fires on
// the WSS listener thread: lwsListenerHandler() holds listenerTracker.lock across lwsCompleteSync()
// for its whole life, lws_service() on that thread delivers CLIENT_RECEIVE -> receiveLwsMessage(), and a malformed
// message makes receiveLwsMessage() call errorReportFn on that same thread.
//
// The emulated listener below sets up exactly that lock state and then makes the real receiveLwsMessage() call.
// freeSignaling() -> terminateLwsListenerLoop() -> awaitForThreadTermination(&listenerTracker) re-locks the
// non-recursive listenerTracker.lock that this thread already holds.
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ListenerErrorContext {
    PSignalingClient pSignalingClient = NULL;
    std::atomic<bool> freeCalled{false};
    std::atomic<bool> freeReturned{false};
    std::atomic<STATUS> freeStatus{STATUS_SUCCESS};
};

static STATUS freeFromErrorReportFn(UINT64 customData, STATUS status, PCHAR msg, UINT32 msgLen)
{
    UNUSED_PARAM(status);
    UNUSED_PARAM(msg);
    UNUSED_PARAM(msgLen);
    ListenerErrorContext* pCtx = (ListenerErrorContext*) customData;
    PSignalingClient pSignalingClient = pCtx->pSignalingClient;

    pCtx->freeCalled = true;
    pCtx->freeStatus = freeSignaling(&pSignalingClient);
    pCtx->freeReturned = true;
    return STATUS_SUCCESS;
}

static void emulatedListenerThread(ListenerErrorContext* pCtx)
{
    PSignalingClient pSignalingClient = pCtx->pSignalingClient;

    // lwsListenerHandler() prologue
    MUTEX_LOCK(pSignalingClient->listenerTracker.lock);
    ATOMIC_STORE(&pSignalingClient->listenerThreadTid, SIGNALING_CURRENT_THREAD_ID());
    ATOMIC_STORE_BOOL(&pSignalingClient->listenerTracker.terminated, FALSE);

    // lws_service() -> lwsWssCallbackRoutine(LWS_CALLBACK_CLIENT_RECEIVE) -> receiveLwsMessage()
    receiveLwsMessage(pSignalingClient, (PCHAR) kShutdownRaceMalformedMessage, ARRAY_SIZE(kShutdownRaceMalformedMessage));

    if (pCtx->freeStatus.load() == STATUS_SUCCESS) {
        // The client was freed underneath this "listener"; do not touch it.
        return;
    }

    // lwsListenerHandler() epilogue
    ATOMIC_STORE(&pSignalingClient->listenerThreadTid, 0);
    ATOMIC_STORE_BOOL(&pSignalingClient->listenerTracker.terminated, TRUE);
    CVAR_BROADCAST(pSignalingClient->listenerTracker.await);
    MUTEX_UNLOCK(pSignalingClient->listenerTracker.lock);
}

TEST_F(SignalingShutdownRaceTest, freeFromErrorReportFnOnListenerThreadIsRejected)
{
    // Leaked on purpose if the listener thread deadlocks.
    ListenerErrorContext* pCtx = new ListenerErrorContext();
    ShutdownRaceClientOptions options;
    PAwsCredentialProvider pCredentialProvider = NULL;
    PSignalingClient pSignalingClient = NULL;

    options.customData = (UINT64) pCtx;
    options.errorReportFn = freeFromErrorReportFn;
    ASSERT_EQ(STATUS_SUCCESS, createShutdownRaceClient(mChannelName, mRegion, mCaCertPath, mLogLevel, options, &pCredentialProvider, &pSignalingClient));
    pCtx->pSignalingClient = pSignalingClient;

    // A live listener always has an ongoing call info; terminateLwsListenerLoop() only acts when it is set.
    pSignalingClient->pOngoingCallInfo = (PLwsCallInfo) MEMCALLOC(1, SIZEOF(LwsCallInfo));
    ASSERT_NE((PLwsCallInfo) NULL, pSignalingClient->pOngoingCallInfo);

    std::thread(emulatedListenerThread, pCtx).detach();

    ASSERT_TRUE(waitFor([pCtx] { return pCtx->freeCalled.load(); }, 5000));
    bool returned = waitFor([pCtx] { return pCtx->freeReturned.load(); }, 15000);

    EXPECT_TRUE(returned) << "freeSignalingClient() called from errorReportFn on the listener thread never returned: it is waiting on "
                             "listenerTracker.lock, which that same thread holds (self-deadlock)";
    if (returned) {
        EXPECT_EQ(STATUS_INVALID_OPERATION, pCtx->freeStatus.load()) << "Include.h documents STATUS_INVALID_OPERATION for this call";
        if (pCtx->freeStatus.load() != STATUS_SUCCESS) {
            THREAD_SLEEP(100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
            MEMFREE(pSignalingClient->pOngoingCallInfo);
            pSignalingClient->pOngoingCallInfo = NULL;
            EXPECT_EQ(STATUS_SUCCESS, freeSignaling(&pSignalingClient));
        }
        freeStaticCredentialProvider(&pCredentialProvider);
        delete pCtx;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// free from stateChangeFn on the caller's own thread: stateChangeFn runs on the application's own thread inside signalingClientFetchSync() /
// signalingClientConnectSync() (signalingStateMachineIterator() runs on the caller's thread). Calling
// freeSignalingClient() from there is not rejected; it frees the state machine, stateLock and the client while
// executeDescribeSignalingState() is still running on this stack.
//
// DISABLED_: the caller's own thread cannot be told apart from any other application thread by a thread-id check,
// so this case is documented in Include.h rather than detected (stateChangeFn / errorReportFn invoked inside a
// signaling API call must not free the client). The free returns STATUS_SUCCESS and the API call then runs on
// freed memory; under ASan the process aborts with heap-use-after-free inside signalingFetchSync() before the
// assertions below are reached. Kept, disabled, as executable documentation of that consequence.
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct StateChangeFreeContext {
    PSignalingClient pSignalingClient = NULL;
    std::atomic<bool> armed{false};
    std::atomic<bool> freeCalled{false};
    std::atomic<STATUS> freeStatus{STATUS_SUCCESS};
};

static STATUS freeOnDescribeStateChangeFn(UINT64 customData, SIGNALING_CLIENT_STATE state)
{
    StateChangeFreeContext* pCtx = (StateChangeFreeContext*) customData;
    PSignalingClient pSignalingClient;

    if (pCtx->armed.load() && state == SIGNALING_CLIENT_STATE_DESCRIBE && !pCtx->freeCalled.exchange(true)) {
        pSignalingClient = pCtx->pSignalingClient;
        pCtx->freeStatus = freeSignaling(&pSignalingClient);
        DLOGI("freeSignaling() from stateChangeFn returned 0x%08x", pCtx->freeStatus.load());
    }
    return STATUS_SUCCESS;
}

TEST_F(SignalingShutdownRaceTest, DISABLED_freeFromStateChangeFnOnCallerThreadIsUndetected)
{
#ifndef SIGNALING_SHUTDOWN_TEST_ASAN
    GTEST_SKIP() << "Frees the client underneath signalingFetchSync(); only safe to run under AddressSanitizer";
#else
    StateChangeFreeContext ctx;
    ShutdownRaceClientOptions options;
    PAwsCredentialProvider pCredentialProvider = NULL;
    PSignalingClient pSignalingClient = NULL;

    options.customData = (UINT64) &ctx;
    options.stateChangeFn = freeOnDescribeStateChangeFn;
    ASSERT_EQ(STATUS_SUCCESS, createShutdownRaceClient(mChannelName, mRegion, mCaCertPath, mLogLevel, options, &pCredentialProvider, &pSignalingClient));
    ctx.pSignalingClient = pSignalingClient;
    ctx.armed = true;

    // Under ASan this call aborts with heap-use-after-free on the client freed by the callback.
    signalingFetchSync(pSignalingClient);

    // Not reached under ASan. Without it, documents that the free was accepted (undetected) and the client is gone.
    EXPECT_TRUE(ctx.freeCalled.load());
    EXPECT_EQ(STATUS_SUCCESS, ctx.freeStatus.load());
    freeStaticCredentialProvider(&pCredentialProvider);
#endif
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Two overlapping reconnect threads: reconnecterTracker.terminated is one boolean, but two reconnect threads
// can be alive at once. Whichever exits
// first stores terminated=TRUE, and freeSignaling() then frees the client under the one still running.
//
// Both threads are the real reconnectHandler(), spawned the way the WSS callback spawns them. Each one fails its
// reconnect (127.0.0.1 refuses) and parks in errorReportFn, which reconnectHandler() calls from its CleanUp just
// before publishing terminated. Releasing only the first thread shows the tracker
// reporting "no reconnect thread" while the second is still inside reconnectHandler().
//
// Real-world trigger: reconnect thread A is still running (e.g. JOIN_SESSION after CONNECTED with media storage
// enabled, or the tail of its CleanUp) when the new WSS connection drops and CLIENT_CLOSED spawns thread B.
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct ReconnectParkingContext {
    std::atomic<UINT32> arrivals{0};
    std::atomic<bool> parked[2] = {{false}, {false}};
    std::atomic<bool> release[2] = {{false}, {false}};
};

static STATUS parkingErrorReportFn(UINT64 customData, STATUS status, PCHAR msg, UINT32 msgLen)
{
    UNUSED_PARAM(status);
    UNUSED_PARAM(msg);
    UNUSED_PARAM(msgLen);
    ReconnectParkingContext* pCtx = (ReconnectParkingContext*) customData;
    UINT32 idx = pCtx->arrivals.fetch_add(1);

    if (idx < 2) {
        pCtx->parked[idx] = true;
        while (!pCtx->release[idx].load()) {
            THREAD_SLEEP(SHUTDOWN_RACE_POLL_INTERVAL);
        }
    }
    return STATUS_SUCCESS;
}

// Set KVS_TEST_RELEASE_PARKED_THREAD_AFTER_FREE=1 under ASan to let a parked thread run on the freed client and get the report.
// Otherwise such threads stay parked forever so they never touch freed memory.
static bool releaseAfterFreeRequested()
{
    return getenv("KVS_TEST_RELEASE_PARKED_THREAD_AFTER_FREE") != NULL;
}

TEST_F(SignalingShutdownRaceTest, freeWaitsForSecondReconnectThreadAfterFirstExits)
{
    // Leaked on purpose when thread B is left parked on a freed client.
    ReconnectParkingContext* pCtx = new ReconnectParkingContext();
    AsyncFree* pAsyncFree = new AsyncFree();
    ShutdownRaceClientOptions options;
    PAwsCredentialProvider pCredentialProvider = NULL;
    PSignalingClient pSignalingClient = NULL;
    bool freeReturned;

    options.customData = (UINT64) pCtx;
    options.errorReportFn = parkingErrorReportFn;
    ASSERT_EQ(STATUS_SUCCESS, createShutdownRaceClient(mChannelName, mRegion, mCaCertPath, mLogLevel, options, &pCredentialProvider, &pSignalingClient));

    // Reconnect thread A
    ASSERT_EQ(STATUS_SUCCESS, startReconnectHandler(pSignalingClient));
    ASSERT_TRUE(waitFor([pCtx] { return pCtx->parked[0].load(); }, 10000));

    // Reconnect thread B, spawned while A is still inside reconnectHandler()
    ASSERT_EQ(STATUS_SUCCESS, startReconnectHandler(pSignalingClient));
    ASSERT_TRUE(waitFor([pCtx] { return pCtx->parked[1].load(); }, 10000));

    // A finishes. With the fix it only decrements the live count; terminated must stay FALSE while B is alive.
    pCtx->release[0] = true;
    ASSERT_TRUE(waitFor([pSignalingClient] { return liveReconnectThreads(pSignalingClient) <= 1; }, 5000));
    THREAD_SLEEP(100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);

    EXPECT_FALSE(ATOMIC_LOAD_BOOL(&pSignalingClient->reconnecterTracker.terminated))
        << "reconnecterTracker reports no live reconnect thread while reconnect thread B is still inside reconnectHandler()";

    pAsyncFree->pSignalingClient = pSignalingClient;
    startAsyncFree(pAsyncFree);
    freeReturned = waitFor([pAsyncFree] { return pAsyncFree->done.load(); }, 3000);
    EXPECT_FALSE(freeReturned) << "freeSignaling() returned and freed the client while reconnect thread B was still running. When B resumes, it "
                                  "locks reconnecterTracker.lock in freed memory.";

    if (freeReturned) {
        if (releaseAfterFreeRequested()) {
            pCtx->release[1] = true;
            THREAD_SLEEP(2 * HUNDREDS_OF_NANOS_IN_A_SECOND);
        }
    } else {
        pCtx->release[1] = true;
        EXPECT_TRUE(waitFor([pAsyncFree] { return pAsyncFree->done.load(); }, 10000));
        delete pCtx;
        delete pAsyncFree;
    }
    freeStaticCredentialProvider(&pCredentialProvider);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Message delivered after the shutdown drain: receiveLwsMessage() used to count and spawn a worker without
// checking shutdown. A CLIENT_RECEIVE delivered
// after the drain in terminateOngoingOperations() saw zero starts a worker that keeps running after the client is
// freed.
//
// The test plays a thread inside lws_service(): lwsWssCallbackRoutine() holds lwsServiceLock when it calls
// receiveLwsMessage(). freeSignaling() takes lwsServiceLock right after the drain (for lws_context_destroy()),
// so holding it pins free between "drain saw 0" and lws_context_destroy().
//
// Reachability: something must still be inside lws_service() at that point, e.g. a listener that outlived the
// bounded 9 s listener wait in terminateConnectionWithStatus(), or a listener started by the reconnect thread during the shutdown wait.
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct LateReceiveContext {
    std::atomic<UINT32> entered{0};
    std::atomic<bool> releaseFirst{false};
    AsyncFree* pAsyncFree = NULL;
    std::atomic<bool> secondRanDuringShutdown{false};
    std::atomic<bool> secondStillRunningAfterFree{false};
};

static STATUS lateReceiveMessageReceivedFn(UINT64 customData, PReceivedSignalingMessage pReceivedSignalingMessage)
{
    UNUSED_PARAM(pReceivedSignalingMessage);
    LateReceiveContext* pCtx = (LateReceiveContext*) customData;

    if (pCtx->entered.fetch_add(1) == 0) {
        // First worker: keeps the shutdown drain waiting until the test releases it.
        while (!pCtx->releaseFirst.load()) {
            THREAD_SLEEP(SHUTDOWN_RACE_POLL_INTERVAL);
        }
        return STATUS_SUCCESS;
    }

    // Second worker: spawned after the drain saw zero.
    pCtx->secondRanDuringShutdown = true;
    if (waitFor([pCtx] { return pCtx->pAsyncFree->done.load(); }, 5000)) {
        pCtx->secondStillRunningAfterFree = true;
        while (!releaseAfterFreeRequested()) {
            THREAD_SLEEP(HUNDREDS_OF_NANOS_IN_A_SECOND);
        }
    }
    // Under ASan, receiveLwsMessageWrapper() now locks the freed receiveWorkerLock.
    return STATUS_SUCCESS;
}

TEST_F(SignalingShutdownRaceTest, messageReceivedAfterShutdownDrainDoesNotSpawnWorker)
{
    // Leaked on purpose only on the failure path, where the second worker may be left parked on the freed client.
    LateReceiveContext* pCtx = new LateReceiveContext();
    ShutdownRaceClientOptions options;
    PAwsCredentialProvider pCredentialProvider = NULL;
    PSignalingClient pSignalingClient = NULL;
    STATUS lateReceiveStatus;
    bool freeReturned;

    pCtx->pAsyncFree = new AsyncFree();
    options.customData = (UINT64) pCtx;
    options.messageReceivedFn = lateReceiveMessageReceivedFn;
    ASSERT_EQ(STATUS_SUCCESS, createShutdownRaceClient(mChannelName, mRegion, mCaCertPath, mLogLevel, options, &pCredentialProvider, &pSignalingClient));

    // Worker 1 holds the drain open.
    ASSERT_EQ(STATUS_SUCCESS, receiveLwsMessage(pSignalingClient, (PCHAR) kShutdownRaceIceCandidateMessage, ARRAY_SIZE(kShutdownRaceIceCandidateMessage)));
    ASSERT_TRUE(waitFor([pCtx] { return pCtx->entered.load() == 1; }, 5000));

    pCtx->pAsyncFree->pSignalingClient = pSignalingClient;
    startAsyncFree(pCtx->pAsyncFree);
    ASSERT_TRUE(waitFor([pSignalingClient] { return ATOMIC_LOAD_BOOL(&pSignalingClient->shutdown) == TRUE; }, 5000));
    // free is now parked in the receive-worker drain, past its lwsServiceLock re-verify.
    THREAD_SLEEP(300 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);

    MUTEX_LOCK(pSignalingClient->lwsServiceLock);
    pCtx->releaseFirst = true;
    EXPECT_TRUE(waitFor([pSignalingClient] { return ATOMIC_LOAD(&pSignalingClient->receiveWorkerCount) == 0; }, 5000));
    // Let free observe 0 and block on lwsServiceLock in front of lws_context_destroy().
    THREAD_SLEEP(300 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
    EXPECT_FALSE(pCtx->pAsyncFree->done.load());

    // A CLIENT_RECEIVE delivered now
    lateReceiveStatus = receiveLwsMessage(pSignalingClient, (PCHAR) kShutdownRaceIceCandidateMessage, ARRAY_SIZE(kShutdownRaceIceCandidateMessage));
    MUTEX_UNLOCK(pSignalingClient->lwsServiceLock);

    freeReturned = waitFor([pCtx] { return pCtx->pAsyncFree->done.load(); }, 10000);
    EXPECT_TRUE(freeReturned);
    // Give the late worker time to observe free's return.
    waitFor([pCtx] { return pCtx->secondStillRunningAfterFree.load(); }, 2000);

    DLOGI("receiveLwsMessage() after the drain returned 0x%08x", lateReceiveStatus);
    EXPECT_FALSE(pCtx->secondRanDuringShutdown.load()) << "receiveLwsMessage() started a new receive worker after the shutdown drain had seen zero";
    EXPECT_FALSE(pCtx->secondStillRunningAfterFree.load())
        << "freeSignaling() returned while that worker was still running; on exit it locks receiveWorkerLock in freed memory";
    freeStaticCredentialProvider(&pCredentialProvider);

    // Pass path: free has returned and no second worker was ever spawned, so nothing can still reference the context.
    if (freeReturned && !pCtx->secondRanDuringShutdown.load()) {
        delete pCtx->pAsyncFree;
        delete pCtx;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Recycled reconnect thread id: reconnectThreadTid used to keep the id of the last reconnect thread after it exited, and pthreads
// recycles ids of exited threads. While the WSS callback was spawning the next reconnect thread (terminated=FALSE
// but the new thread not yet at its first statement), an unrelated app thread that had inherited the old id was
// told it was calling from a signaling callback.
//
// The fix clears the slot when the last reconnect thread exits, so (a) the slot must read 0 after the thread is
// gone and (b) an app thread with the recycled id must be able to free the client while a new reconnect thread is
// starting up. The exited thread's id is captured from inside errorReportFn, which runs on that thread.
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct TidReuseContext {
    PSignalingClient pSignalingClient = NULL;
    std::atomic<SIZE_T> reconnectTid{0};
    BOOL reused = FALSE;
    BOOL freed = FALSE;
    STATUS freeStatus = STATUS_SUCCESS;
};

static STATUS recordTidErrorReportFn(UINT64 customData, STATUS status, PCHAR msg, UINT32 msgLen)
{
    UNUSED_PARAM(status);
    UNUSED_PARAM(msg);
    UNUSED_PARAM(msgLen);
    ((TidReuseContext*) customData)->reconnectTid = SIGNALING_CURRENT_THREAD_ID();
    return STATUS_SUCCESS;
}

static PVOID tidReuseCandidate(PVOID args)
{
    TidReuseContext* pCtx = (TidReuseContext*) args;
    PSignalingClient pSignalingClient = pCtx->pSignalingClient;

    if (SIGNALING_CURRENT_THREAD_ID() != pCtx->reconnectTid.load()) {
        return NULL;
    }

    pCtx->reused = TRUE;
    // The WSS callback spawns reconnect thread B; this app thread frees before B has necessarily recorded its id.
    if (STATUS_FAILED(startReconnectHandler(pSignalingClient))) {
        return NULL;
    }
    pCtx->freeStatus = freeSignaling(&pSignalingClient);
    pCtx->freed = (pSignalingClient == NULL);
    return NULL;
}

TEST_F(SignalingShutdownRaceTest, freeFromThreadWithRecycledReconnectTidIsAccepted)
{
    TidReuseContext ctx;
    ShutdownRaceClientOptions options;
    PAwsCredentialProvider pCredentialProvider = NULL;
    PSignalingClient pSignalingClient = NULL;
    TID candidate;
    UINT32 attempts;

    options.customData = (UINT64) &ctx;
    options.errorReportFn = recordTidErrorReportFn;
    ASSERT_EQ(STATUS_SUCCESS, createShutdownRaceClient(mChannelName, mRegion, mCaCertPath, mLogLevel, options, &pCredentialProvider, &pSignalingClient));
    ctx.pSignalingClient = pSignalingClient;

    // A real reconnect thread runs and exits (its reconnect fails fast offline, so errorReportFn records its id).
    ASSERT_EQ(STATUS_SUCCESS, startReconnectHandler(pSignalingClient));
    ASSERT_TRUE(waitFor([pSignalingClient] { return ATOMIC_LOAD_BOOL(&pSignalingClient->reconnecterTracker.terminated) == TRUE; }, 10000));
    THREAD_SLEEP(100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
    ASSERT_NE((SIZE_T) 0, ctx.reconnectTid.load());

    EXPECT_EQ((SIZE_T) 0, (SIZE_T) ATOMIC_LOAD(&pSignalingClient->reconnectThreadTid))
        << "reconnectThreadTid still holds the id of a reconnect thread that has exited";

    for (attempts = 0; attempts < 100 && !ctx.reused; attempts++) {
        ASSERT_EQ(STATUS_SUCCESS, THREAD_CREATE(&candidate, tidReuseCandidate, (PVOID) &ctx));
        ASSERT_EQ(STATUS_SUCCESS, THREAD_JOIN(candidate, NULL));
    }
    if (!ctx.reused) {
        EXPECT_EQ(STATUS_SUCCESS, freeSignaling(&pSignalingClient));
        freeStaticCredentialProvider(&pCredentialProvider);
        GTEST_SKIP() << "No new thread reused the exited reconnect thread's id in 100 attempts";
    }

    DLOGI("Thread id reused after %u thread(s)", attempts);
    EXPECT_NE(STATUS_INVALID_OPERATION, ctx.freeStatus)
        << "freeSignalingClient() from an ordinary app thread was rejected because the thread inherited the id of an exited reconnect thread";
    EXPECT_EQ(STATUS_SUCCESS, ctx.freeStatus);
    EXPECT_TRUE(ctx.freed);

    if (!ctx.freed) {
        EXPECT_EQ(STATUS_SUCCESS, freeSignaling(&pSignalingClient));
    }
    freeStaticCredentialProvider(&pCredentialProvider);
}

} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com
