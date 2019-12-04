#include "WebRTCClientTestFixture.h"

namespace com { namespace amazonaws { namespace kinesis { namespace video { namespace webrtcclient {

    class IceApiTest : public WebRtcClientTestBase {
    };

    TEST_F(IceApiTest, ConnectionListenerApiTest)
    {
        PConnectionListener pConnectionListener = NULL;
        SocketConnection dummySocketConnection = {0};

        EXPECT_NE(STATUS_SUCCESS, createConnectionListener(NULL));
        EXPECT_NE(STATUS_SUCCESS, freeConnectionListener(NULL));
        EXPECT_NE(STATUS_SUCCESS, connectionListenerRemoveConnection(NULL, NULL));
        EXPECT_NE(STATUS_SUCCESS, connectionListenerAddConnection(NULL, NULL));
        EXPECT_NE(STATUS_SUCCESS, connectionListenerStart(NULL));

        EXPECT_EQ(STATUS_SUCCESS, createConnectionListener(&pConnectionListener));
        EXPECT_NE(STATUS_SUCCESS, connectionListenerRemoveConnection(pConnectionListener, NULL));
        EXPECT_NE(STATUS_SUCCESS, connectionListenerAddConnection(pConnectionListener, NULL));

        EXPECT_EQ(STATUS_SUCCESS, connectionListenerAddConnection(pConnectionListener, &dummySocketConnection));
        EXPECT_EQ(STATUS_SUCCESS, connectionListenerRemoveConnection(pConnectionListener, &dummySocketConnection));

        EXPECT_EQ(STATUS_SUCCESS, freeConnectionListener(&pConnectionListener));
        // free is idempotent
        EXPECT_EQ(STATUS_SUCCESS, freeConnectionListener(&pConnectionListener));
    }

    TEST_F(IceApiTest, IceUtilApiTest)
    {
        PTransactionIdStore pTransactionIdStore;
        BYTE testTransactionId[STUN_TRANSACTION_ID_LEN] = {0};
        BYTE testBuffer[1000], testPassword[30];
        UINT32 testBufferLen = ARRAY_SIZE(testBuffer), testPasswordLen = ARRAY_SIZE(testPassword);
        PStunPacket pStunPacket;
        KvsIpAddress testIpAddr = {0};
        SocketConnection testSocketConn = {0};
        TurnConnection testTurnConn = {0};

        EXPECT_NE(STATUS_SUCCESS, createTransactionIdStore(20, NULL));
        EXPECT_NE(STATUS_SUCCESS, createTransactionIdStore(0, &pTransactionIdStore));
        EXPECT_NE(STATUS_SUCCESS, createTransactionIdStore(MAX_STORED_TRANSACTION_ID_COUNT, &pTransactionIdStore));
        EXPECT_NE(STATUS_SUCCESS, freeTransactionIdStore(NULL));
        EXPECT_DEATH(transactionIdStoreInsert(NULL, testTransactionId), "");
        EXPECT_DEATH(transactionIdStoreHasId(NULL, testTransactionId), "");
        EXPECT_DEATH(transactionIdStoreClear(NULL), "");
        EXPECT_NE(STATUS_SUCCESS, iceUtilsGenerateTransactionId(NULL, STUN_TRANSACTION_ID_LEN));
        EXPECT_NE(STATUS_SUCCESS, iceUtilsGenerateTransactionId(testTransactionId, 0));

        EXPECT_EQ(STATUS_SUCCESS, createStunPacket(STUN_PACKET_TYPE_SEND_INDICATION, NULL, &pStunPacket));
        EXPECT_NE(STATUS_SUCCESS, iceUtilsPackageStunPacket(NULL, testPassword, testPasswordLen, testBuffer, &testBufferLen));
        EXPECT_NE(STATUS_SUCCESS, iceUtilsPackageStunPacket(pStunPacket, NULL, testPasswordLen, testBuffer, &testBufferLen));
        EXPECT_NE(STATUS_SUCCESS, iceUtilsPackageStunPacket(pStunPacket, testPassword, 0, testBuffer, &testBufferLen));
        EXPECT_NE(STATUS_SUCCESS, iceUtilsPackageStunPacket(pStunPacket, testPassword, testPasswordLen, NULL, &testBufferLen));
        EXPECT_NE(STATUS_SUCCESS, iceUtilsPackageStunPacket(pStunPacket, testPassword, testPasswordLen, testBuffer, NULL));

        EXPECT_NE(STATUS_SUCCESS, iceUtilsSendStunPacket(pStunPacket, testPassword, testPasswordLen, &testIpAddr, &testSocketConn, NULL, TRUE));
        EXPECT_NE(STATUS_SUCCESS, iceUtilsSendStunPacket(pStunPacket, testPassword, testPasswordLen, &testIpAddr, NULL, &testTurnConn, FALSE));

        EXPECT_EQ(STATUS_SUCCESS, createTransactionIdStore(20, &pTransactionIdStore));
        transactionIdStoreInsert(pTransactionIdStore, testTransactionId);
        transactionIdStoreHasId(pTransactionIdStore, testTransactionId);
        transactionIdStoreClear(pTransactionIdStore);
        EXPECT_EQ(STATUS_SUCCESS, iceUtilsGenerateTransactionId(testTransactionId, STUN_TRANSACTION_ID_LEN));
        EXPECT_EQ(STATUS_SUCCESS, iceUtilsPackageStunPacket(pStunPacket, testPassword, testPasswordLen, testBuffer, &testBufferLen));
        EXPECT_EQ(STATUS_SUCCESS, iceUtilsPackageStunPacket(pStunPacket, NULL, 0, testBuffer, &testBufferLen));

        EXPECT_EQ(STATUS_SUCCESS, freeStunPacket(&pStunPacket));
        EXPECT_EQ(STATUS_SUCCESS, freeTransactionIdStore(&pTransactionIdStore));
    }

}
}
}
}
}