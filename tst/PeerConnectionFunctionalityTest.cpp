#include "WebRTCClientTestFixture.h"

namespace com { namespace amazonaws { namespace kinesis { namespace video { namespace webrtcclient {

class PeerConnectionFunctionalityTest : public WebRtcClientTestBase {
};

// Connect two RtcPeerConnections, and wait for them to be connected
// in the given amount of time. Return false if they don't go to connected in
// the expected amounted of time
bool connectTwoPeers(PRtcPeerConnection offerPc, PRtcPeerConnection answerPc) {
    RtcSessionDescriptionInit sdp;
    volatile SIZE_T connectedCount = 0;

    auto onICECandidateHdlr = [](UINT64 customData, PCHAR candidateStr) -> void {
        if (candidateStr != NULL) {
            std::thread([customData] (std::string candidate) {
                RtcIceCandidateInit iceCandidate;
                EXPECT_EQ(deserializeRtcIceCandidateInit((PCHAR) candidate.c_str(), STRLEN(candidate.c_str()), &iceCandidate), STATUS_SUCCESS);
                EXPECT_EQ(addIceCandidate((PRtcPeerConnection) customData, iceCandidate.candidate), STATUS_SUCCESS);
            }, std::string(candidateStr)).detach();
        }
    };

    EXPECT_EQ(peerConnectionOnIceCandidate(offerPc, (UINT64) answerPc, onICECandidateHdlr), STATUS_SUCCESS);
    EXPECT_EQ(peerConnectionOnIceCandidate(answerPc, (UINT64) offerPc, onICECandidateHdlr), STATUS_SUCCESS);

    auto onICEConnectionStateChangeHdlr = [](UINT64 customData, RTC_PEER_CONNECTION_STATE newState) -> void {
        if (newState == RTC_PEER_CONNECTION_STATE_CONNECTED) {
            ATOMIC_INCREMENT((PSIZE_T)customData);
        }
    };
    EXPECT_EQ(peerConnectionOnConnectionStateChange(offerPc, (UINT64) &connectedCount, onICEConnectionStateChangeHdlr), STATUS_SUCCESS);
    EXPECT_EQ(peerConnectionOnConnectionStateChange(answerPc, (UINT64) &connectedCount, onICEConnectionStateChangeHdlr), STATUS_SUCCESS);

    EXPECT_EQ(createOffer(offerPc, &sdp), STATUS_SUCCESS);
    EXPECT_EQ(setLocalDescription(offerPc, &sdp), STATUS_SUCCESS);
    EXPECT_EQ(setRemoteDescription(answerPc, &sdp), STATUS_SUCCESS);

    EXPECT_EQ(createAnswer(answerPc, &sdp), STATUS_SUCCESS);
    EXPECT_EQ(setLocalDescription(answerPc, &sdp), STATUS_SUCCESS);
    EXPECT_EQ(setRemoteDescription(offerPc, &sdp), STATUS_SUCCESS);

    for (auto i = 0; i <= 10 && ATOMIC_LOAD(&connectedCount) != 2; i++) {
        THREAD_SLEEP(HUNDREDS_OF_NANOS_IN_A_SECOND);
    }

    return ATOMIC_LOAD(&connectedCount) ==  2;
}

// Assert that two PeerConnections can connect to each other and go to connected
TEST_F(PeerConnectionFunctionalityTest, connectTwoPeers)
{
    RtcConfiguration configuration;
    PRtcPeerConnection offerPc = NULL, answerPc = NULL;

    MEMSET(&configuration, 0x00, SIZEOF(RtcConfiguration));

    EXPECT_EQ(createPeerConnection(&configuration, &offerPc), STATUS_SUCCESS);
    EXPECT_EQ(createPeerConnection(&configuration, &answerPc), STATUS_SUCCESS);

    EXPECT_EQ(connectTwoPeers(offerPc, answerPc), TRUE);

    freePeerConnection(&offerPc);
    freePeerConnection(&answerPc);
}

// Assert that two PeerConnections with forced TURN can connect to each other and go to connected
TEST_F(PeerConnectionFunctionalityTest, connectTwoPeersForcedTURN)
{
    if (!mAccessKeyIdSet) {
        return;
    }

    RtcConfiguration configuration;
    PRtcPeerConnection offerPc = NULL, answerPc = NULL;
    UINT32 i, j, iceConfigCount, uriCount;
    PIceConfigInfo pIceConfigInfo;

    MEMSET(&configuration, 0x00, SIZEOF(RtcConfiguration));
    configuration.iceTransportPolicy = ICE_TRANSPORT_POLICY_RELAY;

    initializeSignalingClient();
    EXPECT_EQ(signalingClientGetIceConfigInfoCout(mSignalingClientHandle, &iceConfigCount), STATUS_SUCCESS);

    for (uriCount = 0, i = 0; i < iceConfigCount; i++) {
        EXPECT_EQ(signalingClientGetIceConfigInfo(mSignalingClientHandle, i, &pIceConfigInfo), STATUS_SUCCESS);
        for (j = 0; j < pIceConfigInfo->uriCount; j++) {
            STRNCPY(configuration.iceServers[uriCount + 1].urls, pIceConfigInfo->uris[j], MAX_ICE_CONFIG_URI_LEN);
            STRNCPY(configuration.iceServers[uriCount + 1].credential, pIceConfigInfo->password, MAX_ICE_CONFIG_CREDENTIAL_LEN);
            STRNCPY(configuration.iceServers[uriCount + 1].username, pIceConfigInfo->userName, MAX_ICE_CONFIG_USER_NAME_LEN);

            uriCount++;
        }
    }


    EXPECT_EQ(createPeerConnection(&configuration, &offerPc), STATUS_SUCCESS);
    EXPECT_EQ(createPeerConnection(&configuration, &answerPc), STATUS_SUCCESS);

    EXPECT_EQ(connectTwoPeers(offerPc, answerPc), TRUE);

    freePeerConnection(&offerPc);
    freePeerConnection(&answerPc);

    deinitializeSignalingClient();
}


}
}
}
}
}
