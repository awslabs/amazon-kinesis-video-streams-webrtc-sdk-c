#include "WebRTCClientTestFixture.h"

namespace com { namespace amazonaws { namespace kinesis { namespace video { namespace webrtcclient {

class PeerConnectionFunctionalityTest : public WebRtcClientTestBase {
};

// Assert that two PeerConnections can connect to each other and go to connected
TEST_F(PeerConnectionFunctionalityTest, connectTwoPeers)
{
    RtcConfiguration configuration;
    PRtcPeerConnection offerPc = NULL, answerPc = NULL;
    RtcSessionDescriptionInit sdp;
    volatile SIZE_T connectedCount = 0;

    MEMSET(&configuration, 0x00, SIZEOF(RtcConfiguration));

    EXPECT_EQ(createPeerConnection(&configuration, &offerPc), STATUS_SUCCESS);
    EXPECT_EQ(createPeerConnection(&configuration, &answerPc), STATUS_SUCCESS);

    auto onICECandidateHdlr = [](UINT64 customData, PCHAR candidateStr) -> void {
        RtcIceCandidateInit iceCandidate;

        if (candidateStr != NULL) {
            EXPECT_EQ(deserializeRtcIceCandidateInit(candidateStr, STRLEN(candidateStr), &iceCandidate), STATUS_SUCCESS);
            EXPECT_EQ(addIceCandidate((PRtcPeerConnection) customData, iceCandidate.candidate), STATUS_SUCCESS);
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

    for (auto i = 0; i <= 10; i++) {
        THREAD_SLEEP(HUNDREDS_OF_NANOS_IN_A_SECOND);

        if (ATOMIC_LOAD(&connectedCount) == 2) {
            break;
        }
    }


    freePeerConnection(&offerPc);
    freePeerConnection(&answerPc);
    EXPECT_EQ(ATOMIC_LOAD(&connectedCount), 2);
}

}
}
}
}
}
