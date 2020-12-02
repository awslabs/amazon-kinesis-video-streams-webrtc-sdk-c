#include "WebRTCClientBenchmarkFixture.h"

namespace com {
namespace amazonaws {
namespace kinesis {
namespace video {
namespace webrtcclient {

class DtlsBenchmark : public WebRtcClientBenchmarkBase {
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
            assert(pCtx != NULL);
            assert(pData != NULL);
            pCtx->mtx.lock();
            pCtx->queue.push(std::vector<BYTE>(pData, pData + dataLen));
            pCtx->mtx.unlock();
        };

        auto consumeMessages = [](Context* pCtx, PDtlsSession pPeer) -> STATUS {
            STATUS retStatus = STATUS_SUCCESS;
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

        for (UINT64 duration = 0; duration < MAX_BENCHMARK_AWAIT_DURATION && ATOMIC_LOAD(&connectedCount) != 2; duration += sleepDelay) {
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

BENCHMARK_DEFINE_F(DtlsBenchmark, BM_DtlsEncrypt)(benchmark::State& state)
{
    STATUS retStatus = STATUS_SUCCESS;
    PDtlsSession pClient = NULL, pServer = NULL;
    TIMER_QUEUE_HANDLE timerQueueHandle = INVALID_TIMER_QUEUE_HANDLE_VALUE;
    INT32 dataSize = state.range(0);
    PBYTE data = (PBYTE) MEMALLOC(dataSize);

    CHK(data != NULL, STATUS_NOT_ENOUGH_MEMORY);
    MEMSET(data, 0x11, dataSize);
    CHK_STATUS(timerQueueCreate(&timerQueueHandle));
    CHK_STATUS(createAndConnect(timerQueueHandle, &pClient, &pServer));

    CHK_STATUS(dtlsSessionOnOutBoundData(pClient, 0, outboundPacketFnNoop));
    CHK_STATUS(dtlsSessionOnOutBoundData(pServer, 0, outboundPacketFnNoop));

    for (auto _ : state) {
        CHK_STATUS(dtlsSessionPutApplicationData(pClient, data, dataSize));
    }
    state.SetBytesProcessed((INT64) state.iterations() * dataSize);

CleanUp:

    if (STATUS_FAILED(retStatus)) {
        DLOGE("Dtls benchmark failed with 0x%08x", retStatus);
    }

    freeDtlsSession(&pClient);
    freeDtlsSession(&pServer);
    timerQueueFree(&timerQueueHandle);
    MEMFREE(data);
}

BENCHMARK_DEFINE_F(DtlsBenchmark, BM_DtlsDecrypt)(benchmark::State& state)
{
    STATUS retStatus = STATUS_SUCCESS;
    PDtlsSession pClient = NULL, pServer = NULL;
    TIMER_QUEUE_HANDLE timerQueueHandle = INVALID_TIMER_QUEUE_HANDLE_VALUE;
    INT32 dataSize = state.range(0);
    PBYTE data = (PBYTE) MEMALLOC(dataSize);
    INT32 readDataSize;

    CHK(data != NULL, STATUS_NOT_ENOUGH_MEMORY);
    MEMSET(data, 0x11, dataSize);
    CHK_STATUS(timerQueueCreate(&timerQueueHandle));
    CHK_STATUS(createAndConnect(timerQueueHandle, &pClient, &pServer));

    CHK_STATUS(dtlsSessionOnOutBoundData(pServer, 0, outboundPacketFnNoop));
    CHK_STATUS(dtlsSessionOnOutBoundData(pClient, 0, outboundPacketFnNoop));

    for (auto _ : state) {
        readDataSize = dataSize;
        CHK_STATUS(dtlsSessionProcessPacket(pServer, data, &readDataSize));
    }
    state.SetBytesProcessed((INT64) state.iterations() * dataSize);

CleanUp:

    if (STATUS_FAILED(retStatus)) {
        DLOGE("Dtls benchmark failed with 0x%08x", retStatus);
    }

    freeDtlsSession(&pClient);
    freeDtlsSession(&pServer);
    timerQueueFree(&timerQueueHandle);
    MEMFREE(data);
}

BENCHMARK_REGISTER_F(DtlsBenchmark, BM_DtlsEncrypt)->Range(8, 8 << 10);
BENCHMARK_REGISTER_F(DtlsBenchmark, BM_DtlsDecrypt)->Range(8, 8 << 10);

} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com
