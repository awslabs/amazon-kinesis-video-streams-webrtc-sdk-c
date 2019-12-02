#include "WebRTCClientTestFixture.h"

namespace com { namespace amazonaws { namespace kinesis { namespace video { namespace webrtcclient {

class PeerConnectionApiTest : public WebRtcClientTestBase {
};

TEST_F(PeerConnectionApiTest, deserializeRtcIceCandidateInit)
{
    RtcIceCandidateInit rtcIceCandidateInit;

    MEMSET(&rtcIceCandidateInit, 0x00, SIZEOF(rtcIceCandidateInit));

    auto notAnObject = "helloWorld";
    EXPECT_EQ(deserializeRtcIceCandidateInit((PCHAR) notAnObject, STRLEN(notAnObject), &rtcIceCandidateInit), STATUS_INVALID_API_CALL_RETURN_JSON);

    auto emptyObject = "{}";
    EXPECT_EQ(deserializeRtcIceCandidateInit((PCHAR) emptyObject, STRLEN(emptyObject), &rtcIceCandidateInit), STATUS_INVALID_API_CALL_RETURN_JSON);

    auto noCandidate = "{randomKey: \"randomValue\"}";
    EXPECT_EQ(deserializeRtcIceCandidateInit((PCHAR) noCandidate, STRLEN(noCandidate), &rtcIceCandidateInit), STATUS_ICE_CANDIDATE_MISSING_CANDIDATE);

    auto keyNoValue = "{1,2,3,4,5}candidate";
    EXPECT_EQ(deserializeRtcIceCandidateInit((PCHAR) keyNoValue, STRLEN(keyNoValue), &rtcIceCandidateInit), STATUS_ICE_CANDIDATE_MISSING_CANDIDATE);

    auto validCandidate = "{candidate: \"foobar\"}";
    EXPECT_EQ(deserializeRtcIceCandidateInit((PCHAR) validCandidate, STRLEN(validCandidate), &rtcIceCandidateInit), STATUS_SUCCESS);
    EXPECT_STREQ(rtcIceCandidateInit.candidate, "foobar");
}

}
}
}
}
}
