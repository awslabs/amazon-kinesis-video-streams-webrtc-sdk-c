#include "WebRTCClientTestFixture.h"

namespace com {
namespace amazonaws {
namespace kinesis {
namespace video {
namespace webrtcclient {

class DtlsFunctionalityTest : public WebRtcClientTestBase {
  public:
    STATUS createAndConnect(TIMER_QUEUE_HANDLE timerQueueHandle, PDtlsSession* ppClient, PDtlsSession* ppServer, BOOL useThread,
                            BOOL serverStartsLate = FALSE)
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
        BOOL startedHere = TRUE;

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

        if (serverStartsLate) {
            // The DTLS client's ClientHello reaches the server before the server starts (RFC 8842 section 5.2): the server
            // keeps it and answers it as it starts. pServer is the DTLS client here, and its packets go to clientCtx.
            startedHere = FALSE;
            CHK_STATUS(dtlsSessionStart(pServer, FALSE));
            CHK_STATUS(consumeMessages(&clientCtx, pClient));
            CHK_STATUS(dtlsSessionStart(pClient, TRUE));
            serverCtx.mtx.lock();
            EXPECT_FALSE(serverCtx.queue.empty());
            serverCtx.mtx.unlock();
        }
        // In case of mbedtls it will be a black return of SUCCESS
#ifdef KVS_USE_OPENSSL
        else if (useThread) {
            startedHere = FALSE;
            dtlsClientThread = std::thread(dtlsSessionHandshakeInThread, pClient, TRUE);
            dtlsServerThread = std::thread(dtlsSessionHandshakeInThread, pServer, FALSE);
        } else {
            CHK_STATUS(dtlsSessionStart(pClient, TRUE));
            CHK_STATUS(dtlsSessionStart(pServer, FALSE));
        }
#else
        else {
            CHK_STATUS(dtlsSessionStart(pClient, TRUE));
            CHK_STATUS(dtlsSessionStart(pServer, FALSE));
        }
#endif

        // dtlsSessionStart() sends the DTLS client's ClientHello rather than the first timer tick; the server waits for it.
        // pServer is the DTLS client here, and its packets go to clientCtx.
        if (startedHere) {
            std::lock_guard<std::mutex> clientLock(clientCtx.mtx), serverLock(serverCtx.mtx);
            EXPECT_FALSE(clientCtx.queue.empty());
            EXPECT_TRUE(serverCtx.queue.empty());
        }

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

// A ClientHello that reaches the server before its dtlsSessionStart() is answered as it starts, not left for the client to
// retransmit (RFC 8842 section 5.2)
TEST_F(DtlsFunctionalityTest, earlyClientHelloAnsweredAtStart)
{
    TIMER_QUEUE_HANDLE timerQueueHandle = INVALID_TIMER_QUEUE_HANDLE_VALUE;
    PDtlsSession pClient = NULL, pServer = NULL;

    EXPECT_EQ(STATUS_SUCCESS, timerQueueCreate(&timerQueueHandle));
    EXPECT_EQ(STATUS_SUCCESS, createAndConnect(timerQueueHandle, &pClient, &pServer, FALSE, TRUE));
    // Both stay NULL if the sessions did not connect
    EXPECT_NE(nullptr, pClient);
    EXPECT_NE(nullptr, pServer);

    freeDtlsSession(&pClient);
    freeDtlsSession(&pServer);
    timerQueueFree(&timerQueueHandle);
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

// One side of a DTLS handshake carried in STUN: a DTLS session feeding a DTLS-in-STUN controller
struct SpedSide {
    PDtlsSession pSession = NULL;
    PDtlsInStun pDtlsInStun = NULL;
    BOOL isServer = FALSE;
    std::atomic<UINT32> plainPackets{0};
    std::atomic<UINT32> flights{0};
    std::atomic<UINT32> retransmittedFlights{0};
    std::atomic<BOOL> connected{FALSE};
    UINT32 largestPacket = 0;
};

static VOID spedOutboundPacket(UINT64 customData, PBYTE pData, UINT32 dataLen)
{
    SpedSide* pSide = (SpedSide*) customData;
    pSide->largestPacket = MAX(pSide->largestPacket, dataLen);
    if (!dtlsInStunCapturePacket(pSide->pDtlsInStun, pData, dataLen)) {
        pSide->plainPackets++;
    }
}

static VOID spedHandshakeFlight(UINT64 customData, BOOL retransmission)
{
    SpedSide* pSide = (SpedSide*) customData;
    pSide->flights++;
    if (retransmission) {
        pSide->retransmittedFlights++;
    }
    dtlsInStunEndFlight(pSide->pDtlsInStun, retransmission);
}

static VOID spedStateChange(UINT64 customData, RTC_DTLS_TRANSPORT_STATE state)
{
    SpedSide* pSide = (SpedSide*) customData;
    if (state == RTC_DTLS_TRANSPORT_STATE_CONNECTED && !pSide->connected) {
        pSide->connected = TRUE;
        dtlsInStunOnHandshakeComplete(pSide->pDtlsInStun, !pSide->isServer);
    }
}

static STATUS spedSetup(SpedSide* pSide, TIMER_QUEUE_HANDLE timerQueueHandle, BOOL isServer)
{
    STATUS retStatus = STATUS_SUCCESS;
    DtlsSessionCallbacks callbacks;

    MEMSET(&callbacks, 0, SIZEOF(callbacks));
    callbacks.stateChangeFn = spedStateChange;
    callbacks.stateChangeFnCustomData = (UINT64) pSide;
    pSide->isServer = isServer;
    CHK_STATUS(createDtlsInStun(&pSide->pDtlsInStun));
    CHK_STATUS(createDtlsSession(&callbacks, timerQueueHandle, 0, FALSE, NULL, &pSide->pSession));
    CHK_STATUS(dtlsSessionOnOutBoundData(pSide->pSession, (UINT64) pSide, spedOutboundPacket));
    CHK_STATUS(dtlsSessionOnHandshakeFlight(pSide->pSession, (UINT64) pSide, spedHandshakeFlight));
    CHK_STATUS(dtlsSessionEnableSped(pSide->pSession));
    CHK_STATUS(dtlsSessionSetRetransmitHold(pSide->pSession, TRUE));
    CHK_STATUS(dtlsInStunStart(pSide->pDtlsInStun));
    CHK_STATUS(dtlsInStunSetDtlsStarted(pSide->pDtlsInStun));
    CHK_STATUS(dtlsSessionStart(pSide->pSession, isServer));

CleanUp:
    return retStatus;
}

static VOID spedTeardown(SpedSide* pSide)
{
    freeDtlsSession(&pSide->pSession);
    freeDtlsInStun(&pSide->pDtlsInStun);
}

// One STUN message from one side to the other, carrying whatever the controllers put in it
static STATUS spedDeliver(SpedSide* pFrom, SpedSide* pTo, STUN_PACKET_TYPE type, PBYTE transactionId)
{
    STATUS retStatus = STATUS_SUCCESS;
    PStunPacket pStunPacket = NULL;
    BYTE packet[DTLS_IN_STUN_MAX_PACKET_SIZE];
    UINT32 packetLen = 0;
    INT32 readLen;

    CHK_STATUS(createStunPacket(type, transactionId, &pStunPacket));
    CHK_STATUS(dtlsInStunAppendAttributes(pFrom->pDtlsInStun, pStunPacket, DEFAULT_MTU_SIZE_BYTES, TRUE));
    CHK_STATUS(dtlsInStunOnStunMessage(pTo->pDtlsInStun, pStunPacket, packet, &packetLen));
    if (packetLen > 0) {
        readLen = (INT32) packetLen;
        CHK_STATUS(dtlsSessionProcessPacket(pTo->pSession, packet, &readLen));
    }

CleanUp:
    freeStunPacket(&pStunPacket);
    return retStatus;
}

TEST_F(DtlsFunctionalityTest, spedHandshakeEntirelyInStun)
{
    TIMER_QUEUE_HANDLE timerQueueHandle = INVALID_TIMER_QUEUE_HANDLE_VALUE;
    SpedSide client, server;
    BYTE transactionId[STUN_TRANSACTION_ID_LEN] = {0};
    UINT32 round, dataPackets;
    BYTE data[1100] = {0};

    EXPECT_EQ(STATUS_SUCCESS, timerQueueCreate(&timerQueueHandle));
    ASSERT_EQ(STATUS_SUCCESS, spedSetup(&server, timerQueueHandle, TRUE));
    ASSERT_EQ(STATUS_SUCCESS, spedSetup(&client, timerQueueHandle, FALSE));

    // The ClientHello is out as soon as the session starts
    EXPECT_EQ(1, client.flights.load());

    // Checks from the client, each answered by the server
    for (round = 0; round < 10 && !(client.connected && server.connected); round++) {
        transactionId[0] = (BYTE) round;
        EXPECT_EQ(STATUS_SUCCESS, spedDeliver(&client, &server, STUN_PACKET_TYPE_BINDING_REQUEST, transactionId));
        EXPECT_EQ(STATUS_SUCCESS, spedDeliver(&server, &client, STUN_PACKET_TYPE_BINDING_RESPONSE_SUCCESS, transactionId));
    }
    EXPECT_TRUE(client.connected);
    EXPECT_TRUE(server.connected);

    // ClientHello / server flight / client flight: two rounds, nothing went out plain, nothing was resent
    EXPECT_LE(round, 3);
    EXPECT_EQ(0, client.plainPackets.load());
    EXPECT_EQ(0, server.plainPackets.load());
    EXPECT_EQ(0, client.retransmittedFlights.load());
    EXPECT_EQ(0, server.retransmittedFlights.load());
    EXPECT_LE(client.largestPacket, DTLS_SPED_MTU_SIZE_BYTES);
    EXPECT_LE(server.largestPacket, DTLS_SPED_MTU_SIZE_BYTES);

    // The server's last flight is acknowledged by the next check, then both are done
    transactionId[0] = 0xFF;
    EXPECT_EQ(STATUS_SUCCESS, spedDeliver(&client, &server, STUN_PACKET_TYPE_BINDING_REQUEST, transactionId));
    EXPECT_EQ(STATUS_SUCCESS, spedDeliver(&server, &client, STUN_PACKET_TYPE_BINDING_RESPONSE_SUCCESS, transactionId));
    EXPECT_EQ(DTLS_IN_STUN_STATE_COMPLETE, dtlsInStunGetState(server.pDtlsInStun));
    EXPECT_EQ(DTLS_IN_STUN_STATE_COMPLETE, dtlsInStunGetState(client.pDtlsInStun));

    // Application data is back on the full MTU: 1100 bytes fit one record
    dataPackets = client.plainPackets;
    EXPECT_EQ(STATUS_SUCCESS, dtlsSessionPutApplicationData(client.pSession, data, SIZEOF(data)));
    EXPECT_EQ(dataPackets + 1, client.plainPackets.load());

    spedTeardown(&client);
    spedTeardown(&server);
    timerQueueFree(&timerQueueHandle);
}

// The handshake MTU is set when the session starts, too late after that
TEST_F(DtlsFunctionalityTest, spedEnableAfterStartFails)
{
    TIMER_QUEUE_HANDLE timerQueueHandle = INVALID_TIMER_QUEUE_HANDLE_VALUE;
    PDtlsSession pSession = NULL;
    DtlsSessionCallbacks callbacks;
    SpedSide side;

    MEMSET(&callbacks, 0, SIZEOF(callbacks));
    ASSERT_EQ(STATUS_SUCCESS, timerQueueCreate(&timerQueueHandle));
    ASSERT_EQ(STATUS_SUCCESS, createDtlsSession(&callbacks, timerQueueHandle, 0, FALSE, NULL, &pSession));
    ASSERT_EQ(STATUS_SUCCESS, dtlsSessionOnOutBoundData(pSession, (UINT64) &side, spedOutboundPacket));
    ASSERT_EQ(STATUS_SUCCESS, dtlsSessionStart(pSession, FALSE));
    EXPECT_EQ(STATUS_INVALID_OPERATION, dtlsSessionEnableSped(pSession));

    freeDtlsSession(&pSession);
    timerQueueFree(&timerQueueHandle);
}

// More SPED sessions in one process than OpenSSL has BIO types to hand out (128), which OpenSSL 3 refuses past that
TEST_F(DtlsFunctionalityTest, spedManySessions)
{
    TIMER_QUEUE_HANDLE timerQueueHandle = INVALID_TIMER_QUEUE_HANDLE_VALUE;
    PDtlsSession pSession = NULL;
    DtlsSessionCallbacks callbacks;
    UINT32 i;

    MEMSET(&callbacks, 0, SIZEOF(callbacks));
    ASSERT_EQ(STATUS_SUCCESS, timerQueueCreate(&timerQueueHandle));
    for (i = 0; i < 300; i++) {
        ASSERT_EQ(STATUS_SUCCESS, createDtlsSession(&callbacks, timerQueueHandle, 0, FALSE, NULL, &pSession));
        EXPECT_EQ(STATUS_SUCCESS, dtlsSessionEnableSped(pSession)) << "session " << i;
        freeDtlsSession(&pSession);
    }

    timerQueueFree(&timerQueueHandle);
}

TEST_F(DtlsFunctionalityTest, spedRetransmitHold)
{
    TIMER_QUEUE_HANDLE timerQueueHandle = INVALID_TIMER_QUEUE_HANDLE_VALUE;
    SpedSide client;

    EXPECT_EQ(STATUS_SUCCESS, timerQueueCreate(&timerQueueHandle));
    ASSERT_EQ(STATUS_SUCCESS, spedSetup(&client, timerQueueHandle, FALSE));
    EXPECT_EQ(1, client.flights.load());

    // The DTLS stack would resend after 1 s and again after 2 s more
    THREAD_SLEEP(2500 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
    EXPECT_EQ(1, client.flights.load());

    // Released, the retransmission timer starts over
    EXPECT_EQ(STATUS_SUCCESS, dtlsSessionSetRetransmitHold(client.pSession, FALSE));
    THREAD_SLEEP(500 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
    EXPECT_EQ(1, client.flights.load());
    THREAD_SLEEP(1000 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
    EXPECT_EQ(2, client.flights.load());
    EXPECT_EQ(1, client.retransmittedFlights.load());

    spedTeardown(&client);
    timerQueueFree(&timerQueueHandle);
}

} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com
