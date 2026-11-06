#include "WebRTCClientTestFixture.h"
#include <atomic>
#include <mutex>
#include <vector>

namespace com {
namespace amazonaws {
namespace kinesis {
namespace video {
namespace webrtcclient {

// reconnectHandler() drives the signaling state machine to CONNECTED with a 15 s deadline
// (SIGNALING_CONNECT_STATE_TIMEOUT). signalingStateMachineIterator() checks the deadline at the top of
// every loop iteration, before stepping. When the WebSocket handshake itself crosses the deadline, the lws
// callback has already set connected = TRUE and result = OK, but the iterator exits with
// STATUS_OPERATION_TIMED_OUT before the CONNECT -> CONNECTED step runs. reconnectHandler() then reports
// STATUS_SIGNALING_RECONNECT_FAILED to the application although the socket is up, and leaves the state
// machine in CONNECT with a live socket.
//
// These tests run the real reconnectHandler() on an offline client whose shared state is set to the
// moment right after the handshake (state CONNECT, connected TRUE, result OK). The clock is the test's
// getCurrentTimeFn: it answers reconnectHandler()'s deadline computation with the real time and every
// later call with real time + 16 s, which models a handshake that finished after the deadline. No
// network is involved; the connect step itself is not executed, its outcome is preset.
class SignalingReconnectDeadlineTest : public WebRtcClientTestBase {};

#define RECONNECT_DEADLINE_TEST_JUMP      (16 * HUNDREDS_OF_NANOS_IN_A_SECOND)
#define RECONNECT_DEADLINE_TEST_EXIT_WAIT (10 * HUNDREDS_OF_NANOS_IN_A_SECOND)
#define RECONNECT_DEADLINE_TEST_POLL      (50 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND)

struct ReconnectDeadlineContext {
    // Clock control. callsBeforeJump < 0: answer real time. callsBeforeJump == n > 0: the next n calls answer
    // real time, then every call answers real time + jump. reconnectHandler() reads the clock once to compute
    // the deadline, so arming with 1 makes the first loop-top check in the iterator see a time past it.
    std::atomic<INT32> callsBeforeJump{-1};
    UINT64 jump = 0;

    std::atomic<UINT32> connectingCallbacks{0};
    std::atomic<UINT32> connectedCallbacks{0};
    std::atomic<UINT32> errorCallbacks{0};
    std::atomic<STATUS> lastErrorStatus{STATUS_SUCCESS};
    std::mutex lock;
    std::vector<SIGNALING_CLIENT_STATE> states;
};

static UINT64 reconnectDeadlineClockFn(UINT64 customData)
{
    ReconnectDeadlineContext* pCtx = (ReconnectDeadlineContext*) customData;
    UINT64 now = GETTIME();
    INT32 remaining = pCtx->callsBeforeJump.load();

    if (remaining < 0) {
        return now;
    }
    if (remaining == 0) {
        return now + pCtx->jump;
    }
    pCtx->callsBeforeJump.store(remaining - 1);
    return now;
}

static STATUS reconnectDeadlineStateChangeFn(UINT64 customData, SIGNALING_CLIENT_STATE state)
{
    ReconnectDeadlineContext* pCtx = (ReconnectDeadlineContext*) customData;

    if (state == SIGNALING_CLIENT_STATE_CONNECTING) {
        pCtx->connectingCallbacks++;
    } else if (state == SIGNALING_CLIENT_STATE_CONNECTED) {
        pCtx->connectedCallbacks++;
    }

    std::lock_guard<std::mutex> guard(pCtx->lock);
    if (pCtx->states.size() < 64) {
        pCtx->states.push_back(state);
    }
    return STATUS_SUCCESS;
}

static STATUS reconnectDeadlineErrorReportFn(UINT64 customData, STATUS status, PCHAR msg, UINT32 msgLen)
{
    UNUSED_PARAM(msg);
    UNUSED_PARAM(msgLen);
    ReconnectDeadlineContext* pCtx = (ReconnectDeadlineContext*) customData;
    pCtx->errorCallbacks++;
    pCtx->lastErrorStatus.store(status);
    return STATUS_SUCCESS;
}

static STATUS reconnectDeadlineMessageReceivedFn(UINT64 customData, PReceivedSignalingMessage pReceivedSignalingMessage)
{
    UNUSED_PARAM(customData);
    UNUSED_PARAM(pReceivedSignalingMessage);
    return STATUS_SUCCESS;
}

// Builds a signaling client without touching the network. createSignalingSync() only drives the state
// machine to GET_TOKEN, which the static credential provider serves locally.
static STATUS createOfflineSignalingClientWithClock(PCHAR channelName, PCHAR region, PCHAR certPath, UINT32 logLevel, ReconnectDeadlineContext* pCtx,
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
    signalingClientCallbacks.customData = (UINT64) pCtx;
    signalingClientCallbacks.messageReceivedFn = reconnectDeadlineMessageReceivedFn;
    signalingClientCallbacks.stateChangeFn = reconnectDeadlineStateChangeFn;
    signalingClientCallbacks.errorReportFn = reconnectDeadlineErrorReportFn;
    signalingClientCallbacks.getCurrentTimeFn = reconnectDeadlineClockFn;

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

// Puts the client in the state the WSS callback leaves behind right after a successful handshake while the
// state machine is still in CONNECT: the lws callback has set connected and result, the iterator has not
// stepped yet.
static VOID setJustConnectedInConnectState(PSignalingClient pSignalingClient)
{
    setStateMachineCurrentState(pSignalingClient->pStateMachine, SIGNALING_STATE_CONNECT);
    ATOMIC_STORE_BOOL(&pSignalingClient->connected, TRUE);
    ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) SERVICE_CALL_RESULT_OK);
}

static BOOL waitForReconnectThreadExit(PSignalingClient pSignalingClient)
{
    UINT64 waited = 0;
    while (!ATOMIC_LOAD_BOOL(&pSignalingClient->reconnecterTracker.terminated) && waited < RECONNECT_DEADLINE_TEST_EXIT_WAIT) {
        THREAD_SLEEP(RECONNECT_DEADLINE_TEST_POLL);
        waited += RECONNECT_DEADLINE_TEST_POLL;
    }
    return ATOMIC_LOAD_BOOL(&pSignalingClient->reconnecterTracker.terminated);
}

// Positive control for the harness: the same preset state with a clock that does not jump. The first loop
// iteration is inside the deadline, the CONNECT -> CONNECTED step runs, and the reconnect is reported as
// success. This passes before and after the fix; it shows the only variable in the test below is the clock.
TEST_F(SignalingReconnectDeadlineTest, reconnectThatConnectsBeforeDeadlineIsReportedAsConnected)
{
    ReconnectDeadlineContext ctx;
    PSignalingClient pSignalingClient = NULL;
    PAwsCredentialProvider pCredentialProvider = NULL;
    PStateMachineState pState = NULL;

    ASSERT_EQ(STATUS_SUCCESS,
              createOfflineSignalingClientWithClock(mChannelName, mRegion, mCaCertPath, mLogLevel, &ctx, &pCredentialProvider, &pSignalingClient));

    setJustConnectedInConnectState(pSignalingClient);
    ctx.connectingCallbacks = 0;
    ctx.connectedCallbacks = 0;
    ctx.errorCallbacks = 0;

    ASSERT_EQ(STATUS_SUCCESS, startReconnectHandler(pSignalingClient));
    ASSERT_TRUE(waitForReconnectThreadExit(pSignalingClient)) << "reconnectHandler() did not exit within 10 s";

    ASSERT_EQ(STATUS_SUCCESS, getStateMachineCurrentState(pSignalingClient->pStateMachine, &pState));
    EXPECT_EQ(0u, ctx.errorCallbacks.load()) << "errorReportFn was called, status 0x" << std::hex << ctx.lastErrorStatus.load();
    EXPECT_EQ(1u, ctx.connectedCallbacks.load());
    EXPECT_EQ(0u, ctx.connectingCallbacks.load());
    EXPECT_EQ(SIGNALING_STATE_CONNECTED, pState->state);

    EXPECT_EQ(STATUS_SUCCESS, freeSignaling(&pSignalingClient));
    EXPECT_EQ(STATUS_SUCCESS, freeStaticCredentialProvider(&pCredentialProvider));
}

// The reported scenario: the handshake finished after the 15 s deadline. The socket is up and
// the shared state says so, but the state machine is still in CONNECT. A reconnect that has in fact
// connected must be reported as connected: stateChangeFn(CONNECTED) once, no errorReportFn, state CONNECTED.
//
// Without the fix: reconnectHandler() reports STATUS_SIGNALING_RECONNECT_FAILED ("Failed to reconnect with
// status code 0x0000000f"), emits no CONNECTED callback and leaves the state machine in CONNECT with
// connected == TRUE.
TEST_F(SignalingReconnectDeadlineTest, reconnectThatConnectsAfterDeadlineIsReportedAsConnected)
{
    ReconnectDeadlineContext ctx;
    PSignalingClient pSignalingClient = NULL;
    PAwsCredentialProvider pCredentialProvider = NULL;
    PStateMachineState pState = NULL;

    ASSERT_EQ(STATUS_SUCCESS,
              createOfflineSignalingClientWithClock(mChannelName, mRegion, mCaCertPath, mLogLevel, &ctx, &pCredentialProvider, &pSignalingClient));

    setJustConnectedInConnectState(pSignalingClient);
    ctx.connectingCallbacks = 0;
    ctx.connectedCallbacks = 0;
    ctx.errorCallbacks = 0;

    // Arm the clock: reconnectHandler()'s deadline computation sees the real time, the iterator's first
    // loop-top check sees real time + 16 s. This is the moment at which the reconnect thread returned
    // from the connect step: deadline passed, socket connected, state machine not yet stepped.
    ctx.jump = RECONNECT_DEADLINE_TEST_JUMP;
    ctx.callsBeforeJump.store(1);

    ASSERT_EQ(STATUS_SUCCESS, startReconnectHandler(pSignalingClient));
    ASSERT_TRUE(waitForReconnectThreadExit(pSignalingClient)) << "reconnectHandler() did not exit within 10 s";
    ctx.callsBeforeJump.store(-1);

    ASSERT_EQ(STATUS_SUCCESS, getStateMachineCurrentState(pSignalingClient->pStateMachine, &pState));
    EXPECT_EQ(0u, ctx.errorCallbacks.load()) << "reconnectHandler() reported failure (errorReportFn status 0x" << std::hex
                                             << ctx.lastErrorStatus.load() << ") although the WebSocket was connected when it hit the deadline";
    EXPECT_EQ(1u, ctx.connectedCallbacks.load()) << "stateChangeFn(CONNECTED) was not delivered for a reconnect that connected";
    EXPECT_EQ(0u, ctx.connectingCallbacks.load());
    EXPECT_EQ(SIGNALING_STATE_CONNECTED, pState->state)
        << "state machine left in 0x" << std::hex << pState->state << " with connected == TRUE; a later disconnect starts from CONNECT";

    EXPECT_EQ(STATUS_SUCCESS, freeSignaling(&pSignalingClient));
    EXPECT_EQ(STATUS_SUCCESS, freeStaticCredentialProvider(&pCredentialProvider));
}

} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com
