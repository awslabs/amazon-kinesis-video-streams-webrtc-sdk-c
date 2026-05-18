#include "WebRTCClientTestFixture.h"

namespace com {
namespace amazonaws {
namespace kinesis {
namespace video {
namespace webrtcclient {

class DtlsFunctionalityTest : public WebRtcClientTestBase {
  public:
    STATUS createAndConnect(TIMER_QUEUE_HANDLE timerQueueHandle, PDtlsSession* ppClient, PDtlsSession* ppServer, BOOL useThread)
    {
        struct Context {
            std::mutex mtx;
            std::queue<std::vector<BYTE>> queue;
        };
        STATUS retStatus = STATUS_SUCCESS;
        DtlsSessionCallbacks callbacks;
        SIZE_T connectedCount = 0;
        PDtlsSession pClient = NULL, pServer = NULL;
        UINT64 sleepDelay = 20 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
        Context clientCtx, serverCtx;
        std::thread dtlsClientThread, dtlsServerThread;

        MEMSET(&callbacks, 0, SIZEOF(callbacks));
        callbacks.stateChangeFn = [](UINT64 customData, RTC_DTLS_TRANSPORT_STATE state) {
            if (state == RTC_DTLS_TRANSPORT_STATE_CONNECTED) {
                ATOMIC_INCREMENT((PSIZE_T) customData);
            }
        };
        callbacks.stateChangeFnCustomData = (UINT64) &connectedCount;

        DtlsSessionOutboundPacketFunc outboundPacketFn = [](UINT64 customData, PBYTE pData, UINT32 dataLen) {
            Context* pCtx = (Context*) customData;
            // since we're not in google test block, we can't use ASSERT_TRUE
            assert(pCtx != NULL);
            assert(pData != NULL);
            pCtx->mtx.lock();
            pCtx->queue.push(std::vector<BYTE>(pData, pData + dataLen));
            pCtx->mtx.unlock();
        };

        auto consumeMessages = [](Context* pCtx, PDtlsSession pPeer) -> STATUS {
            STATUS retStatus = STATUS_SUCCESS;
            // since we're not in google test block, we can't use ASSERT_TRUE
            assert(pCtx != NULL);
            assert(pPeer != NULL);

            pCtx->mtx.lock();
            std::queue<std::vector<BYTE>> pendingMessages;
            pCtx->queue.swap(pendingMessages);
            pCtx->mtx.unlock();

            while (!pendingMessages.empty()) {
                auto& msg = pendingMessages.front();
                auto readLen = (INT32) msg.size();
                CHK_STATUS(dtlsSessionProcessPacket(pPeer, (PBYTE) &msg.front(), &readLen));
                pendingMessages.pop();
            }

        CleanUp:

            return retStatus;
        };

        CHK_STATUS(createDtlsSession(&callbacks, timerQueueHandle, 0, FALSE, NULL, &pServer));
        CHK_STATUS(createDtlsSession(&callbacks, timerQueueHandle, 0, FALSE, NULL, &pClient));

        CHK_STATUS(dtlsSessionOnOutBoundData(pServer, (UINT64) &clientCtx, outboundPacketFn));
        CHK_STATUS(dtlsSessionOnOutBoundData(pClient, (UINT64) &serverCtx, outboundPacketFn));

        // In case of mbedtls it will be a black return of SUCCESS
#ifdef KVS_USE_OPENSSL
        if (useThread) {
            dtlsClientThread = std::thread(dtlsSessionHandshakeInThread, pClient, TRUE);
            dtlsServerThread = std::thread(dtlsSessionHandshakeInThread, pServer, FALSE);
        } else {
            CHK_STATUS(dtlsSessionStart(pClient, TRUE));
            CHK_STATUS(dtlsSessionStart(pServer, FALSE));
        }
#else
        CHK_STATUS(dtlsSessionStart(pClient, TRUE));
        CHK_STATUS(dtlsSessionStart(pServer, FALSE));
#endif

        for (UINT64 duration = 0; duration < MAX_TEST_AWAIT_DURATION && ATOMIC_LOAD(&connectedCount) != 2; duration += sleepDelay) {
            CHK_STATUS(consumeMessages(&serverCtx, pServer));
            CHK_STATUS(consumeMessages(&clientCtx, pClient));
            THREAD_SLEEP(sleepDelay);
        }

        CHK_ERR(ATOMIC_LOAD(&connectedCount) == 2, STATUS_OPERATION_TIMED_OUT, "timeout: failed to finish initial handshake");

        *ppClient = pClient;
        *ppServer = pServer;

#ifdef KVS_USE_OPENSSL
        if (useThread) {
            dtlsClientThread.join();
            dtlsServerThread.join();
        }
#endif

    CleanUp:

        if (STATUS_FAILED(retStatus)) {
            if (pClient != NULL) {
                freeDtlsSession(&pClient);
            }

            if (pServer != NULL) {
                freeDtlsSession(&pServer);
            }
        }

        return STATUS_SUCCESS;
    }
};

VOID outboundPacketFnNoop(UINT64 customData, PBYTE pData, UINT32 dataLen)
{
    UNUSED_PARAM(customData);
    UNUSED_PARAM(pData);
    UNUSED_PARAM(dataLen);
}

TEST_F(DtlsFunctionalityTest, putApplicationDataWithVariedSizes)
{
    PDtlsSession pClient = NULL, pServer = NULL;
    TIMER_QUEUE_HANDLE timerQueueHandle = INVALID_TIMER_QUEUE_HANDLE_VALUE;
    PBYTE pData = NULL;
    INT32 dataSizes[] = {
        4,                            // very small packet
        DEFAULT_MTU_SIZE_BYTES - 200, // small packet but should be still under mtu
        DEFAULT_MTU_SIZE_BYTES + 200, // big packet and bigger than even a jumbo frame
    };

    EXPECT_EQ(STATUS_SUCCESS, timerQueueCreate(&timerQueueHandle));
    EXPECT_EQ(STATUS_SUCCESS, createAndConnect(timerQueueHandle, &pClient, &pServer, FALSE));

    EXPECT_EQ(STATUS_SUCCESS, dtlsSessionOnOutBoundData(pClient, 0, outboundPacketFnNoop));
    EXPECT_EQ(STATUS_SUCCESS, dtlsSessionOnOutBoundData(pServer, 0, outboundPacketFnNoop));

    for (int i = 0; i < (INT32) ARRAY_SIZE(dataSizes); i++) {
        pData = (PBYTE) MEMREALLOC(pData, dataSizes[i]);
        ASSERT_TRUE(pData != NULL);
        MEMSET(pData, 0x11, dataSizes[i]);
        EXPECT_EQ(STATUS_SUCCESS, dtlsSessionPutApplicationData(pClient, pData, dataSizes[i]));
    }

    freeDtlsSession(&pClient);
    freeDtlsSession(&pServer);
    timerQueueFree(&timerQueueHandle);
    MEMFREE(pData);
}

TEST_F(DtlsFunctionalityTest, processPacketWithVariedSizes)
{
    PDtlsSession pClient = NULL, pServer = NULL;
    TIMER_QUEUE_HANDLE timerQueueHandle = INVALID_TIMER_QUEUE_HANDLE_VALUE;
    PBYTE pData = NULL;
    INT32 dataSizes[] = {
        4,                            // very small packet
        DEFAULT_MTU_SIZE_BYTES - 200, // small packet but should be still under mtu
        DEFAULT_MTU_SIZE_BYTES + 200, // big packet and bigger than even a jumbo frame
    };
    INT32 readDataSize;

    EXPECT_EQ(STATUS_SUCCESS, timerQueueCreate(&timerQueueHandle));
    EXPECT_EQ(STATUS_SUCCESS, createAndConnect(timerQueueHandle, &pClient, &pServer, FALSE));

    EXPECT_EQ(STATUS_SUCCESS, dtlsSessionOnOutBoundData(pServer, 0, outboundPacketFnNoop));
    EXPECT_EQ(STATUS_SUCCESS, dtlsSessionOnOutBoundData(pClient, 0, outboundPacketFnNoop));

    for (int i = 0; i < (INT32) ARRAY_SIZE(dataSizes); i++) {
        pData = (PBYTE) MEMREALLOC(pData, dataSizes[i]);
        readDataSize = dataSizes[i];
        ASSERT_TRUE(pData != NULL);
        MEMSET(pData, 0x11, dataSizes[i]);
        EXPECT_EQ(STATUS_SUCCESS, dtlsSessionProcessPacket(pServer, pData, &readDataSize));
    }

    freeDtlsSession(&pClient);
    freeDtlsSession(&pServer);
    timerQueueFree(&timerQueueHandle);
    MEMFREE(pData);
}

TEST_F(DtlsFunctionalityTest, putApplicationDataWithVariedSizesInThread)
{
    PDtlsSession pClient = NULL, pServer = NULL;
    TIMER_QUEUE_HANDLE timerQueueHandle = INVALID_TIMER_QUEUE_HANDLE_VALUE;
    PBYTE pData = NULL;
    INT32 dataSizes[] = {
        4,                            // very small packet
        DEFAULT_MTU_SIZE_BYTES - 200, // small packet but should be still under mtu
        DEFAULT_MTU_SIZE_BYTES + 200, // big packet and bigger than even a jumbo frame
    };

    EXPECT_EQ(STATUS_SUCCESS, timerQueueCreate(&timerQueueHandle));
    EXPECT_EQ(STATUS_SUCCESS, createAndConnect(timerQueueHandle, &pClient, &pServer, TRUE));

    EXPECT_EQ(STATUS_SUCCESS, dtlsSessionOnOutBoundData(pClient, 0, outboundPacketFnNoop));
    EXPECT_EQ(STATUS_SUCCESS, dtlsSessionOnOutBoundData(pServer, 0, outboundPacketFnNoop));

    for (int i = 0; i < (INT32) ARRAY_SIZE(dataSizes); i++) {
        pData = (PBYTE) MEMREALLOC(pData, dataSizes[i]);
        ASSERT_TRUE(pData != NULL);
        MEMSET(pData, 0x11, dataSizes[i]);
        EXPECT_EQ(STATUS_SUCCESS, dtlsSessionPutApplicationData(pClient, pData, dataSizes[i]));
    }

    freeDtlsSession(&pClient);
    freeDtlsSession(&pServer);
    timerQueueFree(&timerQueueHandle);
    MEMFREE(pData);
}

TEST_F(DtlsFunctionalityTest, processPacketWithVariedSizesInThread)
{
    PDtlsSession pClient = NULL, pServer = NULL;
    TIMER_QUEUE_HANDLE timerQueueHandle = INVALID_TIMER_QUEUE_HANDLE_VALUE;
    PBYTE pData = NULL;
    INT32 dataSizes[] = {
        4,                            // very small packet
        DEFAULT_MTU_SIZE_BYTES - 200, // small packet but should be still under mtu
        DEFAULT_MTU_SIZE_BYTES + 200, // big packet and bigger than even a jumbo frame
    };
    INT32 readDataSize;

    EXPECT_EQ(STATUS_SUCCESS, timerQueueCreate(&timerQueueHandle));
    EXPECT_EQ(STATUS_SUCCESS, createAndConnect(timerQueueHandle, &pClient, &pServer, TRUE));
    EXPECT_EQ(STATUS_SUCCESS, dtlsSessionOnOutBoundData(pServer, 0, outboundPacketFnNoop));
    EXPECT_EQ(STATUS_SUCCESS, dtlsSessionOnOutBoundData(pClient, 0, outboundPacketFnNoop));

    for (int i = 0; i < (INT32) ARRAY_SIZE(dataSizes); i++) {
        pData = (PBYTE) MEMREALLOC(pData, dataSizes[i]);
        readDataSize = dataSizes[i];
        ASSERT_TRUE(pData != NULL);
        MEMSET(pData, 0x11, dataSizes[i]);
        EXPECT_EQ(STATUS_SUCCESS, dtlsSessionProcessPacket(pServer, pData, &readDataSize));
    }

    freeDtlsSession(&pClient);
    freeDtlsSession(&pServer);
    timerQueueFree(&timerQueueHandle);
    MEMFREE(pData);
}

TEST_F(DtlsFunctionalityTest, strictServerValidationRejectsUntrustedServerCertificate)
{
    struct PacketContext {
        std::mutex mtx;
        std::queue<std::vector<BYTE>> queue;
    };
    struct SessionState {
        std::atomic<int> state{RTC_DTLS_TRANSPORT_STATE_NEW};
        std::atomic<SIZE_T> connectedCount{0};
        std::atomic<SIZE_T> failedCount{0};
    };

    STATUS retStatus = STATUS_SUCCESS;
    DtlsSessionCallbacks serverCallbacks, clientCallbacks;
    TIMER_QUEUE_HANDLE timerQueueHandle = INVALID_TIMER_QUEUE_HANDLE_VALUE;
    PDtlsSession pServer = NULL, pStrictClient = NULL;
    UINT64 sleepDelay = 20 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
    UINT64 timeout = 5 * HUNDREDS_OF_NANOS_IN_A_SECOND;
    PacketContext clientInboundCtx, serverInboundCtx;
    SessionState serverState, clientState;
    CHAR expectedServerHostname[] = "stun.kinesisvideo-fips.us-gov-west-1.amazonaws.com";
    DtlsSessionOptions strictOptions;

    MEMSET(&serverCallbacks, 0x00, SIZEOF(serverCallbacks));
    MEMSET(&clientCallbacks, 0x00, SIZEOF(clientCallbacks));
    MEMSET(&strictOptions, 0x00, SIZEOF(strictOptions));

    serverCallbacks.stateChangeFn = [](UINT64 customData, RTC_DTLS_TRANSPORT_STATE state) {
        SessionState* pState = reinterpret_cast<SessionState*>(customData);
        pState->state.store((int) state);
        if (state == RTC_DTLS_TRANSPORT_STATE_CONNECTED) {
            pState->connectedCount.fetch_add(1);
        } else if (state == RTC_DTLS_TRANSPORT_STATE_FAILED) {
            pState->failedCount.fetch_add(1);
        }
    };
    serverCallbacks.stateChangeFnCustomData = (UINT64) &serverState;
    clientCallbacks.stateChangeFn = serverCallbacks.stateChangeFn;
    clientCallbacks.stateChangeFnCustomData = (UINT64) &clientState;

    DtlsSessionOutboundPacketFunc outboundPacketFn = [](UINT64 customData, PBYTE pData, UINT32 dataLen) {
        PacketContext* pCtx = reinterpret_cast<PacketContext*>(customData);
        assert(pCtx != NULL);
        assert(pData != NULL);
        std::lock_guard<std::mutex> lock(pCtx->mtx);
        pCtx->queue.push(std::vector<BYTE>(pData, pData + dataLen));
    };

    auto consumeMessages = [](PacketContext* pCtx, PDtlsSession pPeer) -> STATUS {
        STATUS retStatus = STATUS_SUCCESS;
        std::queue<std::vector<BYTE>> pendingMessages;

        assert(pCtx != NULL);
        assert(pPeer != NULL);

        {
            std::lock_guard<std::mutex> lock(pCtx->mtx);
            pCtx->queue.swap(pendingMessages);
        }

        while (!pendingMessages.empty()) {
            auto& msg = pendingMessages.front();
            INT32 readLen = (INT32) msg.size();
            CHK_STATUS(dtlsSessionProcessPacket(pPeer, (PBYTE) msg.data(), &readLen));
            pendingMessages.pop();
        }

    CleanUp:
        return retStatus;
    };

    strictOptions.validationMode = DTLS_SESSION_VALIDATION_MODE_STRICT_SERVER;
    strictOptions.pExpectedServerHostname = expectedServerHostname;

    ASSERT_EQ(STATUS_SUCCESS, timerQueueCreate(&timerQueueHandle));
    ASSERT_EQ(STATUS_SUCCESS, createDtlsSession(&serverCallbacks, timerQueueHandle, 0, FALSE, NULL, &pServer));
    ASSERT_EQ(STATUS_SUCCESS, createDtlsSessionWithOptions(&clientCallbacks, timerQueueHandle, 0, FALSE, NULL, &strictOptions, &pStrictClient));

    ASSERT_EQ(STATUS_SUCCESS, dtlsSessionOnOutBoundData(pServer, (UINT64) &clientInboundCtx, outboundPacketFn));
    ASSERT_EQ(STATUS_SUCCESS, dtlsSessionOnOutBoundData(pStrictClient, (UINT64) &serverInboundCtx, outboundPacketFn));

    ASSERT_EQ(STATUS_SUCCESS, dtlsSessionStart(pServer, TRUE));
    ASSERT_EQ(STATUS_SUCCESS, dtlsSessionStart(pStrictClient, FALSE));

    for (UINT64 duration = 0; duration < timeout && clientState.failedCount.load() == 0 && clientState.connectedCount.load() == 0;
         duration += sleepDelay) {
        STATUS clientConsumeStatus = STATUS_SUCCESS;
        STATUS serverConsumeStatus = STATUS_SUCCESS;

        serverConsumeStatus = consumeMessages(&serverInboundCtx, pServer);
        EXPECT_TRUE(serverConsumeStatus == STATUS_SUCCESS || serverConsumeStatus == STATUS_INTERNAL_ERROR);
        clientConsumeStatus = consumeMessages(&clientInboundCtx, pStrictClient);
        EXPECT_TRUE(clientConsumeStatus == STATUS_SUCCESS || clientConsumeStatus == STATUS_SSL_REMOTE_CERTIFICATE_VERIFICATION_FAILED);
        if (clientConsumeStatus == STATUS_SSL_REMOTE_CERTIFICATE_VERIFICATION_FAILED) {
            break;
        }
        THREAD_SLEEP(sleepDelay);
    }

    EXPECT_EQ(clientState.failedCount.load(), 1);
    EXPECT_EQ(clientState.connectedCount.load(), 0);
    EXPECT_EQ(clientState.state.load(), RTC_DTLS_TRANSPORT_STATE_FAILED);

    if (pStrictClient != NULL) {
        freeDtlsSession(&pStrictClient);
    }
    if (pServer != NULL) {
        freeDtlsSession(&pServer);
    }
    if (IS_VALID_TIMER_QUEUE_HANDLE(timerQueueHandle)) {
        timerQueueFree(&timerQueueHandle);
    }
}

} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com
