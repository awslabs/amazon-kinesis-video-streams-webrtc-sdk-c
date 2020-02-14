#include "WebRTCClientTestFixture.h"

namespace com { namespace amazonaws { namespace kinesis { namespace video { namespace webrtcclient {

class SdpApiTest : public WebRtcClientTestBase {
};

TEST_F(SdpApiTest, serializeSessionDescription_NoMedia)
{
    auto sessionDescriptionNoMedia = R"(v=2
o=- 1904080082932320671 2 IN IP4 127.0.0.1
s=-
t=0 0
a=group:BUNDLE 0 1
a=msid-semantic: WMS f327e13b-3518-47fc-8b53-9cf74d22d03e
)";

    SessionDescription sessionDescription;
    MEMSET(&sessionDescription, 0x00, SIZEOF(SessionDescription));
    EXPECT_EQ(serializeSessionDescription(&sessionDescription, (PCHAR) sessionDescriptionNoMedia), STATUS_SUCCESS);

    EXPECT_EQ(sessionDescription.sessionAttributesCount, 2);

    EXPECT_STREQ(sessionDescription.sdpAttributes[0].attributeName, "group");
    EXPECT_STREQ(sessionDescription.sdpAttributes[0].attributeValue, "BUNDLE 0 1");

    EXPECT_STREQ(sessionDescription.sdpAttributes[1].attributeName, "msid-semantic");
    EXPECT_STREQ(sessionDescription.sdpAttributes[1].attributeValue, " WMS f327e13b-3518-47fc-8b53-9cf74d22d03e");
}

TEST_F(SdpApiTest, serializeSessionDescription_Media)
{
    auto sessionDescriptionMedia = R"(v=2
o=- 1904080082932320671 2 IN IP4 127.0.0.1
s=-
t=0 0
a=group:BUNDLE 0 1
a=msid-semantic: WMS f327e13b-3518-47fc-8b53-9cf74d22d03e
m=audio 3554 UDP/TLS/RTP/SAVPF 111 103 9 102 0 8 105 13 110 113 126
a=candidate:1682923840 1 udp 2113937151 10.111.144.78 63135 typ host generation 0 network-cost 999
a=ssrc:1030548471 cname:AZdzrek14WN2tYrw
m=video 15632 UDP/TLS/RTP/SAVPF 96 97 98 99 100 101 127 125 104
a=ssrc:45567500 cname:AZdzrek14WN2tYrw
a=candidate:842163049 1 udp 1677729535 54.240.196.188 15632 typ srflx raddr 10.111.144.78 rport 53846 generation 0 network-cost 999
)";

    SessionDescription sessionDescription;
    MEMSET(&sessionDescription, 0x00, SIZEOF(SessionDescription));
    EXPECT_EQ(serializeSessionDescription(&sessionDescription, (PCHAR) sessionDescriptionMedia), STATUS_SUCCESS);

    EXPECT_EQ(sessionDescription.mediaCount, 2);

    EXPECT_STREQ(sessionDescription.mediaDescriptions[0].mediaName, "audio 3554 UDP/TLS/RTP/SAVPF 111 103 9 102 0 8 105 13 110 113 126");
    EXPECT_EQ(sessionDescription.mediaDescriptions[0].mediaAttributesCount, 2);
    EXPECT_STREQ(sessionDescription.mediaDescriptions[0].sdpAttributes[0].attributeName, "candidate");
    EXPECT_STREQ(sessionDescription.mediaDescriptions[0].sdpAttributes[0].attributeValue, "1682923840 1 udp 2113937151 10.111.144.78 63135 typ host generation 0 network-cost 999");

    EXPECT_STREQ(sessionDescription.mediaDescriptions[1].mediaName, "video 15632 UDP/TLS/RTP/SAVPF 96 97 98 99 100 101 127 125 104");
    EXPECT_EQ(sessionDescription.mediaDescriptions[1].mediaAttributesCount, 2);
    EXPECT_STREQ(sessionDescription.mediaDescriptions[1].sdpAttributes[0].attributeName, "ssrc");
    EXPECT_STREQ(sessionDescription.mediaDescriptions[1].sdpAttributes[0].attributeValue, "45567500 cname:AZdzrek14WN2tYrw");
}

auto populate_session_description = [](PSessionDescription pSessionDescription) {
    MEMSET(pSessionDescription, 0x00, SIZEOF(SessionDescription));

    pSessionDescription->version = 2;

    STRCPY(pSessionDescription->sdpOrigin.userName, "-");
    pSessionDescription->sdpOrigin.sessionId = 1904080082932320671;
    pSessionDescription->sdpOrigin.sessionVersion = 2;
    STRCPY(pSessionDescription->sdpOrigin.sdpConnectionInformation.networkType, "IN");
    STRCPY(pSessionDescription->sdpOrigin.sdpConnectionInformation.addressType, "IP4");
    STRCPY(pSessionDescription->sdpOrigin.sdpConnectionInformation.connectionAddress, "127.0.0.1");

    STRCPY(pSessionDescription->sessionName, "-");

    pSessionDescription->timeDescriptionCount = 1;
    pSessionDescription->sdpTimeDescription[0].startTime = 0;
    pSessionDescription->sdpTimeDescription[0].stopTime = 0;

    pSessionDescription->sessionAttributesCount = 2;
    STRCPY(pSessionDescription->sdpAttributes[0].attributeName, "group");
    STRCPY(pSessionDescription->sdpAttributes[0].attributeValue, "BUNDLE 0 1");

    STRCPY(pSessionDescription->sdpAttributes[1].attributeName, "msid-semantic");
    STRCPY(pSessionDescription->sdpAttributes[1].attributeValue, " WMS f327e13b-3518-47fc-8b53-9cf74d22d03e");
};

TEST_F(SdpApiTest, deserializeSessionDescription_NoMedia)
{
    auto sessionDescriptionNoMedia = R"(v=2
o=- 1904080082932320671 2 IN IP4 127.0.0.1
s=-
t=0 0
a=group:BUNDLE 0 1
a=msid-semantic: WMS f327e13b-3518-47fc-8b53-9cf74d22d03e
)";

    SessionDescription sessionDescription;
    UINT32 buff_len = 0, invalid_buffer_len = 5;
    std::unique_ptr<CHAR[]> buff(new CHAR[135]);

    populate_session_description(&sessionDescription);

    EXPECT_EQ(deserializeSessionDescription(&sessionDescription, NULL, &buff_len), STATUS_SUCCESS);
    EXPECT_EQ(buff_len, 135);

    std::fill_n(buff.get(), buff_len, '\0');

    EXPECT_EQ(deserializeSessionDescription(&sessionDescription, buff.get(), &invalid_buffer_len), STATUS_BUFFER_TOO_SMALL);

    EXPECT_EQ(deserializeSessionDescription(&sessionDescription, buff.get(), &buff_len), STATUS_SUCCESS);
    EXPECT_STREQ(buff.get(), sessionDescriptionNoMedia);
}

TEST_F(SdpApiTest, deserializeSessionDescription_Media)
{
    auto sessionDescriptionNoMedia = R"(v=2
o=- 1904080082932320671 2 IN IP4 127.0.0.1
s=-
t=0 0
a=group:BUNDLE 0 1
a=msid-semantic: WMS f327e13b-3518-47fc-8b53-9cf74d22d03e
m=audio 3554 UDP/TLS/RTP/SAVPF 111 103 9 102 0 8 105 13 110 113 126
a=candidate:1682923840 1 udp 2113937151 10.111.144.78 63135 typ host generation 0 network-cost 999
m=video 15632 UDP/TLS/RTP/SAVPF 96 97 98 99 100 101 127 125 104
a=ssrc:45567500 cname:AZdzrek14WN2tYrw
)";

    SessionDescription sessionDescription;
    UINT32 buff_len = 0, invalid_buffer_len = 5;
    std::unique_ptr<CHAR[]> buff(new CHAR[405]);

    populate_session_description(&sessionDescription);

    sessionDescription.mediaCount = 2;

    STRCPY(sessionDescription.mediaDescriptions[0].mediaName, "audio 3554 UDP/TLS/RTP/SAVPF 111 103 9 102 0 8 105 13 110 113 126");
    sessionDescription.mediaDescriptions[0].mediaAttributesCount = 1;

    STRCPY(sessionDescription.mediaDescriptions[0].sdpAttributes[0].attributeName, "candidate");
    STRCPY(sessionDescription.mediaDescriptions[0].sdpAttributes[0].attributeValue, "1682923840 1 udp 2113937151 10.111.144.78 63135 typ host generation 0 network-cost 999");

    STRCPY(sessionDescription.mediaDescriptions[1].mediaName, "video 15632 UDP/TLS/RTP/SAVPF 96 97 98 99 100 101 127 125 104");
    sessionDescription.mediaDescriptions[1].mediaAttributesCount = 1;

    STRCPY(sessionDescription.mediaDescriptions[1].sdpAttributes[0].attributeName, "ssrc");
    STRCPY(sessionDescription.mediaDescriptions[1].sdpAttributes[0].attributeValue, "45567500 cname:AZdzrek14WN2tYrw");

    EXPECT_EQ(deserializeSessionDescription(&sessionDescription, NULL, &buff_len), STATUS_SUCCESS);
    EXPECT_EQ(buff_len, 405);

    std::fill_n(buff.get(), buff_len, '\0');

    EXPECT_EQ(deserializeSessionDescription(&sessionDescription, buff.get(), &invalid_buffer_len), STATUS_BUFFER_TOO_SMALL);

    EXPECT_EQ(deserializeSessionDescription(&sessionDescription, buff.get(), &buff_len), STATUS_SUCCESS);
    EXPECT_STREQ(buff.get(), sessionDescriptionNoMedia);
}

TEST_F(SdpApiTest, deserializeSessionDescription_AttributeOverflow)
{
    std::string sessionDescriptionNoMedia = R"(v=2
o=- 1904080082932320671 2 IN IP4 127.0.0.1
s=-
t=0 0
)";

    for (auto i = 0; i < 250; i++) {
        sessionDescriptionNoMedia += "a=b\n";
    }

    SessionDescription sessionDescription;
    MEMSET(&sessionDescription, 0x00, SIZEOF(SessionDescription));
    EXPECT_EQ(serializeSessionDescription(&sessionDescription, (PCHAR) sessionDescriptionNoMedia.c_str()), STATUS_SDP_ATTRIBUTE_MAX_EXCEEDED);
}

TEST_F(SdpApiTest, setTransceiverPayloadTypes_NoRtxType) {
    PHashTable pCodecTable;
    PHashTable pRtxTable;
    PDoubleList pTransceivers;
    KvsRtpTransceiver transceiver;
    transceiver.sender.track.codec = RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE;
    transceiver.transceiver.direction = RTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV;
    transceiver.sender.packetBuffer = NULL;
    transceiver.sender.retransmitter = NULL;
    EXPECT_EQ(STATUS_SUCCESS, hashTableCreate(&pCodecTable));
    EXPECT_EQ(STATUS_SUCCESS, hashTablePut(pCodecTable, RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE, 1));
    EXPECT_EQ(STATUS_SUCCESS, hashTableCreate(&pRtxTable));
    EXPECT_EQ(STATUS_SUCCESS, doubleListCreate(&pTransceivers));
    EXPECT_EQ(STATUS_SUCCESS, doubleListInsertItemHead(pTransceivers, (UINT64) (&transceiver)));
    EXPECT_EQ(STATUS_SUCCESS, setTransceiverPayloadTypes(pCodecTable, pRtxTable, pTransceivers));
    EXPECT_EQ(1, transceiver.sender.payloadType);
    EXPECT_NE((PRtpRollingBuffer) NULL, transceiver.sender.packetBuffer);
    EXPECT_NE((PRetransmitter) NULL, transceiver.sender.retransmitter);
    hashTableFree(pCodecTable);
    hashTableFree(pRtxTable);
    freeRtpRollingBuffer(&transceiver.sender.packetBuffer);
    freeRetransmitter(&transceiver.sender.retransmitter);
    doubleListFree(pTransceivers);
}

TEST_F(SdpApiTest, setTransceiverPayloadTypes_HasRtxType) {
    PHashTable pCodecTable;
    PHashTable pRtxTable;
    PDoubleList pTransceivers;
    KvsRtpTransceiver transceiver;
    transceiver.sender.track.codec = RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE;
    transceiver.transceiver.direction = RTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV;
    transceiver.sender.packetBuffer = NULL;
    transceiver.sender.retransmitter = NULL;
    EXPECT_EQ(STATUS_SUCCESS, hashTableCreate(&pCodecTable));
    EXPECT_EQ(STATUS_SUCCESS, hashTablePut(pCodecTable, RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE, 1));
    EXPECT_EQ(STATUS_SUCCESS, hashTableCreate(&pRtxTable));
    EXPECT_EQ(STATUS_SUCCESS, hashTablePut(pRtxTable, RTC_RTX_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE, 2));
    EXPECT_EQ(STATUS_SUCCESS, doubleListCreate(&pTransceivers));
    EXPECT_EQ(STATUS_SUCCESS, doubleListInsertItemHead(pTransceivers, (UINT64) (&transceiver)));
    EXPECT_EQ(STATUS_SUCCESS, setTransceiverPayloadTypes(pCodecTable, pRtxTable, pTransceivers));
    EXPECT_EQ(1, transceiver.sender.payloadType);
    EXPECT_EQ(2, transceiver.sender.rtxPayloadType);
    EXPECT_NE((PRtpRollingBuffer) NULL, transceiver.sender.packetBuffer);
    EXPECT_NE((PRetransmitter) NULL, transceiver.sender.retransmitter);
    hashTableFree(pCodecTable);
    hashTableFree(pRtxTable);
    freeRtpRollingBuffer(&transceiver.sender.packetBuffer);
    freeRetransmitter(&transceiver.sender.retransmitter);
    doubleListFree(pTransceivers);
}


}
}
}
}
}
