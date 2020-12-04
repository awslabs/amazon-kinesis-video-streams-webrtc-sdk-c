#include <fstream>
#include <sstream>
#include <tuple>

#include "WebRTCClientTestFixture.h"

namespace com {
namespace amazonaws {
namespace kinesis {
namespace video {
namespace webrtcclient {

class SdpApiTest : public WebRtcClientTestBase {
};

/*
 * Parameter expected in certain SDP API tests. First parameter is a filename.
 * Second parameter is a string that is expected to match in the SDP answer.
 */
using SdpMatch = std::tuple<CHAR const*, CHAR const*>;

/*
 * Processes SDP API entries from file.
 */
class SdpApiTest_SdpMatch : public WebRtcClientTestBase, public ::testing::WithParamInterface<SdpMatch> {
  protected:
    CHAR const* sdp()
    {
        return std::get<0>(GetParam());
    }
    CHAR const* match()
    {
        return std::get<1>(GetParam());
    }
};

auto lfToCRLF = [](PCHAR sdp, INT32 sdpLen) -> std::string {
    std::string newSDP;
    INT32 i;
    CHAR c;

    for (i = 0; i < sdpLen; i++) {
        c = sdp[i];
        if (c == '\n') {
            newSDP += "\r\n";
        } else {
            newSDP += c;
        }
    }

    return newSDP;
};

template <typename Func> void assertLFAndCRLF(PCHAR sdp, INT32 sdpLen, Func&& assertFn)
{
    assertFn(sdp);
    auto converted = lfToCRLF(sdp, sdpLen);
    assertFn((PCHAR) converted.c_str());
};

TEST_F(SdpApiTest, deserializeSessionDescription_NoMedia)
{
    CHAR sessionDescriptionNoMedia[] = R"(v=2
o=- 1904080082932320671 2 IN IP4 127.0.0.1
s=-
t=0 0
a=group:BUNDLE 0 1
a=ice-options:trickle
a=msid-semantic: WMS f327e13b-3518-47fc-8b53-9cf74d22d03e
)";

    assertLFAndCRLF(sessionDescriptionNoMedia, ARRAY_SIZE(sessionDescriptionNoMedia) - 1, [](PCHAR sdp) {
        SessionDescription sessionDescription;
        MEMSET(&sessionDescription, 0x00, SIZEOF(SessionDescription));
        EXPECT_EQ(deserializeSessionDescription(&sessionDescription, sdp), STATUS_SUCCESS);

        EXPECT_EQ(sessionDescription.sessionAttributesCount, 3);

        EXPECT_STREQ(sessionDescription.sdpAttributes[0].attributeName, "group");
        EXPECT_STREQ(sessionDescription.sdpAttributes[0].attributeValue, "BUNDLE 0 1");

        EXPECT_STREQ(sessionDescription.sdpAttributes[1].attributeName, "ice-options");
        EXPECT_STREQ(sessionDescription.sdpAttributes[1].attributeValue, "trickle");

        EXPECT_STREQ(sessionDescription.sdpAttributes[2].attributeName, "msid-semantic");
        EXPECT_STREQ(sessionDescription.sdpAttributes[2].attributeValue, " WMS f327e13b-3518-47fc-8b53-9cf74d22d03e");
    });
}

TEST_F(SdpApiTest, deserializeSessionDescription_Media)
{
    CHAR sessionDescriptionMedia[] = R"(v=2
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

    assertLFAndCRLF(sessionDescriptionMedia, ARRAY_SIZE(sessionDescriptionMedia) - 1, [](PCHAR sdp) {
        SessionDescription sessionDescription;
        MEMSET(&sessionDescription, 0x00, SIZEOF(SessionDescription));
        EXPECT_EQ(deserializeSessionDescription(&sessionDescription, sdp), STATUS_SUCCESS);

        EXPECT_EQ(sessionDescription.mediaCount, 2);

        EXPECT_STREQ(sessionDescription.mediaDescriptions[0].mediaName, "audio 3554 UDP/TLS/RTP/SAVPF 111 103 9 102 0 8 105 13 110 113 126");
        EXPECT_EQ(sessionDescription.mediaDescriptions[0].mediaAttributesCount, 2);
        EXPECT_STREQ(sessionDescription.mediaDescriptions[0].sdpAttributes[0].attributeName, "candidate");
        EXPECT_STREQ(sessionDescription.mediaDescriptions[0].sdpAttributes[0].attributeValue,
                     "1682923840 1 udp 2113937151 10.111.144.78 63135 typ host generation 0 network-cost 999");

        EXPECT_STREQ(sessionDescription.mediaDescriptions[1].mediaName, "video 15632 UDP/TLS/RTP/SAVPF 96 97 98 99 100 101 127 125 104");
        EXPECT_EQ(sessionDescription.mediaDescriptions[1].mediaAttributesCount, 2);
        EXPECT_STREQ(sessionDescription.mediaDescriptions[1].sdpAttributes[0].attributeName, "ssrc");
        EXPECT_STREQ(sessionDescription.mediaDescriptions[1].sdpAttributes[0].attributeValue, "45567500 cname:AZdzrek14WN2tYrw");
    });
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

TEST_F(SdpApiTest, serializeSessionDescription_NoMedia)
{
    CHAR sessionDescriptionNoMedia[] = R"(v=2
o=- 1904080082932320671 2 IN IP4 127.0.0.1
s=-
t=0 0
a=group:BUNDLE 0 1
a=msid-semantic: WMS f327e13b-3518-47fc-8b53-9cf74d22d03e
)";

    SessionDescription sessionDescription;
    UINT32 buff_len = 0, invalid_buffer_len = 5;
    const UINT32 expectedLen = ARRAY_SIZE(sessionDescriptionNoMedia) + 6;
    std::unique_ptr<CHAR[]> buff(new CHAR[expectedLen]);

    populate_session_description(&sessionDescription);

    EXPECT_EQ(serializeSessionDescription(&sessionDescription, NULL, &buff_len), STATUS_SUCCESS);
    EXPECT_EQ(buff_len, expectedLen);

    std::fill_n(buff.get(), buff_len, '\0');

    EXPECT_EQ(serializeSessionDescription(&sessionDescription, buff.get(), &invalid_buffer_len), STATUS_BUFFER_TOO_SMALL);

    EXPECT_EQ(serializeSessionDescription(&sessionDescription, buff.get(), &buff_len), STATUS_SUCCESS);
    EXPECT_STREQ(buff.get(), (PCHAR)(lfToCRLF(sessionDescriptionNoMedia, ARRAY_SIZE(sessionDescriptionNoMedia) - 1).c_str()));
}

TEST_F(SdpApiTest, serializeSessionDescription_Media)
{
    CHAR sessionDescriptionNoMedia[] = R"(v=2
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
    const UINT32 expectedLen = ARRAY_SIZE(sessionDescriptionNoMedia) + 10;
    std::unique_ptr<CHAR[]> buff(new CHAR[expectedLen]);

    populate_session_description(&sessionDescription);

    sessionDescription.mediaCount = 2;

    STRCPY(sessionDescription.mediaDescriptions[0].mediaName, "audio 3554 UDP/TLS/RTP/SAVPF 111 103 9 102 0 8 105 13 110 113 126");
    sessionDescription.mediaDescriptions[0].mediaAttributesCount = 1;

    STRCPY(sessionDescription.mediaDescriptions[0].sdpAttributes[0].attributeName, "candidate");
    STRCPY(sessionDescription.mediaDescriptions[0].sdpAttributes[0].attributeValue,
           "1682923840 1 udp 2113937151 10.111.144.78 63135 typ host generation 0 network-cost 999");

    STRCPY(sessionDescription.mediaDescriptions[1].mediaName, "video 15632 UDP/TLS/RTP/SAVPF 96 97 98 99 100 101 127 125 104");
    sessionDescription.mediaDescriptions[1].mediaAttributesCount = 1;

    STRCPY(sessionDescription.mediaDescriptions[1].sdpAttributes[0].attributeName, "ssrc");
    STRCPY(sessionDescription.mediaDescriptions[1].sdpAttributes[0].attributeValue, "45567500 cname:AZdzrek14WN2tYrw");

    EXPECT_EQ(serializeSessionDescription(&sessionDescription, NULL, &buff_len), STATUS_SUCCESS);
    EXPECT_EQ(buff_len, expectedLen);

    std::fill_n(buff.get(), buff_len, '\0');

    EXPECT_EQ(serializeSessionDescription(&sessionDescription, buff.get(), &invalid_buffer_len), STATUS_BUFFER_TOO_SMALL);

    EXPECT_EQ(serializeSessionDescription(&sessionDescription, buff.get(), &buff_len), STATUS_SUCCESS);
    EXPECT_STREQ(buff.get(), (PCHAR) lfToCRLF(sessionDescriptionNoMedia, ARRAY_SIZE(sessionDescriptionNoMedia) - 1).c_str());
}

TEST_F(SdpApiTest, serializeSessionDescription_AttributeOverflow)
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
    auto converted = lfToCRLF((PCHAR) sessionDescriptionNoMedia.c_str(), sessionDescriptionNoMedia.size());
    EXPECT_EQ(deserializeSessionDescription(&sessionDescription, (PCHAR) converted.c_str()), STATUS_SDP_ATTRIBUTE_MAX_EXCEEDED);
}

TEST_F(SdpApiTest, setTransceiverPayloadTypes_NoRtxType)
{
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
    EXPECT_EQ(STATUS_SUCCESS, doubleListInsertItemHead(pTransceivers, (UINT64)(&transceiver)));
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

TEST_F(SdpApiTest, setTransceiverPayloadTypes_HasRtxType)
{
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
    EXPECT_EQ(STATUS_SUCCESS, doubleListInsertItemHead(pTransceivers, (UINT64)(&transceiver)));
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

TEST_F(SdpApiTest, populateSingleMediaSection_TestTxSendRecv)
{
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

    closePeerConnection(offerPc);
    freePeerConnection(&offerPc);
}

TEST_F(SdpApiTest, populateSingleMediaSection_TestTxSendRecvMaxTransceivers)
{
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

    // Max transceivers
    for (UINT32 i = 0; i < MAX_SDP_SESSION_MEDIA_COUNT - 1; i++) {
        EXPECT_EQ(STATUS_SUCCESS, addTransceiver(offerPc, &track, &rtcRtpTransceiverInit, &pTransceiver));
    }

    EXPECT_EQ(STATUS_SUCCESS, createOffer(offerPc, &sessionDescriptionInit));
    EXPECT_PRED_FORMAT2(testing::IsSubstring, "sendrecv", sessionDescriptionInit.sdp);

    // Adding one more should fail
    EXPECT_EQ(STATUS_SUCCESS, addTransceiver(offerPc, &track, &rtcRtpTransceiverInit, &pTransceiver));
    EXPECT_EQ(STATUS_SESSION_DESCRIPTION_MAX_MEDIA_COUNT, createOffer(offerPc, &sessionDescriptionInit));

    closePeerConnection(offerPc);
    freePeerConnection(&offerPc);
}

TEST_F(SdpApiTest, populateSingleMediaSection_TestTxSendOnly)
{
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

    closePeerConnection(offerPc);
    freePeerConnection(&offerPc);
}

TEST_F(SdpApiTest, populateSingleMediaSection_TestTxRecvOnly)
{
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

    closePeerConnection(offerPc);
    freePeerConnection(&offerPc);
}

TEST_F(SdpApiTest, populateSingleMediaSection_TestPayloadNoFmtp)
{
    CHAR remoteSessionDescription[] = R"(v=0
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

    assertLFAndCRLF(remoteSessionDescription, ARRAY_SIZE(remoteSessionDescription) - 1, [](PCHAR sdp) {
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

        STRCPY(rtcSessionDescriptionInit.sdp, (PCHAR) sdp);
        rtcSessionDescriptionInit.type = SDP_TYPE_OFFER;
        EXPECT_EQ(setRemoteDescription(pRtcPeerConnection, &rtcSessionDescriptionInit), STATUS_SUCCESS);
        EXPECT_EQ(TRUE, canTrickleIceCandidates(pRtcPeerConnection).value);
        EXPECT_EQ(createAnswer(pRtcPeerConnection, &rtcSessionDescriptionInit), STATUS_SUCCESS);
        EXPECT_PRED_FORMAT2(testing::IsNotSubstring, "fmtp:102 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42e01f",
                            rtcSessionDescriptionInit.sdp);

        closePeerConnection(pRtcPeerConnection);
        freePeerConnection(&pRtcPeerConnection);
    });
}

const auto sdpdata = R"(m=application 9 DTLS/SCTP 5000
c=IN IP4 0.0.0.0
a=setup:actpass
a=mid:0
a=sendrecv
a=sctpmap:5000 webrtc-datachannel 1024
a=ice-ufrag:WWlXtoHfeAVCwqHc
a=ice-pwd:GvmyTnsfVtQuxuoareyqyAapQRoAeMdp
a=candidate:foundation 1 udp 16777215 10.128.132.55 54118 typ relay raddr 0.0.0.0 rport 56317 generation 0
a=candidate:foundation 2 udp 16777215 10.128.132.55 54118 typ relay raddr 0.0.0.0 rport 56317 generation 0
a=end-of-candidates
a=candidate:foundation 1 udp 16777215 10.128.132.55 54118 typ relay raddr 0.0.0.0 rport 56317 generation 0
a=candidate:foundation 2 udp 16777215 10.128.132.55 54118 typ relay raddr 0.0.0.0 rport 56317 generation 0
a=end-of-candidates)";

const auto sdpvideo = R"(m=video 9 UDP/TLS/RTP/SAVPF 96
c=IN IP4 0.0.0.0
a=setup:actpass
a=mid:1
a=ice-ufrag:WWlXtoHfeAVCwqHc
a=ice-pwd:GvmyTnsfVtQuxuoareyqyAapQRoAeMdp
a=rtcp-mux
a=rtcp-rsize
a=rtpmap:96 VP8/90000
a=recvonly
a=candidate:foundation 1 udp 16777215 10.128.132.55 54118 typ relay raddr 0.0.0.0 rport 56317 generation 0
a=candidate:foundation 2 udp 16777215 10.128.132.55 54118 typ relay raddr 0.0.0.0 rport 56317 generation 0
a=end-of-candidates
a=candidate:foundation 1 udp 16777215 10.128.132.55 54118 typ relay raddr 0.0.0.0 rport 56317 generation 0
a=candidate:foundation 2 udp 16777215 10.128.132.55 54118 typ relay raddr 0.0.0.0 rport 56317 generation 0
a=end-of-candidates)";

const auto sdpaudio_sendrecv_mid0 = R"(m=audio 9 UDP/TLS/RTP/SAVPF 111
c=IN IP4 127.0.0.1
a=msid:myKvsVideoStream myAudioTrack
a=ssrc:1743019002 cname:4PhjhRU0oaDlWmxI
a=ssrc:1743019002 msid:myKvsVideoStream myAudioTrack
a=ssrc:1743019002 mslabel:myKvsVideoStream
a=ssrc:1743019002 label:myAudioTrack
a=rtcp:9 IN IP4 0.0.0.0
a=ice-ufrag:10Po
a=ice-pwd:nwaL7P3ZiD6LKf/f2NRkvE+M
a=ice-options:trickle
a=setup:active
a=mid:0
a=sendrecv
a=rtcp-mux
a=rtcp-rsize
a=rtpmap:111 opus/48000/2
a=fmtp:111 minptime=10;useinbandfec=1
a=rtcp-fb:111 nack)";

TEST_F(SdpApiTest, threeVideoTracksWithSameCodec)
{
    auto offer3 = std::string(R"(v=0
o=- 481034601 1588366671 IN IP4 0.0.0.0
s=-
t=0 0
a=fingerprint:sha-256 87:E6:EC:59:93:76:9F:42:7D:15:17:F6:8F:C4:29:AB:EA:3F:28:B6:DF:F8:14:2F:96:62:2F:16:98:F5:76:E5
a=group:BUNDLE 0 1 2 3
)");
    offer3 += sdpdata;
    offer3 += "\n";
    offer3 += sdpvideo;
    offer3 += "\n";
    offer3 += sdpvideo;
    offer3 += "\n";
    offer3 += sdpvideo;
    offer3 += "\n";
    offer3 += sdpvideo;
    offer3 += "\n";
    offer3 += sdpvideo;
    offer3 += "\n";

    assertLFAndCRLF((PCHAR) offer3.c_str(), offer3.size(), [](PCHAR sdp) {
        SessionDescription sessionDescription;
        MEMSET(&sessionDescription, 0x00, SIZEOF(SessionDescription));
        // as log as Sdp.h  MAX_SDP_SESSION_MEDIA_COUNT 5 this should fail instead of overwriting memory
        EXPECT_EQ(STATUS_BUFFER_TOO_SMALL, deserializeSessionDescription(&sessionDescription, (PCHAR) sdp));
    });
}

// if offer is recvonly answer must be sendonly
TEST_F(SdpApiTest, oneVideoTrack_ValidateDirectionInAnswer)
{
    auto offer = std::string(R"(v=0
o=- 481034601 1588366671 IN IP4 0.0.0.0
s=-
t=0 0
a=fingerprint:sha-256 87:E6:EC:59:93:76:9F:42:7D:15:17:F6:8F:C4:29:AB:EA:3F:28:B6:DF:F8:14:2F:96:62:2F:16:98:F5:76:E5
a=group:BUNDLE 0
)");

    offer += sdpvideo;
    offer += "\n";

    assertLFAndCRLF((PCHAR) offer.c_str(), offer.size(), [](PCHAR sdp) {
        RtcConfiguration configuration{};
        PRtcPeerConnection pRtcPeerConnection = nullptr;
        RtcMediaStreamTrack track1{};
        PRtcRtpTransceiver transceiver1 = nullptr;
        RtcSessionDescriptionInit offerSdp{};
        RtcSessionDescriptionInit answerSdp{};

        SNPRINTF(configuration.iceServers[0].urls, MAX_ICE_CONFIG_URI_LEN, KINESIS_VIDEO_STUN_URL, TEST_DEFAULT_REGION);

        track1.kind = MEDIA_STREAM_TRACK_KIND_VIDEO;
        track1.codec = RTC_CODEC_VP8;
        STRNCPY(track1.streamId, "stream1", MAX_MEDIA_STREAM_ID_LEN);
        STRNCPY(track1.trackId, "track1", MAX_MEDIA_STREAM_TRACK_ID_LEN);

        offerSdp.type = SDP_TYPE_OFFER;
        STRNCPY(offerSdp.sdp, (PCHAR) sdp, MAX_SESSION_DESCRIPTION_INIT_SDP_LEN);

        EXPECT_EQ(STATUS_SUCCESS, createPeerConnection(&configuration, &pRtcPeerConnection));
        EXPECT_EQ(STATUS_SUCCESS, addSupportedCodec(pRtcPeerConnection, RTC_CODEC_VP8));
        EXPECT_EQ(STATUS_SUCCESS, addTransceiver(pRtcPeerConnection, &track1, nullptr, &transceiver1));

        EXPECT_EQ(STATUS_SUCCESS, setRemoteDescription(pRtcPeerConnection, &offerSdp));
        EXPECT_EQ(STATUS_SUCCESS, createAnswer(pRtcPeerConnection, &answerSdp));

        EXPECT_PRED_FORMAT2(testing::IsSubstring, "sendonly", answerSdp.sdp);
        EXPECT_PRED_FORMAT2(testing::IsNotSubstring, "sendrecv", answerSdp.sdp);
        EXPECT_PRED_FORMAT2(testing::IsNotSubstring, "recvonly", answerSdp.sdp);

        closePeerConnection(pRtcPeerConnection);
        EXPECT_EQ(STATUS_SUCCESS, freePeerConnection(&pRtcPeerConnection));
    });
}

// if offer (remote) contains video m-line only then answer (local) should contain video m-line only
// even if local side has other transceivers, i.e. audio
TEST_F(SdpApiTest, offerMediaMultipleDirections_validateAnswerCorrectMatchingDirections)
{
    auto offer = std::string(R"(v=0
o=- 481034601 1588366671 IN IP4 0.0.0.0
s=-
t=0 0
a=fingerprint:sha-256 87:E6:EC:59:93:76:9F:42:7D:15:17:F6:8F:C4:29:AB:EA:3F:28:B6:DF:F8:14:2F:96:62:2F:16:98:F5:76:E5
a=group:BUNDLE 0
)");

    offer += sdpaudio_sendrecv_mid0;
    offer += "\n";
    offer += sdpvideo;
    offer += "\n";

    assertLFAndCRLF((PCHAR) offer.c_str(), offer.size(), [](PCHAR sdp) {
        RtcConfiguration configuration{};
        PRtcPeerConnection pRtcPeerConnection = nullptr;
        RtcMediaStreamTrack track1{};
        RtcMediaStreamTrack track2{};
        PRtcRtpTransceiver transceiver1 = nullptr;
        PRtcRtpTransceiver transceiver2 = nullptr;
        RtcSessionDescriptionInit offerSdp{};
        RtcSessionDescriptionInit answerSdp{};

        SNPRINTF(configuration.iceServers[0].urls, MAX_ICE_CONFIG_URI_LEN, KINESIS_VIDEO_STUN_URL, TEST_DEFAULT_REGION);

        track1.kind = MEDIA_STREAM_TRACK_KIND_VIDEO;
        track1.codec = RTC_CODEC_VP8;
        STRNCPY(track1.streamId, "videoStream", MAX_MEDIA_STREAM_ID_LEN);
        STRNCPY(track1.trackId, "videoTrack1", MAX_MEDIA_STREAM_TRACK_ID_LEN);

        track2.kind = MEDIA_STREAM_TRACK_KIND_AUDIO;
        track2.codec = RTC_CODEC_OPUS;
        STRNCPY(track2.streamId, "audioStream1", MAX_MEDIA_STREAM_ID_LEN);
        STRNCPY(track2.trackId, "audioTrack1", MAX_MEDIA_STREAM_TRACK_ID_LEN);

        offerSdp.type = SDP_TYPE_OFFER;
        STRNCPY(offerSdp.sdp, (PCHAR) sdp, MAX_SESSION_DESCRIPTION_INIT_SDP_LEN);

        EXPECT_EQ(STATUS_SUCCESS, createPeerConnection(&configuration, &pRtcPeerConnection));
        EXPECT_EQ(STATUS_SUCCESS, addSupportedCodec(pRtcPeerConnection, RTC_CODEC_VP8));
        EXPECT_EQ(STATUS_SUCCESS, addSupportedCodec(pRtcPeerConnection, RTC_CODEC_OPUS));

        EXPECT_EQ(STATUS_SUCCESS, addTransceiver(pRtcPeerConnection, &track1, nullptr, &transceiver1));
        EXPECT_EQ(STATUS_SUCCESS, addTransceiver(pRtcPeerConnection, &track2, nullptr, &transceiver2));

        EXPECT_EQ(STATUS_SUCCESS, setRemoteDescription(pRtcPeerConnection, &offerSdp));
        EXPECT_EQ(STATUS_SUCCESS, createAnswer(pRtcPeerConnection, &answerSdp));

        EXPECT_PRED_FORMAT2(testing::IsSubstring, "sendonly", answerSdp.sdp);
        EXPECT_PRED_FORMAT2(testing::IsSubstring, "sendrecv", answerSdp.sdp);

        closePeerConnection(pRtcPeerConnection);
        EXPECT_EQ(STATUS_SUCCESS, freePeerConnection(&pRtcPeerConnection));
    });
}

// if offer (remote) contains video m-line only then answer (local) should contain video m-line only
// even if local side has other transceivers, i.e. audio
TEST_F(SdpApiTest, videoOnlyOffer_validateNoAudioInAnswer)
{
    auto offer = std::string(R"(v=0
o=- 481034601 1588366671 IN IP4 0.0.0.0
s=-
t=0 0
a=fingerprint:sha-256 87:E6:EC:59:93:76:9F:42:7D:15:17:F6:8F:C4:29:AB:EA:3F:28:B6:DF:F8:14:2F:96:62:2F:16:98:F5:76:E5
a=group:BUNDLE 0
)");

    offer += sdpvideo;
    offer += "\n";

    assertLFAndCRLF((PCHAR) offer.c_str(), offer.size(), [](PCHAR sdp) {
        RtcConfiguration configuration{};
        PRtcPeerConnection pRtcPeerConnection = nullptr;
        RtcMediaStreamTrack track1{};
        RtcMediaStreamTrack track2{};
        PRtcRtpTransceiver transceiver1 = nullptr;
        PRtcRtpTransceiver transceiver2 = nullptr;
        RtcSessionDescriptionInit offerSdp{};
        RtcSessionDescriptionInit answerSdp{};

        SNPRINTF(configuration.iceServers[0].urls, MAX_ICE_CONFIG_URI_LEN, KINESIS_VIDEO_STUN_URL, TEST_DEFAULT_REGION);

        track1.kind = MEDIA_STREAM_TRACK_KIND_VIDEO;
        track1.codec = RTC_CODEC_VP8;
        STRNCPY(track1.streamId, "videoStream1", MAX_MEDIA_STREAM_ID_LEN);
        STRNCPY(track1.trackId, "videoTrack1", MAX_MEDIA_STREAM_TRACK_ID_LEN);

        track2.kind = MEDIA_STREAM_TRACK_KIND_AUDIO;
        track2.codec = RTC_CODEC_OPUS;
        STRNCPY(track2.streamId, "audioStream1", MAX_MEDIA_STREAM_ID_LEN);
        STRNCPY(track2.trackId, "audioTrack1", MAX_MEDIA_STREAM_TRACK_ID_LEN);

        offerSdp.type = SDP_TYPE_OFFER;
        STRNCPY(offerSdp.sdp, (PCHAR) sdp, MAX_SESSION_DESCRIPTION_INIT_SDP_LEN);

        EXPECT_EQ(STATUS_SUCCESS, createPeerConnection(&configuration, &pRtcPeerConnection));
        EXPECT_EQ(STATUS_SUCCESS, addSupportedCodec(pRtcPeerConnection, RTC_CODEC_VP8));
        EXPECT_EQ(STATUS_SUCCESS, addSupportedCodec(pRtcPeerConnection, RTC_CODEC_OPUS));

        EXPECT_EQ(STATUS_SUCCESS, addTransceiver(pRtcPeerConnection, &track1, nullptr, &transceiver1));
        EXPECT_EQ(STATUS_SUCCESS, addTransceiver(pRtcPeerConnection, &track2, nullptr, &transceiver2));

        EXPECT_EQ(STATUS_SUCCESS, setRemoteDescription(pRtcPeerConnection, &offerSdp));
        EXPECT_EQ(STATUS_SUCCESS, createAnswer(pRtcPeerConnection, &answerSdp));

        EXPECT_PRED_FORMAT2(testing::IsSubstring, "video", answerSdp.sdp);
        EXPECT_PRED_FORMAT2(testing::IsNotSubstring, "audio", answerSdp.sdp);

        closePeerConnection(pRtcPeerConnection);
        EXPECT_EQ(STATUS_SUCCESS, freePeerConnection(&pRtcPeerConnection));
    });
}

// i receive offer for two video tracks with the same codec
// i add two transceivers with VP8 tracks
// expected answer MUST contain two different ssrc
TEST_F(SdpApiTest, twoVideoTracksWithSameCodec)
{
    auto offer = std::string(R"(v=0
o=- 481034601 1588366671 IN IP4 0.0.0.0
s=-
t=0 0
a=fingerprint:sha-256 87:E6:EC:59:93:76:9F:42:7D:15:17:F6:8F:C4:29:AB:EA:3F:28:B6:DF:F8:14:2F:96:62:2F:16:98:F5:76:E5
a=group:BUNDLE 0 1 2
a=ice-options:trickle
)");
    offer += sdpdata;
    offer += "\n";
    offer += sdpvideo;
    offer += "\n";
    offer += sdpvideo;
    offer += "\n";

    assertLFAndCRLF((PCHAR) offer.c_str(), offer.size(), [](PCHAR sdp) {
        RtcConfiguration configuration{};
        PRtcPeerConnection pRtcPeerConnection = nullptr;
        RtcMediaStreamTrack track1{};
        RtcMediaStreamTrack track2{};
        PRtcRtpTransceiver transceiver1 = nullptr;
        PRtcRtpTransceiver transceiver2 = nullptr;
        RtcSessionDescriptionInit offerSdp{};
        RtcSessionDescriptionInit answerSdp{};

        SNPRINTF(configuration.iceServers[0].urls, MAX_ICE_CONFIG_URI_LEN, KINESIS_VIDEO_STUN_URL, TEST_DEFAULT_REGION);

        track1.kind = MEDIA_STREAM_TRACK_KIND_VIDEO;
        track1.codec = RTC_CODEC_VP8;
        STRNCPY(track1.streamId, "stream1", MAX_MEDIA_STREAM_ID_LEN);
        STRNCPY(track1.trackId, "track1", MAX_MEDIA_STREAM_TRACK_ID_LEN);

        track2.kind = MEDIA_STREAM_TRACK_KIND_VIDEO;
        track2.codec = RTC_CODEC_VP8;
        STRNCPY(track2.streamId, "stream2", MAX_MEDIA_STREAM_ID_LEN);
        STRNCPY(track2.trackId, "track2", MAX_MEDIA_STREAM_TRACK_ID_LEN);

        offerSdp.type = SDP_TYPE_OFFER;
        STRNCPY(offerSdp.sdp, (PCHAR) sdp, MAX_SESSION_DESCRIPTION_INIT_SDP_LEN);

        EXPECT_EQ(STATUS_SUCCESS, createPeerConnection(&configuration, &pRtcPeerConnection));
        EXPECT_EQ(STATUS_SUCCESS, addSupportedCodec(pRtcPeerConnection, RTC_CODEC_VP8));
        EXPECT_EQ(STATUS_SUCCESS, addTransceiver(pRtcPeerConnection, &track1, nullptr, &transceiver1));
        EXPECT_EQ(STATUS_SUCCESS, addTransceiver(pRtcPeerConnection, &track2, nullptr, &transceiver2));

        EXPECT_EQ(STATUS_SUCCESS, setRemoteDescription(pRtcPeerConnection, &offerSdp));
        EXPECT_EQ(TRUE, canTrickleIceCandidates(pRtcPeerConnection).value);
        EXPECT_EQ(STATUS_SUCCESS, createAnswer(pRtcPeerConnection, &answerSdp));

        std::string answer = answerSdp.sdp;
        std::set<std::string> ssrcLines;

        std::size_t current, previous = 0;
        current = answer.find("a=ssrc:");
        while (current != std::string::npos) {
            const auto pos = answer.find_first_of(' ', current);
            const auto& ssrc = answer.substr(current, pos - current);
            ssrcLines.insert(ssrc);
            previous = current + 1;
            current = answer.find("a=ssrc:", previous);
        }

        ASSERT_EQ(2, ssrcLines.size());

        closePeerConnection(pRtcPeerConnection);
        EXPECT_EQ(STATUS_SUCCESS, freePeerConnection(&pRtcPeerConnection));
    });
}

const auto sdpext = R"(v=0
o=- 4936640868317839377 2 IN IP4 127.0.0.1
s=-
t=0 0
a=group:BUNDLE 0 1
a=msid-semantic: WMS IorFnFnF3tBWrBDrWkOrWg3zowztGU0DXDCG
m=video 9 UDP/TLS/RTP/SAVPF 96 97 98 99 100 101 102 121 127 120 125 107 108 109 124 119 123 118 114 115 116
c=IN IP4 0.0.0.0
a=rtcp:9 IN IP4 0.0.0.0
a=ice-ufrag:SNHT
a=ice-pwd:aAU8vF42EO01/2WuVLJ+5kXU
a=ice-options:trickle
a=fingerprint:sha-256 07:A5:92:00:70:B3:51:ED:3E:F5:D4:D1:93:D4:3E:ED:69:5F:9E:81:03:6B:B2:AC:48:D7:35:E4:48:75:B5:91
a=setup:actpass
a=mid:0
a=extmap:1 urn:ietf:params:rtp-hdrext:toffset
a=extmap:2 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time
a=extmap:3 urn:3gpp:video-orientation
a=extmap:4 http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01
a=extmap:5 http://www.webrtc.org/experiments/rtp-hdrext/playout-delay
a=extmap:6 http://www.webrtc.org/experiments/rtp-hdrext/video-content-type
a=extmap:7 http://www.webrtc.org/experiments/rtp-hdrext/video-timing
a=extmap:8 http://www.webrtc.org/experiments/rtp-hdrext/color-space
a=extmap:9 urn:ietf:params:rtp-hdrext:sdes:mid
a=extmap:10 urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id
a=extmap:11 urn:ietf:params:rtp-hdrext:sdes:repaired-rtp-stream-id
)";

TEST_F(SdpApiTest, twccExtension)
{
    SessionDescription sd{};
    EXPECT_EQ(STATUS_SUCCESS, deserializeSessionDescription(&sd, const_cast<PCHAR>(sdpext)));
    uint32_t extid = 0;
    for (int i = 0; i < sd.mediaDescriptions[0].mediaAttributesCount; i++) {
        if (!strncmp("extmap", sd.mediaDescriptions[0].sdpAttributes[i].attributeName, 6)) {
            auto split = strchr(sd.mediaDescriptions[0].sdpAttributes[i].attributeValue, ' ');
            if (split != nullptr) {
                if (!strncmp(TWCC_EXT_URL, split + 1, strlen(TWCC_EXT_URL))) {
                    EXPECT_EQ(STATUS_SUCCESS, strtoui32(sd.mediaDescriptions[0].sdpAttributes[i].attributeValue, split, 10, &extid));
                }
            }
        }
    }
    EXPECT_EQ(4, extid);
}

TEST_F(SdpApiTest, populateSingleMediaSection_TestPayloadFmtp)
{
    CHAR remoteSessionDescription[] = R"(v=0
o=- 7732334361409071710 2 IN IP4 127.0.0.1
s=-
t=0 0
a=group:BUNDLE 0
a=msid-semantic: WMS
m=video 16485 UDP/TLS/RTP/SAVPF 96 102 125
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
a=fmtp:102 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42e01f
a=rtpmap:125 H264/90000
a=fmtp:125 level-asymmetry-allowed=1;packetization-mode=0;profile-level-id=42e01f
)";

    assertLFAndCRLF(remoteSessionDescription, ARRAY_SIZE(remoteSessionDescription) - 1, [](PCHAR sdp) {
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

        STRCPY(rtcSessionDescriptionInit.sdp, (PCHAR) sdp);
        rtcSessionDescriptionInit.type = SDP_TYPE_OFFER;
        EXPECT_EQ(setRemoteDescription(pRtcPeerConnection, &rtcSessionDescriptionInit), STATUS_SUCCESS);
        EXPECT_EQ(createAnswer(pRtcPeerConnection, &rtcSessionDescriptionInit), STATUS_SUCCESS);
        EXPECT_PRED_FORMAT2(testing::IsNotSubstring, "fmtp:125 level-asymmetry-allowed=1;packetization-mode=0;profile-level-id=42e01f",
                            rtcSessionDescriptionInit.sdp);
        EXPECT_PRED_FORMAT2(testing::IsSubstring, "fmtp:102 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42e01f",
                            rtcSessionDescriptionInit.sdp);
        closePeerConnection(pRtcPeerConnection);
        freePeerConnection(&pRtcPeerConnection);
    });
}

TEST_F(SdpApiTest, getH264FmtpScore)
{
    // Lambda to work around non-const nature of the API.
    auto getScore = [](const CHAR* fmtp) { return getH264FmtpScore(const_cast<PCHAR>(fmtp)); };
    // Test perfect matches.
    EXPECT_EQ(3, getScore("level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42e01f"));
    EXPECT_EQ(3, getScore("profile-level-id=42e01f;packetization-mode=1;level-asymmetry-allowed=1"));
    EXPECT_EQ(3, getScore("packetization-mode=1;profile-level-id=42e01f;level-asymmetry-allowed=1"));

    // Case shouldn't matter in profile level parsing (42e01f->42E01F).
    EXPECT_EQ(3, getScore("level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42E01F"));

    // Asymmetry not allowed, but we found a profile match.
    EXPECT_EQ(2, getScore("level-asymmetry-allowed=0;packetization-mode=1;profile-level-id=42e01f"));
    EXPECT_EQ(2, getScore("packetization-mode=1;profile-level-id=42e01f"));

    // Non-preferred profile-level-id, but asymmetry is allowed.
    EXPECT_EQ(2, getScore("level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=640032"));

    // Packetization mode not allowed, but asymmetry and profile are matched.
    EXPECT_EQ(2, getScore("level-asymmetry-allowed=1;packetization-mode=0;profile-level-id=42e01f"));
    EXPECT_EQ(2, getScore("level-asymmetry-allowed=1;profile-level-id=42e01f"));

    // Profile is not matched and asymmetry is not allowed, but packetization mode is matched.
    EXPECT_EQ(1, getScore("level-asymmetry-allowed=0;packetization-mode=1;profile-level-id=640032"));
    EXPECT_EQ(1, getScore("packetization-mode=1;profile-level-id=640032"));
}

TEST_F(SdpApiTest, populateSingleMediaSection_TestMultipleIceOptions)
{
    CHAR remoteSessionDescription[] = R"(v=0
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
a=ice-options:rtp+ecn trickle renomination
a=fingerprint:sha-256 51:04:F9:20:45:5C:9D:85:AF:D7:AF:FB:2B:F8:DB:24:66:7B:6A:E3:E3:EF:EC:72:93:6E:01:B8:C9:53:A6:31
a=setup:actpass
a=mid:1
a=recvonly
a=rtcp-mux
a=rtcp-rsize
a=rtpmap:96 VP8/90000
a=rtpmap:102 H264/90000
)";

    assertLFAndCRLF(remoteSessionDescription, ARRAY_SIZE(remoteSessionDescription) - 1, [](PCHAR sdp) {
        PRtcPeerConnection pRtcPeerConnection = NULL;
        RtcConfiguration rtcConfiguration;
        RtcMediaStreamTrack rtcMediaStreamTrack;
        RtcSessionDescriptionInit rtcSessionDescriptionInit;

        MEMSET(&rtcConfiguration, 0x00, SIZEOF(RtcConfiguration));
        MEMSET(&rtcMediaStreamTrack, 0x00, SIZEOF(RtcMediaStreamTrack));
        MEMSET(&rtcSessionDescriptionInit, 0x00, SIZEOF(RtcSessionDescriptionInit));

        EXPECT_EQ(createPeerConnection(&rtcConfiguration, &pRtcPeerConnection), STATUS_SUCCESS);
        EXPECT_EQ(addSupportedCodec(pRtcPeerConnection, RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE), STATUS_SUCCESS);

        STRCPY(rtcSessionDescriptionInit.sdp, (PCHAR) sdp);
        rtcSessionDescriptionInit.type = SDP_TYPE_OFFER;
        EXPECT_EQ(setRemoteDescription(pRtcPeerConnection, &rtcSessionDescriptionInit), STATUS_SUCCESS);
        EXPECT_EQ(TRUE, canTrickleIceCandidates(pRtcPeerConnection).value);

        closePeerConnection(pRtcPeerConnection);
        freePeerConnection(&pRtcPeerConnection);
    });
}

TEST_P(SdpApiTest_SdpMatch, populateSingleMediaSection_TestH264Fmtp)
{
    PRtcPeerConnection pRtcPeerConnection = NULL;
    PRtcRtpTransceiver transceiver1 = NULL;
    RtcConfiguration rtcConfiguration;
    RtcMediaStreamTrack track1;
    RtcRtpTransceiverInit rtcRtpTransceiverInit;
    RtcSessionDescriptionInit rtcSessionDescriptionInit;

    MEMSET(&rtcConfiguration, 0x00, SIZEOF(RtcConfiguration));
    MEMSET(&track1, 0x00, SIZEOF(RtcMediaStreamTrack));
    MEMSET(&rtcSessionDescriptionInit, 0x00, SIZEOF(RtcSessionDescriptionInit));

    EXPECT_EQ(createPeerConnection(&rtcConfiguration, &pRtcPeerConnection), STATUS_SUCCESS);
    EXPECT_EQ(addSupportedCodec(pRtcPeerConnection, RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE), STATUS_SUCCESS);

    rtcRtpTransceiverInit.direction = RTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY;
    track1.kind = MEDIA_STREAM_TRACK_KIND_VIDEO;
    track1.codec = RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE;
    STRCPY(track1.streamId, "myKvsVideoStream");
    STRCPY(track1.trackId, "myVideo");
    EXPECT_EQ(addTransceiver(pRtcPeerConnection, &track1, &rtcRtpTransceiverInit, &transceiver1), STATUS_SUCCESS);

    STRCPY(rtcSessionDescriptionInit.sdp, sdp());
    rtcSessionDescriptionInit.type = SDP_TYPE_OFFER;
    EXPECT_EQ(setRemoteDescription(pRtcPeerConnection, &rtcSessionDescriptionInit), STATUS_SUCCESS);
    EXPECT_EQ(createAnswer(pRtcPeerConnection, &rtcSessionDescriptionInit), STATUS_SUCCESS);

    EXPECT_PRED_FORMAT2(testing::IsSubstring, match(), rtcSessionDescriptionInit.sdp) << "Offer:\n"
                                                                                      << sdp() << "\nAnswer:\n"
                                                                                      << rtcSessionDescriptionInit.sdp;
    closePeerConnection(pRtcPeerConnection);
    freePeerConnection(&pRtcPeerConnection);
}

SdpMatch offer_1v1a1d_Chrome_Android = SdpMatch{
    R"(v=0
o=- 2383219383355379692 2 IN IP4 127.0.0.1
s=-
t=0 0
a=group:BUNDLE 0 1 2
a=msid-semantic: WMS 3CXV4snScv28Bl5Ltn7V4StSDzTGKOnaaAdf
m=audio 9 UDP/TLS/RTP/SAVPF 111 103 9 0 8 105 13 110 113 126
c=IN IP4 0.0.0.0
a=rtcp:9 IN IP4 0.0.0.0
a=ice-ufrag:Wafl
a=ice-pwd:IS933a+6pookL48tYXASXnwW
a=ice-options:trickle
a=fingerprint:sha-256 49:3C:77:6E:6B:2B:90:69:00:AE:47:4A:87:A7:F4:F6:F0:B3:6D:D6:FA:7F:84:DD:A5:6E:6E:2E:1F:64:D7:7F
a=setup:actpass
a=mid:0
a=extmap:1 urn:ietf:params:rtp-hdrext:ssrc-audio-level
a=extmap:2 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time
a=extmap:3 http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01
a=extmap:4 urn:ietf:params:rtp-hdrext:sdes:mid
a=extmap:5 urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id
a=extmap:6 urn:ietf:params:rtp-hdrext:sdes:repaired-rtp-stream-id
a=sendrecv
a=msid:3CXV4snScv28Bl5Ltn7V4StSDzTGKOnaaAdf f94c5ff6-26b9-4315-815d-40b4dd2efdef
a=rtcp-mux
a=rtpmap:111 opus/48000/2
a=rtcp-fb:111 transport-cc
a=fmtp:111 minptime=10;useinbandfec=1
a=rtpmap:103 ISAC/16000
a=rtpmap:9 G722/8000
a=rtpmap:0 PCMU/8000
a=rtpmap:8 PCMA/8000
a=rtpmap:105 CN/16000
a=rtpmap:13 CN/8000
a=rtpmap:110 telephone-event/48000
a=rtpmap:113 telephone-event/16000
a=rtpmap:126 telephone-event/8000
a=ssrc:2261433415 cname:5L7rDchCDtUnZEok
a=ssrc:2261433415 msid:3CXV4snScv28Bl5Ltn7V4StSDzTGKOnaaAdf f94c5ff6-26b9-4315-815d-40b4dd2efdef
a=ssrc:2261433415 mslabel:3CXV4snScv28Bl5Ltn7V4StSDzTGKOnaaAdf
a=ssrc:2261433415 label:f94c5ff6-26b9-4315-815d-40b4dd2efdef
m=video 9 UDP/TLS/RTP/SAVPF 96 97 98 99 127 125 104 122 106 107 108 109 124 121 100 101 102
c=IN IP4 0.0.0.0
a=rtcp:9 IN IP4 0.0.0.0
a=ice-ufrag:Wafl
a=ice-pwd:IS933a+6pookL48tYXASXnwW
a=ice-options:trickle
a=fingerprint:sha-256 49:3C:77:6E:6B:2B:90:69:00:AE:47:4A:87:A7:F4:F6:F0:B3:6D:D6:FA:7F:84:DD:A5:6E:6E:2E:1F:64:D7:7F
a=setup:actpass
a=mid:1
a=extmap:14 urn:ietf:params:rtp-hdrext:toffset
a=extmap:2 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time
a=extmap:13 urn:3gpp:video-orientation
a=extmap:3 http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01
a=extmap:12 http://www.webrtc.org/experiments/rtp-hdrext/playout-delay
a=extmap:11 http://www.webrtc.org/experiments/rtp-hdrext/video-content-type
a=extmap:7 http://www.webrtc.org/experiments/rtp-hdrext/video-timing
a=extmap:8 http://www.webrtc.org/experiments/rtp-hdrext/color-space
a=extmap:4 urn:ietf:params:rtp-hdrext:sdes:mid
a=extmap:5 urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id
a=extmap:6 urn:ietf:params:rtp-hdrext:sdes:repaired-rtp-stream-id
a=recvonly
a=rtcp-mux
a=rtcp-rsize
a=rtpmap:96 VP8/90000
a=rtcp-fb:96 goog-remb
a=rtcp-fb:96 transport-cc
a=rtcp-fb:96 ccm fir
a=rtcp-fb:96 nack
a=rtcp-fb:96 nack pli
a=rtpmap:97 rtx/90000
a=fmtp:97 apt=96
a=rtpmap:98 VP9/90000
a=rtcp-fb:98 goog-remb
a=rtcp-fb:98 transport-cc
a=rtcp-fb:98 ccm fir
a=rtcp-fb:98 nack
a=rtcp-fb:98 nack pli
a=fmtp:98 profile-id=0
a=rtpmap:99 rtx/90000
a=fmtp:99 apt=98
a=rtpmap:127 VP9/90000
a=rtcp-fb:127 goog-remb
a=rtcp-fb:127 transport-cc
a=rtcp-fb:127 ccm fir
a=rtcp-fb:127 nack
a=rtcp-fb:127 nack pli
a=fmtp:127 profile-id=1
a=rtpmap:125 VP9/90000
a=rtcp-fb:125 goog-remb
a=rtcp-fb:125 transport-cc
a=rtcp-fb:125 ccm fir
a=rtcp-fb:125 nack
a=rtcp-fb:125 nack pli
a=fmtp:125 profile-id=2
a=rtpmap:104 H264/90000
a=rtcp-fb:104 goog-remb
a=rtcp-fb:104 transport-cc
a=rtcp-fb:104 ccm fir
a=rtcp-fb:104 nack
a=rtcp-fb:104 nack pli
a=fmtp:104 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=420015
a=rtpmap:122 rtx/90000
a=fmtp:122 apt=104
a=rtpmap:106 H264/90000
a=rtcp-fb:106 goog-remb
a=rtcp-fb:106 transport-cc
a=rtcp-fb:106 ccm fir
a=rtcp-fb:106 nack
a=rtcp-fb:106 nack pli
a=fmtp:106 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=4d0015
a=rtpmap:107 rtx/90000
a=fmtp:107 apt=106
a=rtpmap:108 H264/90000
a=rtcp-fb:108 goog-remb
a=rtcp-fb:108 transport-cc
a=rtcp-fb:108 ccm fir
a=rtcp-fb:108 nack
a=rtcp-fb:108 nack pli
a=fmtp:108 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=640015
a=rtpmap:109 rtx/90000
a=fmtp:109 apt=108
a=rtpmap:124 H264/90000
a=rtcp-fb:124 goog-remb
a=rtcp-fb:124 transport-cc
a=rtcp-fb:124 ccm fir
a=rtcp-fb:124 nack
a=rtcp-fb:124 nack pli
a=fmtp:124 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42e015
a=rtpmap:121 rtx/90000
a=fmtp:121 apt=124
a=rtpmap:100 red/90000
a=rtpmap:101 rtx/90000
a=fmtp:101 apt=100
a=rtpmap:102 ulpfec/90000
m=application 9 UDP/DTLS/SCTP webrtc-datachannel
c=IN IP4 0.0.0.0
a=ice-ufrag:Wafl
a=ice-pwd:IS933a+6pookL48tYXASXnwW
a=ice-options:trickle
a=fingerprint:sha-256 49:3C:77:6E:6B:2B:90:69:00:AE:47:4A:87:A7:F4:F6:F0:B3:6D:D6:FA:7F:84:DD:A5:6E:6E:2E:1F:64:D7:7F
a=setup:actpass
a=mid:2
a=sctp-port:5000
a=max-message-size:262144
)",
    "level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42e015",
};

SdpMatch offer_1v1a1d_Chrome_Linux = SdpMatch{
    R"(v=0
o=- 7793366052006315995 2 IN IP4 127.0.0.1
s=-
t=0 0
a=group:BUNDLE 0 1 2
a=msid-semantic: WMS
m=video 9 UDP/TLS/RTP/SAVPF 96 97 98 99 100 101 122 102 121 127 120 125 107 108 109 124 119 123
c=IN IP4 0.0.0.0
a=rtcp:9 IN IP4 0.0.0.0
a=ice-ufrag:IGIv
a=ice-pwd:1YOD4CwjPUofTaiAWtWObQoE
a=ice-options:trickle
a=fingerprint:sha-256 E4:01:CC:27:A3:DA:CB:4E:87:46:85:CE:C4:06:1F:B9:83:85:F2:FB:29:11:81:09:16:AE:21:3D:13:A4:55:C5
a=setup:actpass
a=mid:0
a=extmap:1 urn:ietf:params:rtp-hdrext:toffset
a=extmap:2 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time
a=extmap:3 urn:3gpp:video-orientation
a=extmap:4 http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01
a=extmap:5 http://www.webrtc.org/experiments/rtp-hdrext/playout-delay
a=extmap:6 http://www.webrtc.org/experiments/rtp-hdrext/video-content-type
a=extmap:7 http://www.webrtc.org/experiments/rtp-hdrext/video-timing
a=extmap:8 http://www.webrtc.org/experiments/rtp-hdrext/color-space
a=extmap:9 urn:ietf:params:rtp-hdrext:sdes:mid
a=extmap:10 urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id
a=extmap:11 urn:ietf:params:rtp-hdrext:sdes:repaired-rtp-stream-id
a=recvonly
a=rtcp-mux
a=rtcp-rsize
a=rtpmap:96 VP8/90000
a=rtcp-fb:96 goog-remb
a=rtcp-fb:96 transport-cc
a=rtcp-fb:96 ccm fir
a=rtcp-fb:96 nack
a=rtcp-fb:96 nack pli
a=rtpmap:97 rtx/90000
a=fmtp:97 apt=96
a=rtpmap:98 VP9/90000
a=rtcp-fb:98 goog-remb
a=rtcp-fb:98 transport-cc
a=rtcp-fb:98 ccm fir
a=rtcp-fb:98 nack
a=rtcp-fb:98 nack pli
a=fmtp:98 profile-id=0
a=rtpmap:99 rtx/90000
a=fmtp:99 apt=98
a=rtpmap:100 VP9/90000
a=rtcp-fb:100 goog-remb
a=rtcp-fb:100 transport-cc
a=rtcp-fb:100 ccm fir
a=rtcp-fb:100 nack
a=rtcp-fb:100 nack pli
a=fmtp:100 profile-id=2
a=rtpmap:101 rtx/90000
a=fmtp:101 apt=100
a=rtpmap:122 VP9/90000
a=rtcp-fb:122 goog-remb
a=rtcp-fb:122 transport-cc
a=rtcp-fb:122 ccm fir
a=rtcp-fb:122 nack
a=rtcp-fb:122 nack pli
a=fmtp:122 profile-id=1
a=rtpmap:102 H264/90000
a=rtcp-fb:102 goog-remb
a=rtcp-fb:102 transport-cc
a=rtcp-fb:102 ccm fir
a=rtcp-fb:102 nack
a=rtcp-fb:102 nack pli
a=fmtp:102 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42001f
a=rtpmap:121 rtx/90000
a=fmtp:121 apt=102
a=rtpmap:127 H264/90000
a=rtcp-fb:127 goog-remb
a=rtcp-fb:127 transport-cc
a=rtcp-fb:127 ccm fir
a=rtcp-fb:127 nack
a=rtcp-fb:127 nack pli
a=fmtp:127 level-asymmetry-allowed=1;packetization-mode=0;profile-level-id=42001f
a=rtpmap:120 rtx/90000
a=fmtp:120 apt=127
a=rtpmap:125 H264/90000
a=rtcp-fb:125 goog-remb
a=rtcp-fb:125 transport-cc
a=rtcp-fb:125 ccm fir
a=rtcp-fb:125 nack
a=rtcp-fb:125 nack pli
a=fmtp:125 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42e01f
a=rtpmap:107 rtx/90000
a=fmtp:107 apt=125
a=rtpmap:108 H264/90000
a=rtcp-fb:108 goog-remb
a=rtcp-fb:108 transport-cc
a=rtcp-fb:108 ccm fir
a=rtcp-fb:108 nack
a=rtcp-fb:108 nack pli
a=fmtp:108 level-asymmetry-allowed=1;packetization-mode=0;profile-level-id=42e01f
a=rtpmap:109 rtx/90000
a=fmtp:109 apt=108
a=rtpmap:124 red/90000
a=rtpmap:119 rtx/90000
a=fmtp:119 apt=124
a=rtpmap:123 ulpfec/90000
m=audio 9 UDP/TLS/RTP/SAVPF 111 103 104 9 0 8 106 105 13 110 112 113 126
c=IN IP4 0.0.0.0
a=rtcp:9 IN IP4 0.0.0.0
a=ice-ufrag:IGIv
a=ice-pwd:1YOD4CwjPUofTaiAWtWObQoE
a=ice-options:trickle
a=fingerprint:sha-256 E4:01:CC:27:A3:DA:CB:4E:87:46:85:CE:C4:06:1F:B9:83:85:F2:FB:29:11:81:09:16:AE:21:3D:13:A4:55:C5
a=setup:actpass
a=mid:1
a=extmap:14 urn:ietf:params:rtp-hdrext:ssrc-audio-level
a=extmap:2 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time
a=extmap:4 http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01
a=extmap:9 urn:ietf:params:rtp-hdrext:sdes:mid
a=extmap:10 urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id
a=extmap:11 urn:ietf:params:rtp-hdrext:sdes:repaired-rtp-stream-id
a=recvonly
a=rtcp-mux
a=rtpmap:111 opus/48000/2
a=rtcp-fb:111 transport-cc
a=fmtp:111 minptime=10;useinbandfec=1
a=rtpmap:103 ISAC/16000
a=rtpmap:104 ISAC/32000
a=rtpmap:9 G722/8000
a=rtpmap:0 PCMU/8000
a=rtpmap:8 PCMA/8000
a=rtpmap:106 CN/32000
a=rtpmap:105 CN/16000
a=rtpmap:13 CN/8000
a=rtpmap:110 telephone-event/48000
a=rtpmap:112 telephone-event/32000
a=rtpmap:113 telephone-event/16000
a=rtpmap:126 telephone-event/8000
m=application 9 UDP/DTLS/SCTP webrtc-datachannel
c=IN IP4 0.0.0.0
a=ice-ufrag:IGIv
a=ice-pwd:1YOD4CwjPUofTaiAWtWObQoE
a=ice-options:trickle
a=fingerprint:sha-256 E4:01:CC:27:A3:DA:CB:4E:87:46:85:CE:C4:06:1F:B9:83:85:F2:FB:29:11:81:09:16:AE:21:3D:13:A4:55:C5
a=setup:actpass
a=mid:2
a=sctp-port:5000
a=max-message-size:262144
)",
    "level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42e01f",
};

SdpMatch offer_1v1a1d_Chrome_Mac = SdpMatch{
    R"(v=0
o=- 5885812016638194651 2 IN IP4 127.0.0.1
s=-
t=0 0
a=group:BUNDLE 0 1 2 3 4 5
a=msid-semantic: WMS
m=video 9 UDP/TLS/RTP/SAVPF 96 97 98 99 100 101 122 102 121 127 120 125 107 108 109 124 119 123 118 114 115 116
c=IN IP4 0.0.0.0
a=rtcp:9 IN IP4 0.0.0.0
a=ice-ufrag:oA2n
a=ice-pwd:JS5O7Y45gSl3kkfRlmEimf2P
a=ice-options:trickle
a=fingerprint:sha-256 F3:0F:71:45:88:45:8E:81:3B:9E:1C:FB:5C:45:76:28:24:12:7D:17:36:BD:BA:50:63:DF:94:53:53:F9:3D:1E
a=setup:actpass
a=mid:0
a=extmap:1 urn:ietf:params:rtp-hdrext:toffset
a=extmap:2 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time
a=extmap:3 urn:3gpp:video-orientation
a=extmap:4 http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01
a=extmap:5 http://www.webrtc.org/experiments/rtp-hdrext/playout-delay
a=extmap:6 http://www.webrtc.org/experiments/rtp-hdrext/video-content-type
a=extmap:7 http://www.webrtc.org/experiments/rtp-hdrext/video-timing
a=extmap:8 http://www.webrtc.org/experiments/rtp-hdrext/color-space
a=extmap:9 urn:ietf:params:rtp-hdrext:sdes:mid
a=extmap:10 urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id
a=extmap:11 urn:ietf:params:rtp-hdrext:sdes:repaired-rtp-stream-id
a=recvonly
a=rtcp-mux
a=rtcp-rsize
a=rtpmap:96 VP8/90000
a=rtcp-fb:96 goog-remb
a=rtcp-fb:96 transport-cc
a=rtcp-fb:96 ccm fir
a=rtcp-fb:96 nack
a=rtcp-fb:96 nack pli
a=rtpmap:97 rtx/90000
a=fmtp:97 apt=96
a=rtpmap:98 VP9/90000
a=rtcp-fb:98 goog-remb
a=rtcp-fb:98 transport-cc
a=rtcp-fb:98 ccm fir
a=rtcp-fb:98 nack
a=rtcp-fb:98 nack pli
a=fmtp:98 profile-id=0
a=rtpmap:99 rtx/90000
a=fmtp:99 apt=98
a=rtpmap:100 VP9/90000
a=rtcp-fb:100 goog-remb
a=rtcp-fb:100 transport-cc
a=rtcp-fb:100 ccm fir
a=rtcp-fb:100 nack
a=rtcp-fb:100 nack pli
a=fmtp:100 profile-id=2
a=rtpmap:101 rtx/90000
a=fmtp:101 apt=100
a=rtpmap:122 VP9/90000
a=rtcp-fb:122 goog-remb
a=rtcp-fb:122 transport-cc
a=rtcp-fb:122 ccm fir
a=rtcp-fb:122 nack
a=rtcp-fb:122 nack pli
a=fmtp:122 profile-id=1
a=rtpmap:102 H264/90000
a=rtcp-fb:102 goog-remb
a=rtcp-fb:102 transport-cc
a=rtcp-fb:102 ccm fir
a=rtcp-fb:102 nack
a=rtcp-fb:102 nack pli
a=fmtp:102 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42001f
a=rtpmap:121 rtx/90000
a=fmtp:121 apt=102
a=rtpmap:127 H264/90000
a=rtcp-fb:127 goog-remb
a=rtcp-fb:127 transport-cc
a=rtcp-fb:127 ccm fir
a=rtcp-fb:127 nack
a=rtcp-fb:127 nack pli
a=fmtp:127 level-asymmetry-allowed=1;packetization-mode=0;profile-level-id=42001f
a=rtpmap:120 rtx/90000
a=fmtp:120 apt=127
a=rtpmap:125 H264/90000
a=rtcp-fb:125 goog-remb
a=rtcp-fb:125 transport-cc
a=rtcp-fb:125 ccm fir
a=rtcp-fb:125 nack
a=rtcp-fb:125 nack pli
a=fmtp:125 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42e01f
a=rtpmap:107 rtx/90000
a=fmtp:107 apt=125
a=rtpmap:108 H264/90000
a=rtcp-fb:108 goog-remb
a=rtcp-fb:108 transport-cc
a=rtcp-fb:108 ccm fir
a=rtcp-fb:108 nack
a=rtcp-fb:108 nack pli
a=fmtp:108 level-asymmetry-allowed=1;packetization-mode=0;profile-level-id=42e01f
a=rtpmap:109 rtx/90000
a=fmtp:109 apt=108
a=rtpmap:124 H264/90000
a=rtcp-fb:124 goog-remb
a=rtcp-fb:124 transport-cc
a=rtcp-fb:124 ccm fir
a=rtcp-fb:124 nack
a=rtcp-fb:124 nack pli
a=fmtp:124 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=4d0032
a=rtpmap:119 rtx/90000
a=fmtp:119 apt=124
a=rtpmap:123 H264/90000
a=rtcp-fb:123 goog-remb
a=rtcp-fb:123 transport-cc
a=rtcp-fb:123 ccm fir
a=rtcp-fb:123 nack
a=rtcp-fb:123 nack pli
a=fmtp:123 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=640032
a=rtpmap:118 rtx/90000
a=fmtp:118 apt=123
a=rtpmap:114 red/90000
a=rtpmap:115 rtx/90000
a=fmtp:115 apt=114
a=rtpmap:116 ulpfec/90000
m=audio 9 UDP/TLS/RTP/SAVPF 111 103 104 9 0 8 106 105 13 110 112 113 126
c=IN IP4 0.0.0.0
a=rtcp:9 IN IP4 0.0.0.0
a=ice-ufrag:oA2n
a=ice-pwd:JS5O7Y45gSl3kkfRlmEimf2P
a=ice-options:trickle
a=fingerprint:sha-256 F3:0F:71:45:88:45:8E:81:3B:9E:1C:FB:5C:45:76:28:24:12:7D:17:36:BD:BA:50:63:DF:94:53:53:F9:3D:1E
a=setup:actpass
a=mid:1
a=extmap:14 urn:ietf:params:rtp-hdrext:ssrc-audio-level
a=extmap:2 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time
a=extmap:4 http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01
a=extmap:9 urn:ietf:params:rtp-hdrext:sdes:mid
a=extmap:10 urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id
a=extmap:11 urn:ietf:params:rtp-hdrext:sdes:repaired-rtp-stream-id
a=recvonly
a=rtcp-mux
a=rtpmap:111 opus/48000/2
a=rtcp-fb:111 transport-cc
a=fmtp:111 minptime=10;useinbandfec=1
a=rtpmap:103 ISAC/16000
a=rtpmap:104 ISAC/32000
a=rtpmap:9 G722/8000
a=rtpmap:0 PCMU/8000
a=rtpmap:8 PCMA/8000
a=rtpmap:106 CN/32000
a=rtpmap:105 CN/16000
a=rtpmap:13 CN/8000
a=rtpmap:110 telephone-event/48000
a=rtpmap:112 telephone-event/32000
a=rtpmap:113 telephone-event/16000
a=rtpmap:126 telephone-event/8000
m=application 9 UDP/DTLS/SCTP webrtc-datachannel
c=IN IP4 0.0.0.0
a=ice-ufrag:oA2n
a=ice-pwd:JS5O7Y45gSl3kkfRlmEimf2P
a=ice-options:trickle
a=fingerprint:sha-256 F3:0F:71:45:88:45:8E:81:3B:9E:1C:FB:5C:45:76:28:24:12:7D:17:36:BD:BA:50:63:DF:94:53:53:F9:3D:1E
a=setup:actpass
a=mid:2
a=sctp-port:5000
a=max-message-size:262144
)",
    "level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42e01f",
};

SdpMatch offer_1v1a1d_Firefox_Linux = SdpMatch{
    R"(v=0
o=mozilla...THIS_IS_SDPARTA-82.0 5414958935891470877 0 IN IP4 0.0.0.0
s=-
t=0 0
a=sendrecv
a=fingerprint:sha-256 8A:93:03:27:9A:AB:64:BD:12:1D:3D:6B:63:4C:6E:B4:8D:9D:C5:8D:B8:7B:87:8C:6B:BF:DD:19:B1:6E:A7:E5
a=group:BUNDLE 0 1 2
a=ice-options:trickle
a=msid-semantic:WMS *
m=video 9 UDP/TLS/RTP/SAVPF 120 124 121 125 126 127 97 98
c=IN IP4 0.0.0.0
a=recvonly
a=extmap:3 urn:ietf:params:rtp-hdrext:sdes:mid
a=extmap:4 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time
a=extmap:5 urn:ietf:params:rtp-hdrext:toffset
a=extmap:6/recvonly http://www.webrtc.org/experiments/rtp-hdrext/playout-delay
a=extmap:7 http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01
a=fmtp:126 profile-level-id=42e01f;level-asymmetry-allowed=1;packetization-mode=1
a=fmtp:97 profile-level-id=42e01f;level-asymmetry-allowed=1
a=fmtp:120 max-fs=12288;max-fr=60
a=fmtp:124 apt=120
a=fmtp:121 max-fs=12288;max-fr=60
a=fmtp:125 apt=121
a=fmtp:127 apt=126
a=fmtp:98 apt=97
a=ice-pwd:db2619f637ea75cf7e578e8fc7829ebf
a=ice-ufrag:6a957b4a
a=mid:0
a=rtcp-fb:120 nack
a=rtcp-fb:120 nack pli
a=rtcp-fb:120 ccm fir
a=rtcp-fb:120 goog-remb
a=rtcp-fb:120 transport-cc
a=rtcp-fb:121 nack
a=rtcp-fb:121 nack pli
a=rtcp-fb:121 ccm fir
a=rtcp-fb:121 goog-remb
a=rtcp-fb:121 transport-cc
a=rtcp-fb:126 nack
a=rtcp-fb:126 nack pli
a=rtcp-fb:126 ccm fir
a=rtcp-fb:126 goog-remb
a=rtcp-fb:126 transport-cc
a=rtcp-fb:97 nack
a=rtcp-fb:97 nack pli
a=rtcp-fb:97 ccm fir
a=rtcp-fb:97 goog-remb
a=rtcp-fb:97 transport-cc
a=rtcp-mux
a=rtcp-rsize
a=rtpmap:120 VP8/90000
a=rtpmap:124 rtx/90000
a=rtpmap:121 VP9/90000
a=rtpmap:125 rtx/90000
a=rtpmap:126 H264/90000
a=rtpmap:127 rtx/90000
a=rtpmap:97 H264/90000
a=rtpmap:98 rtx/90000
a=setup:actpass
a=ssrc:3469574630 cname:{080eadab-2d5f-4ba3-abc2-0ccc305a028d}
m=audio 9 UDP/TLS/RTP/SAVPF 109 9 0 8 101
c=IN IP4 0.0.0.0
a=recvonly
a=extmap:1 urn:ietf:params:rtp-hdrext:ssrc-audio-level
a=extmap:2/recvonly urn:ietf:params:rtp-hdrext:csrc-audio-level
a=extmap:3 urn:ietf:params:rtp-hdrext:sdes:mid
a=fmtp:109 maxplaybackrate=48000;stereo=1;useinbandfec=1
a=fmtp:101 0-15
a=ice-pwd:db2619f637ea75cf7e578e8fc7829ebf
a=ice-ufrag:6a957b4a
a=mid:1
a=rtcp-mux
a=rtpmap:109 opus/48000/2
a=rtpmap:9 G722/8000/1
a=rtpmap:0 PCMU/8000
a=rtpmap:8 PCMA/8000
a=rtpmap:101 telephone-event/8000
a=setup:actpass
a=ssrc:1326673407 cname:{080eadab-2d5f-4ba3-abc2-0ccc305a028d}
m=application 9 UDP/DTLS/SCTP webrtc-datachannel
c=IN IP4 0.0.0.0
a=sendrecv
a=ice-pwd:db2619f637ea75cf7e578e8fc7829ebf
a=ice-ufrag:6a957b4a
a=mid:2
a=setup:actpass
a=sctp-port:5000
a=max-message-size:1073741823
)",
    "profile-level-id=42e01f;level-asymmetry-allowed=1;packetization-mode=1",
};

SdpMatch offer_1v1a1d_Firefox_Mac = SdpMatch{
    R"(v=0
o=mozilla...THIS_IS_SDPARTA-82.0 8497183495409687627 0 IN IP4 0.0.0.0
s=-
t=0 0
a=sendrecv
a=fingerprint:sha-256 4B:D9:D1:E1:37:BC:96:BB:E8:2E:35:0B:D7:21:F9:E8:9E:DF:6C:DB:64:7A:BF:5C:13:88:5B:28:4C:60:A7:45
a=group:BUNDLE 0 1 2
a=ice-options:trickle
a=msid-semantic:WMS *
m=video 9 UDP/TLS/RTP/SAVPF 120 124 121 125 126 127 97 98
c=IN IP4 0.0.0.0
a=recvonly
a=extmap:3 urn:ietf:params:rtp-hdrext:sdes:mid
a=extmap:4 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time
a=extmap:5 urn:ietf:params:rtp-hdrext:toffset
a=extmap:6/recvonly http://www.webrtc.org/experiments/rtp-hdrext/playout-delay
a=extmap:7 http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01
a=fmtp:126 profile-level-id=42e01f;level-asymmetry-allowed=1;packetization-mode=1
a=fmtp:97 profile-level-id=42e01f;level-asymmetry-allowed=1
a=fmtp:120 max-fs=12288;max-fr=60
a=fmtp:124 apt=120
a=fmtp:121 max-fs=12288;max-fr=60
a=fmtp:125 apt=121
a=fmtp:127 apt=126
a=fmtp:98 apt=97
a=ice-pwd:e2f571f28334aa91043ed38190223da9
a=ice-ufrag:9dfb71ef
a=mid:0
a=rtcp-fb:120 nack
a=rtcp-fb:120 nack pli
a=rtcp-fb:120 ccm fir
a=rtcp-fb:120 goog-remb
a=rtcp-fb:120 transport-cc
a=rtcp-fb:121 nack
a=rtcp-fb:121 nack pli
a=rtcp-fb:121 ccm fir
a=rtcp-fb:121 goog-remb
a=rtcp-fb:121 transport-cc
a=rtcp-fb:126 nack
a=rtcp-fb:126 nack pli
a=rtcp-fb:126 ccm fir
a=rtcp-fb:126 goog-remb
a=rtcp-fb:126 transport-cc
a=rtcp-fb:97 nack
a=rtcp-fb:97 nack pli
a=rtcp-fb:97 ccm fir
a=rtcp-fb:97 goog-remb
a=rtcp-fb:97 transport-cc
a=rtcp-mux
a=rtcp-rsize
a=rtpmap:120 VP8/90000
a=rtpmap:124 rtx/90000
a=rtpmap:121 VP9/90000
a=rtpmap:125 rtx/90000
a=rtpmap:126 H264/90000
a=rtpmap:127 rtx/90000
a=rtpmap:97 H264/90000
a=rtpmap:98 rtx/90000
a=setup:actpass
a=ssrc:1644235696 cname:{36a6a74c-73a4-594b-9bb0-023b4d357280}
m=audio 9 UDP/TLS/RTP/SAVPF 109 9 0 8 101
c=IN IP4 0.0.0.0
a=recvonly
a=extmap:1 urn:ietf:params:rtp-hdrext:ssrc-audio-level
a=extmap:2/recvonly urn:ietf:params:rtp-hdrext:csrc-audio-level
a=extmap:3 urn:ietf:params:rtp-hdrext:sdes:mid
a=fmtp:109 maxplaybackrate=48000;stereo=1;useinbandfec=1
a=fmtp:101 0-15
a=ice-pwd:e2f571f28334aa91043ed38190223da9
a=ice-ufrag:9dfb71ef
a=mid:1
a=rtcp-mux
a=rtpmap:109 opus/48000/2
a=rtpmap:9 G722/8000/1
a=rtpmap:0 PCMU/8000
a=rtpmap:8 PCMA/8000
a=rtpmap:101 telephone-event/8000
a=setup:actpass
a=ssrc:277637612 cname:{36a6a74c-73a4-594b-9bb0-023b4d357280}
m=application 9 UDP/DTLS/SCTP webrtc-datachannel
c=IN IP4 0.0.0.0
a=sendrecv
a=ice-pwd:e2f571f28334aa91043ed38190223da9
a=ice-ufrag:9dfb71ef
a=mid:2
a=setup:actpass
a=sctp-port:5000
a=max-message-size:1073741823
)",
    "profile-level-id=42e01f;level-asymmetry-allowed=1;packetization-mode=1",
};

SdpMatch offer_1v1a1d_Chromium_Linux = SdpMatch{
    R"(v=0
o=- 4846147289032659091 2 IN IP4 127.0.0.1
s=-
t=0 0
a=group:BUNDLE 0 1 2
a=msid-semantic: WMS
m=video 9 UDP/TLS/RTP/SAVPF 96 97 98 99 100 101 122 102 121 127 120 125 107 108 109 124 119 123
c=IN IP4 0.0.0.0
a=rtcp:9 IN IP4 0.0.0.0
a=ice-ufrag:XvdX
a=ice-pwd:VbwqLiodQFWAt3YVdyK/HG04
a=ice-options:trickle
a=fingerprint:sha-256 F0:6C:41:02:7D:AC:E0:CA:3B:2A:F9:92:F9:13:86:67:DA:71:5B:4E:E4:83:80:C5:87:3F:3B:4D:41:F2:91:44
a=setup:actpass
a=mid:0
a=extmap:1 urn:ietf:params:rtp-hdrext:toffset
a=extmap:2 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time
a=extmap:3 urn:3gpp:video-orientation
a=extmap:4 http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01
a=extmap:5 http://www.webrtc.org/experiments/rtp-hdrext/playout-delay
a=extmap:6 http://www.webrtc.org/experiments/rtp-hdrext/video-content-type
a=extmap:7 http://www.webrtc.org/experiments/rtp-hdrext/video-timing
a=extmap:8 http://www.webrtc.org/experiments/rtp-hdrext/color-space
a=extmap:9 urn:ietf:params:rtp-hdrext:sdes:mid
a=extmap:10 urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id
a=extmap:11 urn:ietf:params:rtp-hdrext:sdes:repaired-rtp-stream-id
a=recvonly
a=rtcp-mux
a=rtcp-rsize
a=rtpmap:96 VP8/90000
a=rtcp-fb:96 goog-remb
a=rtcp-fb:96 transport-cc
a=rtcp-fb:96 ccm fir
a=rtcp-fb:96 nack
a=rtcp-fb:96 nack pli
a=rtpmap:97 rtx/90000
a=fmtp:97 apt=96
a=rtpmap:98 VP9/90000
a=rtcp-fb:98 goog-remb
a=rtcp-fb:98 transport-cc
a=rtcp-fb:98 ccm fir
a=rtcp-fb:98 nack
a=rtcp-fb:98 nack pli
a=fmtp:98 profile-id=0
a=rtpmap:99 rtx/90000
a=fmtp:99 apt=98
a=rtpmap:100 VP9/90000
a=rtcp-fb:100 goog-remb
a=rtcp-fb:100 transport-cc
a=rtcp-fb:100 ccm fir
a=rtcp-fb:100 nack
a=rtcp-fb:100 nack pli
a=fmtp:100 profile-id=2
a=rtpmap:101 rtx/90000
a=fmtp:101 apt=100
a=rtpmap:122 VP9/90000
a=rtcp-fb:122 goog-remb
a=rtcp-fb:122 transport-cc
a=rtcp-fb:122 ccm fir
a=rtcp-fb:122 nack
a=rtcp-fb:122 nack pli
a=fmtp:122 profile-id=1
a=rtpmap:102 H264/90000
a=rtcp-fb:102 goog-remb
a=rtcp-fb:102 transport-cc
a=rtcp-fb:102 ccm fir
a=rtcp-fb:102 nack
a=rtcp-fb:102 nack pli
a=fmtp:102 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42001f
a=rtpmap:121 rtx/90000
a=fmtp:121 apt=102
a=rtpmap:127 H264/90000
a=rtcp-fb:127 goog-remb
a=rtcp-fb:127 transport-cc
a=rtcp-fb:127 ccm fir
a=rtcp-fb:127 nack
a=rtcp-fb:127 nack pli
a=fmtp:127 level-asymmetry-allowed=1;packetization-mode=0;profile-level-id=42001f
a=rtpmap:120 rtx/90000
a=fmtp:120 apt=127
a=rtpmap:125 H264/90000
a=rtcp-fb:125 goog-remb
a=rtcp-fb:125 transport-cc
a=rtcp-fb:125 ccm fir
a=rtcp-fb:125 nack
a=rtcp-fb:125 nack pli
a=fmtp:125 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42e01f
a=rtpmap:107 rtx/90000
a=fmtp:107 apt=125
a=rtpmap:108 H264/90000
a=rtcp-fb:108 goog-remb
a=rtcp-fb:108 transport-cc
a=rtcp-fb:108 ccm fir
a=rtcp-fb:108 nack
a=rtcp-fb:108 nack pli
a=fmtp:108 level-asymmetry-allowed=1;packetization-mode=0;profile-level-id=42e01f
a=rtpmap:109 rtx/90000
a=fmtp:109 apt=108
a=rtpmap:124 red/90000
a=rtpmap:119 rtx/90000
a=fmtp:119 apt=124
a=rtpmap:123 ulpfec/90000
m=audio 9 UDP/TLS/RTP/SAVPF 111 103 104 9 0 8 106 105 13 110 112 113 126
c=IN IP4 0.0.0.0
a=rtcp:9 IN IP4 0.0.0.0
a=ice-ufrag:XvdX
a=ice-pwd:VbwqLiodQFWAt3YVdyK/HG04
a=ice-options:trickle
a=fingerprint:sha-256 F0:6C:41:02:7D:AC:E0:CA:3B:2A:F9:92:F9:13:86:67:DA:71:5B:4E:E4:83:80:C5:87:3F:3B:4D:41:F2:91:44
a=setup:actpass
a=mid:1
a=extmap:14 urn:ietf:params:rtp-hdrext:ssrc-audio-level
a=extmap:2 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time
a=extmap:4 http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01
a=extmap:9 urn:ietf:params:rtp-hdrext:sdes:mid
a=extmap:10 urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id
a=extmap:11 urn:ietf:params:rtp-hdrext:sdes:repaired-rtp-stream-id
a=recvonly
a=rtcp-mux
a=rtpmap:111 opus/48000/2
a=rtcp-fb:111 transport-cc
a=fmtp:111 minptime=10;useinbandfec=1
a=rtpmap:103 ISAC/16000
a=rtpmap:104 ISAC/32000
a=rtpmap:9 G722/8000
a=rtpmap:0 PCMU/8000
a=rtpmap:8 PCMA/8000
a=rtpmap:106 CN/32000
a=rtpmap:105 CN/16000
a=rtpmap:13 CN/8000
a=rtpmap:110 telephone-event/48000
a=rtpmap:112 telephone-event/32000
a=rtpmap:113 telephone-event/16000
a=rtpmap:126 telephone-event/8000
m=application 9 UDP/DTLS/SCTP webrtc-datachannel
c=IN IP4 0.0.0.0
a=ice-ufrag:XvdX
a=ice-pwd:VbwqLiodQFWAt3YVdyK/HG04
a=ice-options:trickle
a=fingerprint:sha-256 F0:6C:41:02:7D:AC:E0:CA:3B:2A:F9:92:F9:13:86:67:DA:71:5B:4E:E4:83:80:C5:87:3F:3B:4D:41:F2:91:44
a=setup:actpass
a=mid:2
a=sctp-port:5000
a=max-message-size:262144
)",
    "level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42e01f",
};

SdpMatch offer_1v1a1d_Safari_Mac = SdpMatch{
    R"(v=0
o=- 1787042925906504798 2 IN IP4 127.0.0.1
s=-
t=0 0
a=group:BUNDLE 0 1 2
a=msid-semantic: WMS
m=video 9 UDP/TLS/RTP/SAVPF 96 97 98 99 100 101 127 125 104
c=IN IP4 0.0.0.0
a=rtcp:9 IN IP4 0.0.0.0
a=ice-ufrag:tPim
a=ice-pwd:Jcd6kb0MxzrIFwHHgNzy2fkO
a=ice-options:trickle
a=fingerprint:sha-256 F8:18:88:48:62:2A:67:F5:37:77:25:E9:8D:D8:98:99:38:F5:0D:CC:D4:B7:B3:CD:47:CC:5F:F9:FE:C0:94:BA
a=setup:actpass
a=mid:0
a=extmap:14 urn:ietf:params:rtp-hdrext:toffset
a=extmap:2 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time
a=extmap:13 urn:3gpp:video-orientation
a=extmap:3 http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01
a=extmap:12 http://www.webrtc.org/experiments/rtp-hdrext/playout-delay
a=extmap:11 http://www.webrtc.org/experiments/rtp-hdrext/video-content-type
a=extmap:7 http://www.webrtc.org/experiments/rtp-hdrext/video-timing
a=extmap:8 http://tools.ietf.org/html/draft-ietf-avtext-framemarking-07
a=extmap:9 http://www.webrtc.org/experiments/rtp-hdrext/color-space
a=extmap:4 urn:ietf:params:rtp-hdrext:sdes:mid
a=extmap:5 urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id
a=extmap:6 urn:ietf:params:rtp-hdrext:sdes:repaired-rtp-stream-id
a=recvonly
a=rtcp-mux
a=rtcp-rsize
a=rtpmap:96 H264/90000
a=rtcp-fb:96 goog-remb
a=rtcp-fb:96 transport-cc
a=rtcp-fb:96 ccm fir
a=rtcp-fb:96 nack
a=rtcp-fb:96 nack pli
a=fmtp:96 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=640c1f
a=rtpmap:97 rtx/90000
a=fmtp:97 apt=96
a=rtpmap:98 H264/90000
a=rtcp-fb:98 goog-remb
a=rtcp-fb:98 transport-cc
a=rtcp-fb:98 ccm fir
a=rtcp-fb:98 nack
a=rtcp-fb:98 nack pli
a=fmtp:98 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42e01f
a=rtpmap:99 rtx/90000
a=fmtp:99 apt=98
a=rtpmap:100 VP8/90000
a=rtcp-fb:100 goog-remb
a=rtcp-fb:100 transport-cc
a=rtcp-fb:100 ccm fir
a=rtcp-fb:100 nack
a=rtcp-fb:100 nack pli
a=rtpmap:101 rtx/90000
a=fmtp:101 apt=100
a=rtpmap:127 red/90000
a=rtpmap:125 rtx/90000
a=fmtp:125 apt=127
a=rtpmap:104 ulpfec/90000
m=audio 9 UDP/TLS/RTP/SAVPF 111 103 9 102 0 8 105 13 110 113 126
c=IN IP4 0.0.0.0
a=rtcp:9 IN IP4 0.0.0.0
a=ice-ufrag:tPim
a=ice-pwd:Jcd6kb0MxzrIFwHHgNzy2fkO
a=ice-options:trickle
a=fingerprint:sha-256 F8:18:88:48:62:2A:67:F5:37:77:25:E9:8D:D8:98:99:38:F5:0D:CC:D4:B7:B3:CD:47:CC:5F:F9:FE:C0:94:BA
a=setup:actpass
a=mid:1
a=extmap:1 urn:ietf:params:rtp-hdrext:ssrc-audio-level
a=extmap:2 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time
a=extmap:3 http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01
a=extmap:4 urn:ietf:params:rtp-hdrext:sdes:mid
a=extmap:5 urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id
a=extmap:6 urn:ietf:params:rtp-hdrext:sdes:repaired-rtp-stream-id
a=sendrecv
a=msid:- 53a91694-a120-4a65-96be-f164d2695455
a=rtcp-mux
a=rtpmap:111 opus/48000/2
a=rtcp-fb:111 transport-cc
a=fmtp:111 minptime=10;useinbandfec=1
a=rtpmap:103 ISAC/16000
a=rtpmap:9 G722/8000
a=rtpmap:102 ILBC/8000
a=rtpmap:0 PCMU/8000
a=rtpmap:8 PCMA/8000
a=rtpmap:105 CN/16000
a=rtpmap:13 CN/8000
a=rtpmap:110 telephone-event/48000
a=rtpmap:113 telephone-event/16000
a=rtpmap:126 telephone-event/8000
a=ssrc:2805193976 cname:7QoBnlnTDjko/niB
a=ssrc:2805193976 msid:- 53a91694-a120-4a65-96be-f164d2695455
a=ssrc:2805193976 mslabel:-
a=ssrc:2805193976 label:53a91694-a120-4a65-96be-f164d2695455
m=application 9 UDP/DTLS/SCTP webrtc-datachannel
c=IN IP4 0.0.0.0
a=ice-ufrag:tPim
a=ice-pwd:Jcd6kb0MxzrIFwHHgNzy2fkO
a=ice-options:trickle
a=fingerprint:sha-256 F8:18:88:48:62:2A:67:F5:37:77:25:E9:8D:D8:98:99:38:F5:0D:CC:D4:B7:B3:CD:47:CC:5F:F9:FE:C0:94:BA
a=setup:actpass
a=mid:2
a=sctp-port:5000
a=max-message-size:262144
)",
    "level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42e01f",
};

// 1v1a1d represents 1 video + 1 audio + 1 data channel
INSTANTIATE_TEST_CASE_P(SdpApiTest_SdpMatch_Chrome, SdpApiTest_SdpMatch,
                        ::testing::Values(offer_1v1a1d_Chrome_Android, offer_1v1a1d_Chrome_Linux,
                                          offer_1v1a1d_Chrome_Mac), ); // the last comma is used to silent a warning

INSTANTIATE_TEST_CASE_P(SdpApiTest_SdpMatch_Firefox, SdpApiTest_SdpMatch, ::testing::Values(offer_1v1a1d_Firefox_Linux, offer_1v1a1d_Firefox_Mac), );

INSTANTIATE_TEST_CASE_P(SdpApiTest_SdpMatch_Chromium, SdpApiTest_SdpMatch, ::testing::Values(offer_1v1a1d_Chromium_Linux), );

INSTANTIATE_TEST_CASE_P(SdpApiTest_SdpMatch_Safari, SdpApiTest_SdpMatch, ::testing::Values(offer_1v1a1d_Safari_Mac), );

} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com
