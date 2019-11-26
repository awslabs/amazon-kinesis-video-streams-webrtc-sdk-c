#include "WebRTCClientTestFixture.h"

namespace com { namespace amazonaws { namespace kinesis { namespace video { namespace webrtcclient {

class IceFunctionalityTest : public WebRtcClientTestBase {
};

// check if iceCandidatePairs is in descending order
BOOL candidatePairsInOrder(PIceCandidatePair *iceCandidatePairs, UINT32 candidatePairCount)
{
    BOOL inOrder = TRUE;
    UINT32 i;

    if (candidatePairCount < 2) {
        return inOrder;
    }

    for(i = 0; i < candidatePairCount - 1 && inOrder; ++i) {
        if (iceCandidatePairs[i]->priority < iceCandidatePairs[i+1]->priority) {
            inOrder = FALSE;
        }
    }

    return inOrder;
}

TEST_F(IceFunctionalityTest, sortIceCandidatePairsTest)
{
    IceAgent iceAgent;
    IceCandidatePair iceCandidatePair[10];
    UINT32 i;
    for (i = 0; i < 10; ++i) {
        iceAgent.candidatePairs[i] = &iceCandidatePair[i];
    }

    iceAgent.candidatePairCount = 0;
    EXPECT_EQ(STATUS_SUCCESS, sortIceCandidatePairs(&iceAgent));
    EXPECT_EQ(TRUE, candidatePairsInOrder(iceAgent.candidatePairs, iceAgent.candidatePairCount));

    iceAgent.candidatePairCount = 1;
    iceCandidatePair[0].priority = 1;
    EXPECT_EQ(STATUS_SUCCESS, sortIceCandidatePairs(&iceAgent));
    EXPECT_EQ(TRUE, candidatePairsInOrder(iceAgent.candidatePairs, iceAgent.candidatePairCount));

    iceAgent.candidatePairCount = 2;
    iceCandidatePair[0].priority = 1;
    iceCandidatePair[1].priority = 2;
    EXPECT_EQ(STATUS_SUCCESS, sortIceCandidatePairs(&iceAgent));
    EXPECT_EQ(TRUE, candidatePairsInOrder(iceAgent.candidatePairs, iceAgent.candidatePairCount));

    iceAgent.candidatePairCount = 2;
    iceCandidatePair[0].priority = 2;
    iceCandidatePair[1].priority = 1;
    EXPECT_EQ(STATUS_SUCCESS, sortIceCandidatePairs(&iceAgent));
    EXPECT_EQ(TRUE, candidatePairsInOrder(iceAgent.candidatePairs, iceAgent.candidatePairCount));

    iceAgent.candidatePairCount = 3;
    iceCandidatePair[0].priority = 1;
    iceCandidatePair[1].priority = 1;
    iceCandidatePair[2].priority = 2;
    EXPECT_EQ(STATUS_SUCCESS, sortIceCandidatePairs(&iceAgent));
    EXPECT_EQ(TRUE, candidatePairsInOrder(iceAgent.candidatePairs, iceAgent.candidatePairCount));

    iceAgent.candidatePairCount = 3;
    iceCandidatePair[0].priority = 1;
    iceCandidatePair[1].priority = 2;
    iceCandidatePair[2].priority = 1;
    EXPECT_EQ(STATUS_SUCCESS, sortIceCandidatePairs(&iceAgent));
    EXPECT_EQ(TRUE, candidatePairsInOrder(iceAgent.candidatePairs, iceAgent.candidatePairCount));

    iceAgent.candidatePairCount = 10;
    iceCandidatePair[0].priority = 12312;
    iceCandidatePair[1].priority = 23;
    iceCandidatePair[2].priority = 656;
    iceCandidatePair[3].priority = 123123;
    iceCandidatePair[4].priority = 432432;
    iceCandidatePair[5].priority = 312312312;
    iceCandidatePair[6].priority = 123123;
    iceCandidatePair[7].priority = 4546457;
    iceCandidatePair[8].priority = 87867;
    iceCandidatePair[9].priority = 87678;
    EXPECT_EQ(STATUS_SUCCESS, sortIceCandidatePairs(&iceAgent));
    EXPECT_EQ(TRUE, candidatePairsInOrder(iceAgent.candidatePairs, iceAgent.candidatePairCount));
}

}
}
}
}
}