#include "WebRTCClientTestFixture.h"

namespace com { namespace amazonaws { namespace kinesis { namespace video { namespace webrtcclient {

    class IceApiTest : public WebRtcClientTestBase {
    };

    TEST_F(IceApiTest, ConnectionListenerApiTest)
    {
        PConnectionListener pConnectionListener = NULL;
        PSocketConnection pDummySocketConnection = NULL;
        KvsIpAddress localhost;

        localhost.family = KVS_IP_FAMILY_TYPE_IPV4;
        localhost.isPointToPoint = FALSE;
        // 127.0.0.1
        localhost.address[0] = 0x7f;
        localhost.address[1] = 0x00;
        localhost.address[2] = 0x00;
        localhost.address[3] = 0x01;
        localhost.port = 0;

        MEMSET(&localhost, 0x00, SIZEOF(KvsIpAddress));

        EXPECT_EQ(STATUS_SUCCESS, createSocketConnection(&localhost, NULL, KVS_SOCKET_PROTOCOL_UDP, 0, NULL, 0, &pDummySocketConnection));

        EXPECT_NE(STATUS_SUCCESS, createConnectionListener(NULL));
        EXPECT_NE(STATUS_SUCCESS, freeConnectionListener(NULL));
        EXPECT_NE(STATUS_SUCCESS, connectionListenerRemoveConnection(NULL, NULL));
        EXPECT_NE(STATUS_SUCCESS, connectionListenerAddConnection(NULL, NULL));
        EXPECT_NE(STATUS_SUCCESS, connectionListenerStart(NULL));

        EXPECT_EQ(STATUS_SUCCESS, createConnectionListener(&pConnectionListener));
        EXPECT_NE(STATUS_SUCCESS, connectionListenerRemoveConnection(pConnectionListener, NULL));
        EXPECT_NE(STATUS_SUCCESS, connectionListenerAddConnection(pConnectionListener, NULL));

        EXPECT_EQ(STATUS_SUCCESS, connectionListenerAddConnection(pConnectionListener, pDummySocketConnection));
        EXPECT_EQ(STATUS_SUCCESS, connectionListenerRemoveConnection(pConnectionListener, pDummySocketConnection));

        // pDummySocketConnection is freed too
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
        KvsIpAddress testIpAddr;
        SocketConnection testSocketConn;
        TurnConnection testTurnConn;

        MEMSET(&testIpAddr, 0x0, SIZEOF(KvsIpAddress));
        MEMSET(&testSocketConn, 0x0, SIZEOF(SocketConnection));
        MEMSET(&testTurnConn, 0x0, SIZEOF(TurnConnection));
        MEMSET(&testBuffer, 0x0, testBufferLen);
        MEMSET(&testPassword, 0x0, testPasswordLen);

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

    TEST_F(IceApiTest, IceUtilPopulateIpFromStringTest)
    {
        KvsIpAddress kvsIpAddress;
        PCHAR ipString = (PCHAR) "16.213.65.123";
        BYTE expectIpAddr[IPV4_ADDRESS_LENGTH] = {0x10, 0xD5, 0x41, 0x7B};
        PCHAR ipString2 = (PCHAR) "255.255.255.255";
        PCHAR ipString2Longer = (PCHAR) "255.255.255.255 34388 222.222.222.222";
        BYTE expectIpAddr2[IPV4_ADDRESS_LENGTH] = {0xFF, 0xFF, 0xFF, 0xFF};

        MEMSET(&kvsIpAddress, 0x0, SIZEOF(KvsIpAddress));

        EXPECT_NE(STATUS_SUCCESS, populateIpFromString(NULL, NULL));
        EXPECT_NE(STATUS_SUCCESS, populateIpFromString(&kvsIpAddress, NULL));
        EXPECT_NE(STATUS_SUCCESS, populateIpFromString(&kvsIpAddress, (PCHAR) ""));
        EXPECT_NE(STATUS_SUCCESS, populateIpFromString(NULL, ipString));

        EXPECT_EQ(STATUS_SUCCESS, populateIpFromString(&kvsIpAddress, ipString));
        EXPECT_EQ(0, MEMCMP(expectIpAddr, kvsIpAddress.address, IPV4_ADDRESS_LENGTH));

        EXPECT_EQ(STATUS_SUCCESS, populateIpFromString(&kvsIpAddress, ipString2));
        EXPECT_EQ(0, MEMCMP(expectIpAddr2, kvsIpAddress.address, IPV4_ADDRESS_LENGTH));

        EXPECT_EQ(STATUS_SUCCESS, populateIpFromString(&kvsIpAddress, ipString2Longer));
        EXPECT_EQ(0, MEMCMP(expectIpAddr2, kvsIpAddress.address, IPV4_ADDRESS_LENGTH));
    }

}
}
}
}
}
