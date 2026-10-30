#include "WebRTCClientTestFixture.h"

namespace com {
namespace amazonaws {
namespace kinesis {
namespace video {
namespace webrtcclient {

class DtlsInStunFunctionalityTest : public WebRtcClientTestBase {
  protected:
    PDtlsInStun pDtlsInStun = NULL;
    BYTE nextTransactionId = 0;
    BYTE injected[DTLS_IN_STUN_MAX_PACKET_SIZE];
    UINT32 injectedLen = 0;

    void SetUp() override
    {
        WebRtcClientTestBase::SetUp();
        ASSERT_EQ(STATUS_SUCCESS, createDtlsInStun(&pDtlsInStun));
    }

    void TearDown() override
    {
        EXPECT_EQ(STATUS_SUCCESS, freeDtlsInStun(&pDtlsInStun));
        WebRtcClientTestBase::TearDown();
    }

    // A DTLS 1.2 handshake record with the given payload byte
    static std::vector<BYTE> dtlsPacket(BYTE fill, UINT32 length = 100)
    {
        std::vector<BYTE> packet(length, fill);
        packet[0] = 22;
        packet[1] = 0xfe;
        packet[2] = 0xfd;
        return packet;
    }

    static UINT32 crcOf(std::vector<BYTE>& packet)
    {
        return COMPUTE_CRC32(packet.data(), (UINT32) packet.size());
    }

    PStunPacket newMessage(STUN_PACKET_TYPE type, PBYTE transactionId = NULL)
    {
        BYTE id[STUN_TRANSACTION_ID_LEN] = {0};
        PStunPacket pStunPacket = NULL;

        if (transactionId == NULL) {
            id[0] = ++nextTransactionId;
            transactionId = id;
        }
        EXPECT_EQ(STATUS_SUCCESS, createStunPacket(type, transactionId, &pStunPacket));
        EXPECT_EQ(STATUS_SUCCESS, appendStunUsernameAttribute(pStunPacket, (PCHAR) "remote:local"));
        return pStunPacket;
    }

    // What the peer sends: optional ACKs, optional embedded packet
    STATUS receive(STUN_PACKET_TYPE type, std::vector<UINT32>* pAcks, std::vector<BYTE>* pData, PBYTE transactionId = NULL)
    {
        PStunPacket pStunPacket = newMessage(type, transactionId);
        STATUS retStatus;

        if (pAcks != NULL) {
            EXPECT_EQ(STATUS_SUCCESS, appendStunDtlsInStunAckAttribute(pStunPacket, pAcks->data(), (UINT32) pAcks->size()));
        }
        if (pData != NULL) {
            EXPECT_EQ(STATUS_SUCCESS, appendStunDtlsInStunAttribute(pStunPacket, pData->data(), (UINT16) pData->size()));
        }
        retStatus = dtlsInStunOnStunMessage(pDtlsInStun, pStunPacket, injected, &injectedLen);
        freeStunPacket(&pStunPacket);
        return retStatus;
    }

    // What we send: returns the message so the test can look at the attributes
    PStunPacket send(STUN_PACKET_TYPE type, BOOL embed = TRUE, UINT32 maxSize = 1200)
    {
        PStunPacket pStunPacket = newMessage(type);
        EXPECT_EQ(STATUS_SUCCESS, dtlsInStunAppendAttributes(pDtlsInStun, pStunPacket, maxSize, embed));
        return pStunPacket;
    }

    static PStunAttributeHeader attributeOf(PStunPacket pStunPacket, STUN_ATTRIBUTE_TYPE type)
    {
        PStunAttributeHeader pAttribute = NULL;
        EXPECT_EQ(STATUS_SUCCESS, getStunAttribute(pStunPacket, type, &pAttribute));
        return pAttribute;
    }

    static std::vector<UINT32> acksOf(PStunPacket pStunPacket)
    {
        UINT32 acks[STUN_DTLS_IN_STUN_ACK_MAX_RECEIVE], count = 0;
        PStunAttributeHeader pAttribute = attributeOf(pStunPacket, STUN_ATTRIBUTE_TYPE_DTLS_IN_STUN_ACK);

        if (pAttribute == NULL) {
            return {};
        }
        EXPECT_EQ(STATUS_SUCCESS, getStunDtlsInStunAcks(pAttribute, acks, &count));
        return std::vector<UINT32>(acks, acks + count);
    }

    static std::vector<BYTE> embeddedOf(PStunPacket pStunPacket)
    {
        PStunAttributeData pData = (PStunAttributeData) attributeOf(pStunPacket, STUN_ATTRIBUTE_TYPE_DTLS_IN_STUN);
        if (pData == NULL) {
            return {};
        }
        return std::vector<BYTE>(pData->data, pData->data + pData->attribute.length);
    }

    void stashFlight(std::vector<std::vector<BYTE>> packets, BOOL retransmission = FALSE)
    {
        for (auto& packet : packets) {
            EXPECT_TRUE(dtlsInStunCapturePacket(pDtlsInStun, packet.data(), (UINT32) packet.size()));
        }
        EXPECT_EQ(STATUS_SUCCESS, dtlsInStunEndFlight(pDtlsInStun, retransmission));
    }
};

TEST_F(DtlsInStunFunctionalityTest, offUntilStarted)
{
    auto packet = dtlsPacket(1);
    PStunPacket pStunPacket;

    EXPECT_FALSE(dtlsInStunIsActive(pDtlsInStun));
    EXPECT_FALSE(dtlsInStunCapturePacket(pDtlsInStun, packet.data(), (UINT32) packet.size()));
    pStunPacket = send(STUN_PACKET_TYPE_BINDING_REQUEST);
    EXPECT_EQ(1, pStunPacket->attributesCount);
    freeStunPacket(&pStunPacket);
}

TEST_F(DtlsInStunFunctionalityTest, tentativeSendsAcksAndRotatesTheFlight)
{
    auto first = dtlsPacket(1), second = dtlsPacket(2, 300);
    PStunPacket pStunPacket;

    EXPECT_EQ(STATUS_SUCCESS, dtlsInStunStart(pDtlsInStun));
    EXPECT_EQ(DTLS_IN_STUN_STATE_TENTATIVE, dtlsInStunGetState(pDtlsInStun));

    // Before the DTLS session runs, an empty DATA announces support, and there is no ACK attribute
    pStunPacket = send(STUN_PACKET_TYPE_BINDING_RESPONSE_SUCCESS);
    EXPECT_TRUE(attributeOf(pStunPacket, STUN_ATTRIBUTE_TYPE_DTLS_IN_STUN_ACK) == NULL);
    ASSERT_TRUE(attributeOf(pStunPacket, STUN_ATTRIBUTE_TYPE_DTLS_IN_STUN) != NULL);
    EXPECT_EQ(0, attributeOf(pStunPacket, STUN_ATTRIBUTE_TYPE_DTLS_IN_STUN)->length);
    freeStunPacket(&pStunPacket);
    EXPECT_EQ(STATUS_SUCCESS, dtlsInStunSetDtlsStarted(pDtlsInStun));

    // Nothing to embed yet: an empty ACK announces support
    pStunPacket = send(STUN_PACKET_TYPE_BINDING_REQUEST);
    EXPECT_TRUE(attributeOf(pStunPacket, STUN_ATTRIBUTE_TYPE_DTLS_IN_STUN_ACK) != NULL);
    EXPECT_TRUE(attributeOf(pStunPacket, STUN_ATTRIBUTE_TYPE_DTLS_IN_STUN) == NULL);
    freeStunPacket(&pStunPacket);

    stashFlight({first, second});
    pStunPacket = send(STUN_PACKET_TYPE_BINDING_REQUEST);
    EXPECT_EQ(first, embeddedOf(pStunPacket));
    freeStunPacket(&pStunPacket);
    pStunPacket = send(STUN_PACKET_TYPE_BINDING_RESPONSE_SUCCESS);
    EXPECT_EQ(second, embeddedOf(pStunPacket));
    freeStunPacket(&pStunPacket);
    pStunPacket = send(STUN_PACKET_TYPE_BINDING_REQUEST);
    EXPECT_EQ(first, embeddedOf(pStunPacket));
    freeStunPacket(&pStunPacket);

    // Pacing: acknowledge only
    pStunPacket = send(STUN_PACKET_TYPE_BINDING_REQUEST, FALSE);
    EXPECT_TRUE(attributeOf(pStunPacket, STUN_ATTRIBUTE_TYPE_DTLS_IN_STUN) == NULL);
    freeStunPacket(&pStunPacket);
}

TEST_F(DtlsInStunFunctionalityTest, peerWithoutSupportTurnsItOff)
{
    auto packet = dtlsPacket(1);
    PStunPacket pStunPacket;
    DtlsInStunPacket packets[DTLS_IN_STUN_MAX_STASHED_PACKETS];
    UINT32 count = 0;

    EXPECT_EQ(STATUS_SUCCESS, dtlsInStunStart(pDtlsInStun));
    stashFlight({packet});
    EXPECT_EQ(STATUS_SUCCESS, receive(STUN_PACKET_TYPE_BINDING_REQUEST, NULL, NULL));
    EXPECT_EQ(DTLS_IN_STUN_STATE_OFF, dtlsInStunGetState(pDtlsInStun));
    pStunPacket = send(STUN_PACKET_TYPE_BINDING_RESPONSE_SUCCESS);
    EXPECT_EQ(1, pStunPacket->attributesCount);
    freeStunPacket(&pStunPacket);
    EXPECT_FALSE(dtlsInStunCapturePacket(pDtlsInStun, packet.data(), (UINT32) packet.size()));

    // The flight the peer never got is still there to send plain
    EXPECT_EQ(STATUS_SUCCESS, dtlsInStunTakeStash(pDtlsInStun, packets, &count));
    ASSERT_EQ(1, count);
    EXPECT_EQ(packet, std::vector<BYTE>(packets[0].pData, packets[0].pData + packets[0].length));
    SAFE_MEMFREE(packets[0].pData);

    // Stopping drops it
    EXPECT_EQ(STATUS_SUCCESS, dtlsInStunStart(pDtlsInStun));
    stashFlight({packet});
    EXPECT_EQ(STATUS_SUCCESS, dtlsInStunStop(pDtlsInStun));
    EXPECT_EQ(STATUS_SUCCESS, dtlsInStunTakeStash(pDtlsInStun, packets, &count));
    EXPECT_EQ(0, count);
}

TEST_F(DtlsInStunFunctionalityTest, confirmedAnnouncesWithEmptyData)
{
    std::vector<UINT32> noAcks;
    PStunPacket pStunPacket;
    PStunAttributeHeader pAttribute;

    EXPECT_EQ(STATUS_SUCCESS, dtlsInStunStart(pDtlsInStun));
    EXPECT_EQ(STATUS_SUCCESS, receive(STUN_PACKET_TYPE_BINDING_REQUEST, &noAcks, NULL));
    EXPECT_EQ(DTLS_IN_STUN_STATE_CONFIRMED, dtlsInStunGetState(pDtlsInStun));
    pStunPacket = send(STUN_PACKET_TYPE_BINDING_RESPONSE_SUCCESS);
    pAttribute = attributeOf(pStunPacket, STUN_ATTRIBUTE_TYPE_DTLS_IN_STUN);
    ASSERT_TRUE(pAttribute != NULL);
    EXPECT_EQ(0, pAttribute->length);
    freeStunPacket(&pStunPacket);
}

TEST_F(DtlsInStunFunctionalityTest, embeddedPacketsWaitForTheDtlsSession)
{
    auto clientHello = dtlsPacket(7);
    std::vector<UINT32> noAcks;
    PStunPacket pStunPacket;

    EXPECT_EQ(STATUS_SUCCESS, dtlsInStunStart(pDtlsInStun));

    // The DTLS session can not take it yet: neither injected nor acknowledged, not even implicitly by an ACK attribute
    // in the response
    EXPECT_EQ(STATUS_SUCCESS, receive(STUN_PACKET_TYPE_BINDING_REQUEST, &noAcks, &clientHello));
    EXPECT_EQ(0, injectedLen);
    pStunPacket = send(STUN_PACKET_TYPE_BINDING_RESPONSE_SUCCESS);
    EXPECT_TRUE(attributeOf(pStunPacket, STUN_ATTRIBUTE_TYPE_DTLS_IN_STUN_ACK) == NULL);
    freeStunPacket(&pStunPacket);

    EXPECT_EQ(0, pDtlsInStun->receivedPacketCount);
    EXPECT_EQ(STATUS_SUCCESS, dtlsInStunSetDtlsStarted(pDtlsInStun));
    EXPECT_EQ(STATUS_SUCCESS, receive(STUN_PACKET_TYPE_BINDING_REQUEST, &noAcks, &clientHello));
    EXPECT_EQ(1, pDtlsInStun->receivedPacketCount);
    ASSERT_EQ(clientHello.size(), injectedLen);
    EXPECT_EQ(0, MEMCMP(clientHello.data(), injected, injectedLen));
    pStunPacket = send(STUN_PACKET_TYPE_BINDING_RESPONSE_SUCCESS);
    EXPECT_EQ(std::vector<UINT32>{crcOf(clientHello)}, acksOf(pStunPacket));
    freeStunPacket(&pStunPacket);
}

TEST_F(DtlsInStunFunctionalityTest, nonDtlsDataIsDropped)
{
    std::vector<BYTE> notDtls(50, 0x80);
    std::vector<UINT32> noAcks;

    EXPECT_EQ(STATUS_SUCCESS, dtlsInStunStart(pDtlsInStun));
    EXPECT_EQ(STATUS_SUCCESS, dtlsInStunSetDtlsStarted(pDtlsInStun));
    EXPECT_EQ(STATUS_SUCCESS, receive(STUN_PACKET_TYPE_BINDING_REQUEST, &noAcks, &notDtls));
    EXPECT_EQ(0, injectedLen);
    EXPECT_FALSE(dtlsInStunCapturePacket(pDtlsInStun, notDtls.data(), (UINT32) notDtls.size()));
}

TEST_F(DtlsInStunFunctionalityTest, acksPruneTheStash)
{
    auto first = dtlsPacket(1), second = dtlsPacket(2);
    std::vector<UINT32> ackFirst{crcOf(first)}, ackSecond{crcOf(second)};
    PStunPacket pStunPacket;

    EXPECT_EQ(STATUS_SUCCESS, dtlsInStunStart(pDtlsInStun));
    stashFlight({first, second});
    EXPECT_EQ(STATUS_SUCCESS, receive(STUN_PACKET_TYPE_BINDING_REQUEST, &ackFirst, NULL));
    for (int i = 0; i < 3; i++) {
        pStunPacket = send(STUN_PACKET_TYPE_BINDING_REQUEST);
        EXPECT_EQ(second, embeddedOf(pStunPacket));
        freeStunPacket(&pStunPacket);
    }
    EXPECT_EQ(STATUS_SUCCESS, receive(STUN_PACKET_TYPE_BINDING_REQUEST, &ackSecond, NULL));
    pStunPacket = send(STUN_PACKET_TYPE_BINDING_REQUEST);
    EXPECT_TRUE(embeddedOf(pStunPacket).empty());
    freeStunPacket(&pStunPacket);
}

TEST_F(DtlsInStunFunctionalityTest, responseImplicitlyAcksItsRequest)
{
    auto packet = dtlsPacket(1);
    std::vector<UINT32> noAcks;
    PStunPacket pRequest, pStunPacket;

    EXPECT_EQ(STATUS_SUCCESS, dtlsInStunStart(pDtlsInStun));
    stashFlight({packet});
    pRequest = send(STUN_PACKET_TYPE_BINDING_REQUEST);
    EXPECT_EQ(packet, embeddedOf(pRequest));

    // A response to a request sentRequests does not hold acknowledges only what its ACK attribute lists
    EXPECT_EQ(STATUS_SUCCESS, receive(STUN_PACKET_TYPE_BINDING_RESPONSE_SUCCESS, &noAcks, NULL));
    EXPECT_EQ(DTLS_IN_STUN_STATE_CONFIRMED, dtlsInStunGetState(pDtlsInStun));
    pStunPacket = send(STUN_PACKET_TYPE_BINDING_REQUEST);
    EXPECT_EQ(packet, embeddedOf(pStunPacket));
    freeStunPacket(&pStunPacket);

    // An ACK attribute, even an empty one, in the response acknowledges what the request carried
    EXPECT_EQ(STATUS_SUCCESS, receive(STUN_PACKET_TYPE_BINDING_RESPONSE_SUCCESS, &noAcks, NULL, pRequest->header.transactionId));
    EXPECT_EQ(DTLS_IN_STUN_STATE_CONFIRMED, dtlsInStunGetState(pDtlsInStun));
    freeStunPacket(&pRequest);
    pRequest = send(STUN_PACKET_TYPE_BINDING_REQUEST);
    EXPECT_EQ(0, embeddedOf(pRequest).size());
    freeStunPacket(&pRequest);
}

TEST_F(DtlsInStunFunctionalityTest, responseToAnEvictedRequestStillCarriesData)
{
    auto serverHello = dtlsPacket(5);
    std::vector<UINT32> noAcks;
    BYTE firstId[STUN_TRANSACTION_ID_LEN];
    PStunPacket pRequest;

    EXPECT_EQ(STATUS_SUCCESS, dtlsInStunStart(pDtlsInStun));
    EXPECT_EQ(STATUS_SUCCESS, dtlsInStunSetDtlsStarted(pDtlsInStun));

    // Without the attributes it says nothing about support: its request may not have carried them either
    EXPECT_EQ(STATUS_SUCCESS, receive(STUN_PACKET_TYPE_BINDING_RESPONSE_SUCCESS, NULL, NULL));
    EXPECT_EQ(DTLS_IN_STUN_STATE_TENTATIVE, dtlsInStunGetState(pDtlsInStun));

    for (UINT32 i = 0; i <= DTLS_IN_STUN_MAX_SENT_REQUESTS; i++) {
        pRequest = send(STUN_PACKET_TYPE_BINDING_REQUEST);
        if (i == 0) {
            MEMCPY(firstId, pRequest->header.transactionId, STUN_TRANSACTION_ID_LEN);
        }
        freeStunPacket(&pRequest);
    }

    // The first request has left sentRequests; the peer's packet in its response still goes to DTLS
    EXPECT_EQ(STATUS_SUCCESS, receive(STUN_PACKET_TYPE_BINDING_RESPONSE_SUCCESS, &noAcks, &serverHello, firstId));
    EXPECT_EQ(DTLS_IN_STUN_STATE_CONFIRMED, dtlsInStunGetState(pDtlsInStun));
    ASSERT_EQ(serverHello.size(), injectedLen);
    EXPECT_EQ(0, MEMCMP(serverHello.data(), injected, injectedLen));
}

TEST_F(DtlsInStunFunctionalityTest, malformedAcksAreIgnored)
{
    auto clientHello = dtlsPacket(3);
    BYTE threeBytes[] = {1, 2, 3};
    PStunPacket pStunPacket = newMessage(STUN_PACKET_TYPE_BINDING_REQUEST);

    EXPECT_EQ(STATUS_SUCCESS, dtlsInStunStart(pDtlsInStun));
    EXPECT_EQ(STATUS_SUCCESS, dtlsInStunSetDtlsStarted(pDtlsInStun));
    EXPECT_EQ(STATUS_SUCCESS, appendStunDtlsInStunAttribute(pStunPacket, threeBytes, SIZEOF(threeBytes)));
    attributeOf(pStunPacket, STUN_ATTRIBUTE_TYPE_DTLS_IN_STUN)->type = STUN_ATTRIBUTE_TYPE_DTLS_IN_STUN_ACK;
    EXPECT_EQ(STATUS_SUCCESS, appendStunDtlsInStunAttribute(pStunPacket, clientHello.data(), (UINT16) clientHello.size()));
    EXPECT_EQ(STATUS_SUCCESS, dtlsInStunOnStunMessage(pDtlsInStun, pStunPacket, injected, &injectedLen));
    freeStunPacket(&pStunPacket);

    // Neither taken for a peer without SPED nor acted on
    EXPECT_EQ(DTLS_IN_STUN_STATE_TENTATIVE, dtlsInStunGetState(pDtlsInStun));
    EXPECT_EQ(0, injectedLen);
}

TEST_F(DtlsInStunFunctionalityTest, retransmittedFlightKeepsTheStash)
{
    auto flight = dtlsPacket(1), resent = dtlsPacket(2), next = dtlsPacket(3);
    PStunPacket pStunPacket;

    EXPECT_EQ(STATUS_SUCCESS, dtlsInStunStart(pDtlsInStun));
    stashFlight({flight});
    stashFlight({resent}, TRUE);
    pStunPacket = send(STUN_PACKET_TYPE_BINDING_REQUEST);
    EXPECT_EQ(flight, embeddedOf(pStunPacket));
    freeStunPacket(&pStunPacket);

    // A new flight replaces it
    stashFlight({next});
    pStunPacket = send(STUN_PACKET_TYPE_BINDING_REQUEST);
    EXPECT_EQ(next, embeddedOf(pStunPacket));
    freeStunPacket(&pStunPacket);
}

TEST_F(DtlsInStunFunctionalityTest, atMostFourAcksNewestKept)
{
    std::vector<std::vector<BYTE>> packets;
    std::vector<UINT32> expected;
    PStunPacket pStunPacket;
    int i;

    EXPECT_EQ(STATUS_SUCCESS, dtlsInStunStart(pDtlsInStun));
    EXPECT_EQ(STATUS_SUCCESS, dtlsInStunSetDtlsStarted(pDtlsInStun));
    for (i = 0; i < 6; i++) {
        packets.push_back(dtlsPacket((BYTE) (10 + i)));
        EXPECT_EQ(STATUS_SUCCESS, dtlsInStunOnDtlsPacket(pDtlsInStun, packets[i].data(), (UINT32) packets[i].size()));
        // Duplicates are acknowledged once
        EXPECT_EQ(STATUS_SUCCESS, dtlsInStunOnDtlsPacket(pDtlsInStun, packets[i].data(), (UINT32) packets[i].size()));
    }
    for (i = 2; i < 6; i++) {
        expected.push_back(crcOf(packets[i]));
    }
    pStunPacket = send(STUN_PACKET_TYPE_BINDING_REQUEST);
    EXPECT_EQ(expected, acksOf(pStunPacket));
    freeStunPacket(&pStunPacket);
}

TEST_F(DtlsInStunFunctionalityTest, sizeBudgetIsRespected)
{
    auto big = dtlsPacket(1, 900);
    PStunPacket pStunPacket;

    EXPECT_EQ(STATUS_SUCCESS, dtlsInStunStart(pDtlsInStun));
    EXPECT_EQ(STATUS_SUCCESS, dtlsInStunSetDtlsStarted(pDtlsInStun));
    stashFlight({big});

    // Room for the ACK only
    pStunPacket = send(STUN_PACKET_TYPE_BINDING_REQUEST, TRUE, 200);
    EXPECT_TRUE(attributeOf(pStunPacket, STUN_ATTRIBUTE_TYPE_DTLS_IN_STUN_ACK) != NULL);
    EXPECT_TRUE(attributeOf(pStunPacket, STUN_ATTRIBUTE_TYPE_DTLS_IN_STUN) == NULL);
    freeStunPacket(&pStunPacket);

    // No room at all
    pStunPacket = send(STUN_PACKET_TYPE_BINDING_REQUEST, TRUE, 40);
    EXPECT_EQ(1, pStunPacket->attributesCount);
    freeStunPacket(&pStunPacket);

    pStunPacket = send(STUN_PACKET_TYPE_BINDING_REQUEST, TRUE, 1200);
    EXPECT_EQ(big, embeddedOf(pStunPacket));
    freeStunPacket(&pStunPacket);
}

TEST_F(DtlsInStunFunctionalityTest, clientCompletesOnceTheServerIsHeardFrom)
{
    auto finished = dtlsPacket(1);
    std::vector<UINT32> noAcks;

    EXPECT_EQ(STATUS_SUCCESS, dtlsInStunStart(pDtlsInStun));
    EXPECT_EQ(STATUS_SUCCESS, receive(STUN_PACKET_TYPE_BINDING_REQUEST, &noAcks, NULL));
    stashFlight({finished});

    // DTLS 1.2 client: the server's last flight proves it has ours, nothing left to send
    EXPECT_EQ(STATUS_SUCCESS, dtlsInStunOnHandshakeComplete(pDtlsInStun, TRUE));
    EXPECT_EQ(DTLS_IN_STUN_STATE_PENDING, dtlsInStunGetState(pDtlsInStun));
    EXPECT_EQ(STATUS_SUCCESS, receive(STUN_PACKET_TYPE_BINDING_REQUEST, &noAcks, NULL));
    EXPECT_EQ(DTLS_IN_STUN_STATE_COMPLETE, dtlsInStunGetState(pDtlsInStun));
    EXPECT_FALSE(dtlsInStunIsActive(pDtlsInStun));
}

TEST_F(DtlsInStunFunctionalityTest, serverKeepsItsLastFlightUntilAcked)
{
    auto lastFlight = dtlsPacket(4);
    std::vector<UINT32> noAcks, ackLast{crcOf(lastFlight)};
    PStunPacket pStunPacket;

    EXPECT_EQ(STATUS_SUCCESS, dtlsInStunStart(pDtlsInStun));
    EXPECT_EQ(STATUS_SUCCESS, receive(STUN_PACKET_TYPE_BINDING_REQUEST, &noAcks, NULL));

    // The last flight has no retransmission timer behind it, so no end of flight either
    EXPECT_TRUE(dtlsInStunCapturePacket(pDtlsInStun, lastFlight.data(), (UINT32) lastFlight.size()));
    EXPECT_EQ(STATUS_SUCCESS, dtlsInStunOnHandshakeComplete(pDtlsInStun, FALSE));
    pStunPacket = send(STUN_PACKET_TYPE_BINDING_RESPONSE_SUCCESS);
    EXPECT_EQ(lastFlight, embeddedOf(pStunPacket));
    freeStunPacket(&pStunPacket);

    EXPECT_EQ(STATUS_SUCCESS, receive(STUN_PACKET_TYPE_BINDING_REQUEST, &noAcks, NULL));
    EXPECT_EQ(DTLS_IN_STUN_STATE_PENDING, dtlsInStunGetState(pDtlsInStun));
    EXPECT_EQ(STATUS_SUCCESS, receive(STUN_PACKET_TYPE_BINDING_REQUEST, &ackLast, NULL));
    EXPECT_EQ(DTLS_IN_STUN_STATE_COMPLETE, dtlsInStunGetState(pDtlsInStun));
}

TEST_F(DtlsInStunFunctionalityTest, applicationDataCompletes)
{
    auto lastFlight = dtlsPacket(4);
    std::vector<UINT32> noAcks;

    EXPECT_EQ(STATUS_SUCCESS, dtlsInStunStart(pDtlsInStun));
    EXPECT_EQ(STATUS_SUCCESS, receive(STUN_PACKET_TYPE_BINDING_REQUEST, &noAcks, NULL));
    stashFlight({lastFlight});
    EXPECT_EQ(STATUS_SUCCESS, dtlsInStunOnApplicationData(pDtlsInStun));
    EXPECT_EQ(DTLS_IN_STUN_STATE_CONFIRMED, dtlsInStunGetState(pDtlsInStun));
    EXPECT_EQ(STATUS_SUCCESS, dtlsInStunOnHandshakeComplete(pDtlsInStun, FALSE));
    EXPECT_EQ(STATUS_SUCCESS, dtlsInStunOnApplicationData(pDtlsInStun));
    EXPECT_EQ(DTLS_IN_STUN_STATE_COMPLETE, dtlsInStunGetState(pDtlsInStun));
}

TEST_F(DtlsInStunFunctionalityTest, failedDtlsTurnsItOff)
{
    EXPECT_EQ(STATUS_SUCCESS, dtlsInStunStart(pDtlsInStun));
    EXPECT_EQ(STATUS_SUCCESS, dtlsInStunOnDtlsFailed(pDtlsInStun));
    EXPECT_EQ(DTLS_IN_STUN_STATE_OFF, dtlsInStunGetState(pDtlsInStun));
}

TEST_F(DtlsInStunFunctionalityTest, takeStashForPlainSending)
{
    auto first = dtlsPacket(1), second = dtlsPacket(2, 250), later = dtlsPacket(3);
    std::vector<UINT32> noAcks;
    DtlsInStunPacket packets[DTLS_IN_STUN_MAX_STASHED_PACKETS];
    UINT32 count = 0, i;
    PStunPacket pStunPacket;

    EXPECT_EQ(STATUS_SUCCESS, dtlsInStunStart(pDtlsInStun));
    EXPECT_EQ(STATUS_SUCCESS, dtlsInStunSetDtlsStarted(pDtlsInStun));
    EXPECT_EQ(STATUS_SUCCESS, receive(STUN_PACKET_TYPE_BINDING_REQUEST, &noAcks, NULL));
    stashFlight({first, second});
    EXPECT_EQ(STATUS_SUCCESS, dtlsInStunTakeStash(pDtlsInStun, packets, &count));
    ASSERT_EQ(2, count);
    EXPECT_EQ(first, std::vector<BYTE>(packets[0].pData, packets[0].pData + packets[0].length));
    EXPECT_EQ(second, std::vector<BYTE>(packets[1].pData, packets[1].pData + packets[1].length));
    for (i = 0; i < count; i++) {
        SAFE_MEMFREE(packets[i].pData);
    }

    // Once the flight went out plain, a response only acknowledges: no stale flight, no later one, no empty announcement
    EXPECT_FALSE(dtlsInStunCapturePacket(pDtlsInStun, later.data(), (UINT32) later.size()));
    EXPECT_EQ(DTLS_IN_STUN_STATE_CONFIRMED, dtlsInStunGetState(pDtlsInStun));
    pStunPacket = send(STUN_PACKET_TYPE_BINDING_RESPONSE_SUCCESS);
    EXPECT_NE(nullptr, attributeOf(pStunPacket, STUN_ATTRIBUTE_TYPE_DTLS_IN_STUN_ACK));
    EXPECT_EQ(nullptr, attributeOf(pStunPacket, STUN_ATTRIBUTE_TYPE_DTLS_IN_STUN));
    freeStunPacket(&pStunPacket);
}

} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com
