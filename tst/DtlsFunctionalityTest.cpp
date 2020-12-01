#include "WebRTCClientTestFixture.h"

namespace com {
namespace amazonaws {
namespace kinesis {
namespace video {
namespace webrtcclient {

class DtlsFunctionalityTest : public WebRtcClientTestBase {
  public:
    STATUS createAndConnect(TIMER_QUEUE_HANDLE timerQueueHandle, PDtlsSession* ppClient, PDtlsSession* ppServer)
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

        CHK_STATUS(dtlsSessionStart(pServer, FALSE));
        CHK_STATUS(dtlsSessionStart(pClient, TRUE));

        for (UINT64 duration = 0; duration < MAX_TEST_AWAIT_DURATION && ATOMIC_LOAD(&connectedCount) != 2; duration += sleepDelay) {
            CHK_STATUS(consumeMessages(&serverCtx, pServer));
            CHK_STATUS(consumeMessages(&clientCtx, pClient));
            THREAD_SLEEP(sleepDelay);
        }

        CHK_ERR(ATOMIC_LOAD(&connectedCount) == 2, STATUS_OPERATION_TIMED_OUT, "timeout: failed to finish initial handshake");

        *ppClient = pClient;
        *ppServer = pServer;

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
        4,                      // very small packet
        DEFAULT_MTU_SIZE - 200, // small packet but should be still under mtu
        DEFAULT_MTU_SIZE + 200, // big packet and bigger than even a jumbo frame
    };

    EXPECT_EQ(STATUS_SUCCESS, timerQueueCreate(&timerQueueHandle));
    EXPECT_EQ(STATUS_SUCCESS, createAndConnect(timerQueueHandle, &pClient, &pServer));

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
        4,                      // very small packet
        DEFAULT_MTU_SIZE - 200, // small packet but should be still under mtu
        DEFAULT_MTU_SIZE + 200, // big packet and bigger than even a jumbo frame
    };
    INT32 readDataSize;

    EXPECT_EQ(STATUS_SUCCESS, timerQueueCreate(&timerQueueHandle));
    EXPECT_EQ(STATUS_SUCCESS, createAndConnect(timerQueueHandle, &pClient, &pServer));

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

} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com
