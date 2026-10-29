#include "WebRTCClientTestFixture.h"

// getTurnServersFromOffer() and initializePeerConnection() live in samples/common/Common.c, which this test binary
// compiles in.
#include "../samples/common/Samples.h"

namespace com {
namespace amazonaws {
namespace kinesis {
namespace video {
namespace webrtcclient {

class ViewerSuppliedIceServersTest : public WebRtcClientTestBase {
  protected:
    RtcIceServer mIceServers[MAX_ICE_CONFIG_URI_COUNT];
    UINT32 mIceServerCount = 0;

    // A TURN REST API username, "<expiry in seconds since the epoch>:<id>", expiring the given number of seconds from now.
    static std::string userNameExpiringIn(INT64 seconds)
    {
        return std::to_string((INT64) (GETTIME() / HUNDREDS_OF_NANOS_IN_A_SECOND) + seconds) + ":AbCdEfGh";
    }

    // One GetIceServerConfig IceServerList entry, as the API returns it.
    static std::string iceServerSet(const std::string& uris, const std::string& userName, const std::string& password = "pass+word/=")
    {
        return R"({"Uris":[)" + uris + R"(],"Username":")" + userName + R"(","Password":")" + password + R"(","Ttl":300})";
    }

    // The offer payload the JS viewer sends: type and sdp first, IceServerList last.
    static std::string offerWith(const std::string& iceServerList)
    {
        return R"({"type":"offer","sdp":"v=0\r\no=- 1 2 IN IP4 127.0.0.1\r\ns=-\r\n","IceServerList":[)" + iceServerList + "]}";
    }

    static std::string threeKvsUris()
    {
        return R"("turn:18-237-1-2.t-0123abcd.kinesisvideo.us-west-2.amazonaws.com:443?transport=udp",)"
               R"("turns:18-237-1-2.t-0123abcd.kinesisvideo.us-west-2.amazonaws.com:443?transport=udp",)"
               R"("turns:18-237-1-2.t-0123abcd.kinesisvideo.us-west-2.amazonaws.com:443?transport=tcp")";
    }

    STATUS extract(const std::string& payload, UINT32 maxIceServers = MAX_ICE_CONFIG_URI_COUNT)
    {
        // Garbage, so the tests also show what is left behind when nothing is used.
        MEMSET(mIceServers, 'x', SIZEOF(mIceServers));
        mIceServerCount = MAX_UINT32;
        return getTurnServersFromOffer((PCHAR) payload.c_str(), (UINT32) payload.length(), mIceServers, maxIceServers, &mIceServerCount);
    }

    // Nothing usable was found: zero servers and nothing left in the array for the caller's own config to mix with.
    VOID expectNone()
    {
        RtcIceServer zeroed[MAX_ICE_CONFIG_URI_COUNT];
        MEMSET(zeroed, 0x00, SIZEOF(zeroed));
        EXPECT_EQ(0, mIceServerCount);
        EXPECT_EQ(0, MEMCMP(zeroed, mIceServers, SIZEOF(mIceServers)));
    }
};

TEST_F(ViewerSuppliedIceServersTest, jsViewerPayloadIsUsedAsIs)
{
    std::string userName = userNameExpiringIn(300);

    EXPECT_EQ(STATUS_SUCCESS, extract(offerWith(iceServerSet(threeKvsUris(), userName))));

    ASSERT_EQ(3, mIceServerCount);
    EXPECT_STREQ("turn:18-237-1-2.t-0123abcd.kinesisvideo.us-west-2.amazonaws.com:443?transport=udp", mIceServers[0].urls);
    EXPECT_STREQ("turns:18-237-1-2.t-0123abcd.kinesisvideo.us-west-2.amazonaws.com:443?transport=udp", mIceServers[1].urls);
    EXPECT_STREQ("turns:18-237-1-2.t-0123abcd.kinesisvideo.us-west-2.amazonaws.com:443?transport=tcp", mIceServers[2].urls);
    for (UINT32 i = 0; i < 3; i++) {
        EXPECT_STREQ(userName.c_str(), mIceServers[i].username);
        EXPECT_STREQ("pass+word/=", mIceServers[i].credential);
    }
}

TEST_F(ViewerSuppliedIceServersTest, offerWithoutIceServerListGivesNone)
{
    // What every viewer sends today.
    EXPECT_EQ(STATUS_SUCCESS, extract(R"({"type":"offer","sdp":"v=0\r\n"})"));
    expectNone();
}

TEST_F(ViewerSuppliedIceServersTest, propertyOrderDoesNotMatter)
{
    std::string set = iceServerSet(R"("turn:1.2.3.4:443?transport=udp")", userNameExpiringIn(300));

    EXPECT_EQ(STATUS_SUCCESS, extract(R"({"IceServerList":[)" + set + R"(],"type":"offer","sdp":"v=0\r\n"})"));
    EXPECT_EQ(1, mIceServerCount);

    // Keys within a set in another order.
    EXPECT_EQ(STATUS_SUCCESS,
              extract(offerWith(R"({"Ttl":300,"Password":"pw","Username":")" + userNameExpiringIn(300) + R"(","Uris":["turn:1.2.3.4:443"]})")));
    EXPECT_EQ(1, mIceServerCount);
    EXPECT_STREQ("pw", mIceServers[0].credential);
}

TEST_F(ViewerSuppliedIceServersTest, onlyTheFirstSetIsUsed)
{
    std::string first = iceServerSet(R"("turn:1.1.1.1:443?transport=udp")", userNameExpiringIn(300), "first");
    std::string second = iceServerSet(R"("turn:2.2.2.2:443?transport=udp","turn:2.2.2.2:443?transport=tcp")", userNameExpiringIn(300), "second");

    EXPECT_EQ(STATUS_SUCCESS, extract(offerWith(first + "," + second)));

    ASSERT_EQ(1, mIceServerCount);
    EXPECT_STREQ("turn:1.1.1.1:443?transport=udp", mIceServers[0].urls);
    EXPECT_STREQ("first", mIceServers[0].credential);
    // Untouched.
    EXPECT_EQ('x', mIceServers[1].urls[0]);
}

TEST_F(ViewerSuppliedIceServersTest, expiredOrExpiringCredentialsAreNotUsed)
{
    std::string uris = R"("turn:1.2.3.4:443?transport=udp")";
    INT64 graceSeconds = SAMPLE_VIEWER_ICE_SERVER_EXPIRY_GRACE_PERIOD / HUNDREDS_OF_NANOS_IN_A_SECOND;

    // The same grace period the signaling client refreshes its own ICE configuration with.
    EXPECT_EQ(ICE_CONFIGURATION_REFRESH_GRACE_PERIOD, SAMPLE_VIEWER_ICE_SERVER_EXPIRY_GRACE_PERIOD);

    EXPECT_EQ(STATUS_SUCCESS, extract(offerWith(iceServerSet(uris, userNameExpiringIn(-60)))));
    expectNone();

    EXPECT_EQ(STATUS_SUCCESS, extract(offerWith(iceServerSet(uris, userNameExpiringIn(graceSeconds - 5)))));
    expectNone();

    EXPECT_EQ(STATUS_SUCCESS, extract(offerWith(iceServerSet(uris, userNameExpiringIn(graceSeconds + 5)))));
    EXPECT_EQ(1, mIceServerCount);

    // A username without an expiry cannot be checked, so it is used.
    EXPECT_EQ(STATUS_SUCCESS, extract(offerWith(iceServerSet(uris, "plainUser"))));
    EXPECT_EQ(1, mIceServerCount);
}

TEST_F(ViewerSuppliedIceServersTest, unusableListsGiveNone)
{
    std::string userName = userNameExpiringIn(300);
    std::string uri = R"("turn:1.2.3.4:443")";

    for (const std::string& payload : {
             // A part of the set is missing or does not fit.
             offerWith(R"({"Uris":["turn:1.2.3.4:443"],"Password":"pw"})"),
             offerWith(R"({"Uris":["turn:1.2.3.4:443"],"Username":")" + userName + R"("})"),
             offerWith(R"({"Username":")" + userName + R"(","Password":"pw"})"),
             offerWith(iceServerSet("", userName)),
             offerWith(iceServerSet(uri, userName + std::string(MAX_ICE_CONFIG_USER_NAME_LEN, 'u'))),
             offerWith(iceServerSet(uri, userName, std::string(MAX_ICE_CONFIG_CREDENTIAL_LEN + 1, 'p'))),
             // The list itself is not a list of sets.
             std::string(R"({"type":"offer","sdp":"v=0\r\n","IceServerList":[]})"),
             std::string(R"({"type":"offer","sdp":"v=0\r\n","IceServerList":{}})"),
             std::string(R"({"type":"offer","sdp":"v=0\r\n","IceServerList":["turn:1.2.3.4:443"]})"),
             // Not a property of the offer itself.
             R"({"type":"offer","sdp":"v=0\r\n","extra":{"IceServerList":[)" + iceServerSet(uri, userName) + "]}}",
             // Not an offer that can be read.
             std::string(""),
             std::string("not json"),
         }) {
        EXPECT_EQ(STATUS_SUCCESS, extract(payload)) << payload;
        expectNone();
    }
}

TEST_F(ViewerSuppliedIceServersTest, urisThatDoNotFitAreSkipped)
{
    std::string longestUri = "turn:" + std::string(MAX_ICE_CONFIG_URI_LEN - STRLEN("turn:"), 'a');
    std::string uris = "\"" + longestUri + "a\",\"" + longestUri + R"(","turn:2.2.2.2:443","turn:3.3.3.3:443","turn:4.4.4.4:443","turn:5.5.5.5:443")";

    // Too long, and past the room given.
    EXPECT_EQ(STATUS_SUCCESS, extract(offerWith(iceServerSet(uris, userNameExpiringIn(300)))));
    ASSERT_EQ(MAX_ICE_CONFIG_URI_COUNT, mIceServerCount);
    EXPECT_STREQ(longestUri.c_str(), mIceServers[0].urls);
    EXPECT_STREQ("turn:4.4.4.4:443", mIceServers[MAX_ICE_CONFIG_URI_COUNT - 1].urls);
}

TEST_F(ViewerSuppliedIceServersTest, invalidArgs)
{
    std::string offer = offerWith(iceServerSet(R"("turn:1.2.3.4:443")", userNameExpiringIn(300)));

    EXPECT_EQ(STATUS_NULL_ARG, getTurnServersFromOffer(NULL, (UINT32) offer.length(), mIceServers, MAX_ICE_CONFIG_URI_COUNT, &mIceServerCount));
    EXPECT_EQ(STATUS_NULL_ARG,
              getTurnServersFromOffer((PCHAR) offer.c_str(), (UINT32) offer.length(), NULL, MAX_ICE_CONFIG_URI_COUNT, &mIceServerCount));
    EXPECT_EQ(STATUS_NULL_ARG, getTurnServersFromOffer((PCHAR) offer.c_str(), (UINT32) offer.length(), mIceServers, MAX_ICE_CONFIG_URI_COUNT, NULL));
}

// With the list after "type" and "sdp", the offer is still readable by every version of this library.
TEST_F(ViewerSuppliedIceServersTest, offerWithIceServerListIsStillAReadableOffer)
{
    RtcSessionDescriptionInit sessionDescriptionInit;
    std::string offer = offerWith(iceServerSet(threeKvsUris(), userNameExpiringIn(300)));

    EXPECT_EQ(STATUS_SUCCESS, deserializeSessionDescriptionInit((PCHAR) offer.c_str(), (UINT32) offer.length(), &sessionDescriptionInit));
    EXPECT_EQ(SDP_TYPE_OFFER, sessionDescriptionInit.type);
    EXPECT_STREQ("v=0\r\no=- 1 2 IN IP4 127.0.0.1\r\ns=-\r\n", sessionDescriptionInit.sdp);
}

class ViewerSuppliedIceServersPeerConnectionTest : public ViewerSuppliedIceServersTest {
  protected:
    SampleConfiguration mSampleConfiguration;
    PRtcPeerConnection mPeerConnection = NULL;

    VOID SetUp() override
    {
        ViewerSuppliedIceServersTest::SetUp();
        MEMSET(&mSampleConfiguration, 0x00, SIZEOF(mSampleConfiguration));
        mSampleConfiguration.channelInfo.pRegion = (PCHAR) TEST_DEFAULT_REGION;
        mSampleConfiguration.useTurn = TRUE;
        mSampleConfiguration.useViewerTurnServers = TRUE;
        // No signaling client: falling back to it fails, which shows when the fallback is taken.
        mSampleConfiguration.signalingClientHandle = INVALID_SIGNALING_CLIENT_HANDLE_VALUE;
        ASSERT_EQ(STATUS_SUCCESS, stackQueueCreate(&mSampleConfiguration.pregeneratedCertificates));
    }

    VOID TearDown() override
    {
        if (mPeerConnection != NULL) {
            EXPECT_EQ(STATUS_SUCCESS, freePeerConnection(&mPeerConnection));
        }
        stackQueueFree(mSampleConfiguration.pregeneratedCertificates);
        ViewerSuppliedIceServersTest::TearDown();
    }

    STATUS initializeWithOffer(const std::string& payload)
    {
        SignalingMessage offer;
        MEMSET(&offer, 0x00, SIZEOF(offer));
        offer.version = SIGNALING_MESSAGE_CURRENT_VERSION;
        offer.messageType = SIGNALING_MESSAGE_TYPE_OFFER;
        STRNCPY(offer.payload, payload.c_str(), MAX_SIGNALING_MESSAGE_LEN);
        offer.payloadLen = (UINT32) payload.length();
        return initializePeerConnection(&mSampleConfiguration, &offer, &mPeerConnection);
    }
};

TEST_F(ViewerSuppliedIceServersPeerConnectionTest, viewerSuppliedServersSkipTheSignalingClient)
{
    std::string uris = R"("turn:127.0.0.1:3478?transport=udp","turns:127.0.0.1:443?transport=udp","turns:127.0.0.1:443?transport=tcp")";
    std::string userName = userNameExpiringIn(300);
    PIceAgent pIceAgent;
    UINT32 i;

    EXPECT_EQ(STATUS_SUCCESS, initializeWithOffer(offerWith(iceServerSet(uris, userName))));
    ASSERT_NE((PRtcPeerConnection) NULL, mPeerConnection);
    // STUN plus the viewer's three.
    EXPECT_EQ(4, mSampleConfiguration.iceUriCount);

    // What the peer connection got: the STUN server first, then the viewer's servers with the viewer's credentials.
    pIceAgent = ((PKvsPeerConnection) mPeerConnection)->pIceAgent;
    ASSERT_EQ(4, pIceAgent->iceServersCount);
    EXPECT_FALSE(pIceAgent->iceServers[0].isTurn);
    for (i = 1; i < pIceAgent->iceServersCount; i++) {
        EXPECT_TRUE(pIceAgent->iceServers[i].isTurn);
        EXPECT_STREQ(userName.c_str(), pIceAgent->iceServers[i].username);
        EXPECT_STREQ("pass+word/=", pIceAgent->iceServers[i].credential);
    }
}

TEST_F(ViewerSuppliedIceServersPeerConnectionTest, fallsBackToTheSignalingClient)
{
    // An offer without servers, and one whose credentials have expired.
    EXPECT_EQ(STATUS_NULL_ARG, initializeWithOffer(R"({"type":"offer","sdp":"v=0\r\n"})"));
    EXPECT_EQ(STATUS_NULL_ARG, initializeWithOffer(offerWith(iceServerSet(R"("turn:127.0.0.1:3478")", userNameExpiringIn(-60)))));
    EXPECT_EQ((PRtcPeerConnection) NULL, mPeerConnection);
}

TEST_F(ViewerSuppliedIceServersPeerConnectionTest, viewerSuppliedServersAreIgnoredWhenDisabled)
{
    mSampleConfiguration.useViewerTurnServers = FALSE;

    // Falls back to the signaling client even though the offer carries usable servers.
    EXPECT_EQ(STATUS_NULL_ARG, initializeWithOffer(offerWith(iceServerSet(R"("turn:127.0.0.1:3478")", userNameExpiringIn(300)))));
    EXPECT_EQ((PRtcPeerConnection) NULL, mPeerConnection);
}

TEST_F(ViewerSuppliedIceServersPeerConnectionTest, viewerSuppliedServersAreIgnoredWithoutTurn)
{
    mSampleConfiguration.useTurn = FALSE;

    EXPECT_EQ(STATUS_SUCCESS, initializeWithOffer(offerWith(iceServerSet(R"("turn:127.0.0.1:3478")", userNameExpiringIn(300)))));
    // STUN only.
    EXPECT_EQ(1, mSampleConfiguration.iceUriCount);
}

TEST_F(ViewerSuppliedIceServersPeerConnectionTest, withoutAnOffer)
{
    EXPECT_EQ(STATUS_NULL_ARG, initializePeerConnection(&mSampleConfiguration, NULL, &mPeerConnection));

    mSampleConfiguration.useTurn = FALSE;
    EXPECT_EQ(STATUS_SUCCESS, initializePeerConnection(&mSampleConfiguration, NULL, &mPeerConnection));
    EXPECT_EQ(1, mSampleConfiguration.iceUriCount);
}

} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com
