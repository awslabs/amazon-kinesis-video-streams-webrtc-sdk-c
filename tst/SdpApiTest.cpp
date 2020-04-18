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

TEST_F(SdpApiTest, populateSingleMediaSection_TestTxSendRecv) {
    PRtcPeerConnection offerPc = NULL;
    RtcConfiguration configuration;
    RtcSessionDescriptionInit sessionDescriptionInit;

    MEMSET(&configuration, 0x00, SIZEOF(RtcConfiguration));

    // Create peer connection
    EXPECT_EQ(createPeerConnection(&configuration, &offerPc), STATUS_SUCCESS);

    RtcMediaStreamTrack track;
    PRtcRtpTransceiver pTransceiver;
    RtcRtpTransceiverInit rtcRtpTransceiverInit;
    rtcRtpTransceiverInit.direction = RTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV;

    MEMSET(&track, 0x00, SIZEOF(RtcMediaStreamTrack));

    track.kind = MEDIA_STREAM_TRACK_KIND_VIDEO;
    track.codec = RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE;
    STRCPY(track.streamId, "myKvsVideoStream");
    STRCPY(track.trackId, "myTrack");

    EXPECT_EQ(STATUS_SUCCESS, addTransceiver(offerPc, &track, &rtcRtpTransceiverInit, &pTransceiver));
    EXPECT_EQ(STATUS_SUCCESS, createOffer(offerPc, &sessionDescriptionInit));
    EXPECT_PRED_FORMAT2(testing::IsSubstring, "sendrecv", sessionDescriptionInit.sdp);

    freePeerConnection(&offerPc);
}

TEST_F(SdpApiTest, populateSingleMediaSection_TestTxSendOnly) {

    PRtcPeerConnection offerPc = NULL;
    RtcConfiguration configuration;
    RtcSessionDescriptionInit sessionDescriptionInit;

    MEMSET(&configuration, 0x00, SIZEOF(RtcConfiguration));

    // Create peer connection
    EXPECT_EQ(createPeerConnection(&configuration, &offerPc), STATUS_SUCCESS);

    RtcMediaStreamTrack track;
    PRtcRtpTransceiver pTransceiver;
    RtcRtpTransceiverInit rtcRtpTransceiverInit;
    rtcRtpTransceiverInit.direction = RTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY;

    MEMSET(&track, 0x00, SIZEOF(RtcMediaStreamTrack));

    track.kind = MEDIA_STREAM_TRACK_KIND_VIDEO;
    track.codec = RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE;
    STRCPY(track.streamId, "myKvsVideoStream");
    STRCPY(track.trackId, "myTrack");

    EXPECT_EQ(STATUS_SUCCESS, addTransceiver(offerPc, &track, &rtcRtpTransceiverInit, &pTransceiver));
    EXPECT_EQ(STATUS_SUCCESS, createOffer(offerPc, &sessionDescriptionInit));
    EXPECT_PRED_FORMAT2(testing::IsSubstring, "sendonly", sessionDescriptionInit.sdp);

    freePeerConnection(&offerPc);
}

TEST_F(SdpApiTest, populateSingleMediaSection_TestTxRecvOnly) {

    PRtcPeerConnection offerPc = NULL;
    RtcConfiguration configuration;
    RtcSessionDescriptionInit sessionDescriptionInit;

    MEMSET(&configuration, 0x00, SIZEOF(RtcConfiguration));

    // Create peer connection
    EXPECT_EQ(createPeerConnection(&configuration, &offerPc), STATUS_SUCCESS);

    RtcMediaStreamTrack track;
    PRtcRtpTransceiver pTransceiver;
    RtcRtpTransceiverInit rtcRtpTransceiverInit;
    rtcRtpTransceiverInit.direction = RTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY;

    MEMSET(&track, 0x00, SIZEOF(RtcMediaStreamTrack));

    track.kind = MEDIA_STREAM_TRACK_KIND_VIDEO;
    track.codec = RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE;
    STRCPY(track.streamId, "myKvsVideoStream");
    STRCPY(track.trackId, "myTrack");

    EXPECT_EQ(STATUS_SUCCESS, addTransceiver(offerPc, &track, &rtcRtpTransceiverInit, &pTransceiver));
    EXPECT_EQ(STATUS_SUCCESS, createOffer(offerPc, &sessionDescriptionInit));
    EXPECT_PRED_FORMAT2(testing::IsSubstring, "recvonly", sessionDescriptionInit.sdp);

    freePeerConnection(&offerPc);
}

TEST_F(SdpApiTest, populateSingleMediaSection_TestPayloadNoFmtp) {
    auto remoteSessionDescription = R"(v=0
o=- 7732334361409071710 2 IN IP4 127.0.0.1
s=-
t=0 0
a=group:BUNDLE 0
a=msid-semantic: WMS
m=video 16485 UDP/TLS/RTP/SAVPF 96 102
c=IN IP4 205.251.233.176
a=rtcp:9 IN IP4 0.0.0.0
a=ice-ufrag:9YRc
a=ice-pwd:/ELMEiczRSsx2OEi2ynq+TbZ
a=ice-options:trickle
a=fingerprint:sha-256 51:04:F9:20:45:5C:9D:85:AF:D7:AF:FB:2B:F8:DB:24:66:7B:6A:E3:E3:EF:EC:72:93:6E:01:B8:C9:53:A6:31
a=setup:actpass
a=mid:1
a=recvonly
a=rtcp-mux
a=rtcp-rsize
a=rtpmap:96 VP8/90000
a=rtpmap:102 H264/90000
)";

    PRtcPeerConnection pRtcPeerConnection = NULL;
    PRtcRtpTransceiver pRtcRtpTransceiver = NULL;
    RtcConfiguration rtcConfiguration;
    RtcMediaStreamTrack rtcMediaStreamTrack;
    RtcRtpTransceiverInit rtcRtpTransceiverInit;
    RtcSessionDescriptionInit rtcSessionDescriptionInit;

    MEMSET(&rtcConfiguration, 0x00, SIZEOF(RtcConfiguration));
    MEMSET(&rtcMediaStreamTrack, 0x00, SIZEOF(RtcMediaStreamTrack));
    MEMSET(&rtcSessionDescriptionInit, 0x00, SIZEOF(RtcSessionDescriptionInit));

    EXPECT_EQ(createPeerConnection(&rtcConfiguration, &pRtcPeerConnection), STATUS_SUCCESS);
    EXPECT_EQ(addSupportedCodec(pRtcPeerConnection, RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE), STATUS_SUCCESS);

    rtcRtpTransceiverInit.direction = RTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY;
    rtcMediaStreamTrack.kind = MEDIA_STREAM_TRACK_KIND_VIDEO;
    rtcMediaStreamTrack.codec = RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE;
    STRCPY(rtcMediaStreamTrack.streamId, "myKvsVideoStream");
    STRCPY(rtcMediaStreamTrack.trackId, "myTrack");
    EXPECT_EQ(addTransceiver(pRtcPeerConnection, &rtcMediaStreamTrack, &rtcRtpTransceiverInit, &pRtcRtpTransceiver), STATUS_SUCCESS);

    STRCPY(rtcSessionDescriptionInit.sdp, remoteSessionDescription);
    rtcSessionDescriptionInit.type = SDP_TYPE_OFFER;
    EXPECT_EQ(setRemoteDescription(pRtcPeerConnection, &rtcSessionDescriptionInit), STATUS_SUCCESS);
    EXPECT_EQ(createAnswer(pRtcPeerConnection, &rtcSessionDescriptionInit), STATUS_SUCCESS);
    EXPECT_PRED_FORMAT2(testing::IsNotSubstring, "fmtp:102 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42e01f", rtcSessionDescriptionInit.sdp);
    freePeerConnection(&pRtcPeerConnection);
}

// i receive offer for two video tracks with the same codec
// i add two transceivers with VP8 tracks
// expected answer MUST contain two different ssrc
TEST_F(SdpApiTest, twoVideoTracksWithSameCodec) {
    auto offer = R"(v=0
o=- 59125572 1587098218 IN IP4 0.0.0.0
s=-
t=0 0
a=fingerprint:sha-256 4D:FD:34:E5:97:F5:DA:21:1F:97:36:3D:92:FB:29:A7:7C:5E:27:E3:C9:2E:8E:0A:04:6B:92:DF:CB:72:41:E6
a=group:BUNDLE 0 1 2
m=application 9 DTLS/SCTP 5000
c=IN IP4 0.0.0.0
a=setup:actpass
a=mid:0
a=sendrecv
a=sctpmap:5000 webrtc-datachannel 1024
a=ice-ufrag:AIYDVLsshQYmwjKt
a=ice-pwd:nfUKrCyCexbfghChxlytInZADRmiHrNQ
a=candidate:foundation 1 udp 16777215 10.128.132.55 62872 typ relay raddr 0.0.0.0 rport 46485 generation 0
a=candidate:foundation 2 udp 16777215 10.128.132.55 62872 typ relay raddr 0.0.0.0 rport 46485 generation 0
a=end-of-candidates
a=candidate:foundation 1 udp 16777215 10.128.132.55 62872 typ relay raddr 0.0.0.0 rport 46485 generation 0
a=candidate:foundation 2 udp 16777215 10.128.132.55 62872 typ relay raddr 0.0.0.0 rport 46485 generation 0
a=end-of-candidates
m=video 9 UDP/TLS/RTP/SAVPF 96
c=IN IP4 0.0.0.0
a=setup:actpass
a=mid:1
a=ice-ufrag:AIYDVLsshQYmwjKt
a=ice-pwd:nfUKrCyCexbfghChxlytInZADRmiHrNQ
a=rtcp-mux
a=rtcp-rsize
a=rtpmap:96 VP8/90000
a=recvonly
a=candidate:foundation 1 udp 16777215 10.128.132.55 62872 typ relay raddr 0.0.0.0 rport 46485 generation 0
a=candidate:foundation 2 udp 16777215 10.128.132.55 62872 typ relay raddr 0.0.0.0 rport 46485 generation 0
a=end-of-candidates
a=candidate:foundation 1 udp 16777215 10.128.132.55 62872 typ relay raddr 0.0.0.0 rport 46485 generation 0
a=candidate:foundation 2 udp 16777215 10.128.132.55 62872 typ relay raddr 0.0.0.0 rport 46485 generation 0
a=end-of-candidates
m=video 9 UDP/TLS/RTP/SAVPF 96
c=IN IP4 0.0.0.0
a=setup:actpass
a=mid:2
a=ice-ufrag:AIYDVLsshQYmwjKt
a=ice-pwd:nfUKrCyCexbfghChxlytInZADRmiHrNQ
a=rtcp-mux
a=rtcp-rsize
a=rtpmap:96 VP8/90000
a=recvonly
a=candidate:foundation 1 udp 16777215 10.128.132.55 62872 typ relay raddr 0.0.0.0 rport 46485 generation 0
a=candidate:foundation 2 udp 16777215 10.128.132.55 62872 typ relay raddr 0.0.0.0 rport 46485 generation 0
a=end-of-candidates
a=candidate:foundation 1 udp 16777215 10.128.132.55 62872 typ relay raddr 0.0.0.0 rport 46485 generation 0
a=candidate:foundation 2 udp 16777215 10.128.132.55 62872 typ relay raddr 0.0.0.0 rport 46485 generation 0
a=end-of-candidates
)";
    RtcConfiguration configuration{};
    PRtcPeerConnection pRtcPeerConnection = nullptr;
    RtcMediaStreamTrack track1{};
    RtcMediaStreamTrack track2{};
    PRtcRtpTransceiver transceiver1 = nullptr;
    PRtcRtpTransceiver transceiver2 = nullptr;
    RtcSessionDescriptionInit offerSdp{};
    RtcSessionDescriptionInit answerSdp{};

    SNPRINTF(configuration.iceServers[0].urls, MAX_ICE_CONFIG_URI_LEN, KINESIS_VIDEO_STUN_URL, TEST_DEFAULT_REGION);

    track1.kind  = MEDIA_STREAM_TRACK_KIND_VIDEO;
    track1.codec = RTC_CODEC_VP8;
    STRNCPY(track1.streamId, "track1", MAX_MEDIA_STREAM_ID_LEN);
    STRNCPY(track1.trackId, "track1", MAX_MEDIA_STREAM_ID_LEN);

    track2.kind  = MEDIA_STREAM_TRACK_KIND_VIDEO;
    track2.codec = RTC_CODEC_VP8;
    STRNCPY(track2.streamId, "track2", MAX_MEDIA_STREAM_ID_LEN);
    STRNCPY(track2.trackId, "track2", MAX_MEDIA_STREAM_ID_LEN);

    offerSdp.type = SDP_TYPE_OFFER;
    STRNCPY(offerSdp.sdp, offer, MAX_SESSION_DESCRIPTION_INIT_SDP_LEN);

    EXPECT_EQ(STATUS_SUCCESS, createPeerConnection(&configuration, &pRtcPeerConnection));
    EXPECT_EQ(STATUS_SUCCESS, addSupportedCodec(pRtcPeerConnection, RTC_CODEC_VP8));
    EXPECT_EQ(STATUS_SUCCESS, addTransceiver(pRtcPeerConnection, &track1, nullptr, &transceiver1));
    EXPECT_EQ(STATUS_SUCCESS, addTransceiver(pRtcPeerConnection, &track2, nullptr, &transceiver2));

    EXPECT_EQ(STATUS_SUCCESS,setRemoteDescription(pRtcPeerConnection, &offerSdp));
    EXPECT_EQ(STATUS_SUCCESS,createAnswer(pRtcPeerConnection, &answerSdp));

    std::string answer = answerSdp.sdp;
    std::set<std::string> ssrcLines;

    std::size_t current, previous = 0;
    current = answer.find("a=ssrc:");
    while (current != std::string::npos) {
        const auto pos = answer.find_first_of(' ', current);
        const auto &ssrc = answer.substr(current, pos - current);
        ssrcLines.insert(ssrc);
        previous = current + 1;
        current = answer.find("a=ssrc:", previous);
    }

    ASSERT_EQ(2, ssrcLines.size());
}


TEST_F(SdpApiTest, populateSingleMediaSection_TestPayloadFmtp) {
    auto remoteSessionDescription = R"(v=0
o=- 7732334361409071710 2 IN IP4 127.0.0.1
s=-
t=0 0
a=group:BUNDLE 0
a=msid-semantic: WMS
m=video 16485 UDP/TLS/RTP/SAVPF 96 102
c=IN IP4 205.251.233.176
a=rtcp:9 IN IP4 0.0.0.0
a=ice-ufrag:9YRc
a=ice-pwd:/ELMEiczRSsx2OEi2ynq+TbZ
a=ice-options:trickle
a=fingerprint:sha-256 51:04:F9:20:45:5C:9D:85:AF:D7:AF:FB:2B:F8:DB:24:66:7B:6A:E3:E3:EF:EC:72:93:6E:01:B8:C9:53:A6:31
a=setup:actpass
a=mid:1
a=recvonly
a=rtcp-mux
a=rtcp-rsize
a=rtpmap:96 VP8/90000
a=rtpmap:102 H264/90000
a=fmtp:102 strange
)";

    PRtcPeerConnection pRtcPeerConnection = NULL;
    PRtcRtpTransceiver pRtcRtpTransceiver = NULL;
    RtcConfiguration rtcConfiguration;
    RtcMediaStreamTrack rtcMediaStreamTrack;
    RtcRtpTransceiverInit rtcRtpTransceiverInit;
    RtcSessionDescriptionInit rtcSessionDescriptionInit;

    MEMSET(&rtcConfiguration, 0x00, SIZEOF(RtcConfiguration));
    MEMSET(&rtcMediaStreamTrack, 0x00, SIZEOF(RtcMediaStreamTrack));
    MEMSET(&rtcSessionDescriptionInit, 0x00, SIZEOF(RtcSessionDescriptionInit));

    EXPECT_EQ(createPeerConnection(&rtcConfiguration, &pRtcPeerConnection), STATUS_SUCCESS);
    EXPECT_EQ(addSupportedCodec(pRtcPeerConnection, RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE), STATUS_SUCCESS);

    rtcRtpTransceiverInit.direction = RTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY;
    rtcMediaStreamTrack.kind = MEDIA_STREAM_TRACK_KIND_VIDEO;
    rtcMediaStreamTrack.codec = RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE;
    STRCPY(rtcMediaStreamTrack.streamId, "myKvsVideoStream");
    STRCPY(rtcMediaStreamTrack.trackId, "myTrack");
    EXPECT_EQ(addTransceiver(pRtcPeerConnection, &rtcMediaStreamTrack, &rtcRtpTransceiverInit, &pRtcRtpTransceiver), STATUS_SUCCESS);

    STRCPY(rtcSessionDescriptionInit.sdp, remoteSessionDescription);
    rtcSessionDescriptionInit.type = SDP_TYPE_OFFER;
    EXPECT_EQ(setRemoteDescription(pRtcPeerConnection, &rtcSessionDescriptionInit), STATUS_SUCCESS);
    EXPECT_EQ(createAnswer(pRtcPeerConnection, &rtcSessionDescriptionInit), STATUS_SUCCESS);
    EXPECT_PRED_FORMAT2(testing::IsSubstring, "fmtp:102 strange", rtcSessionDescriptionInit.sdp);
    EXPECT_PRED_FORMAT2(testing::IsNotSubstring, "fmtp:102 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42e01f", rtcSessionDescriptionInit.sdp);
    freePeerConnection(&pRtcPeerConnection);
}

}
}
}
}
}
