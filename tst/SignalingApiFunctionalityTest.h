
#ifndef AMAZON_KINESIS_VIDEO_STREAMS_WEBRTC_SDK_C_SIGNALINGAPIFUNCTIONALITYTEST_H
#define AMAZON_KINESIS_VIDEO_STREAMS_WEBRTC_SDK_C_SIGNALINGAPIFUNCTIONALITYTEST_H

#include "WebRTCClientTestFixture.h"

namespace com {
namespace amazonaws {
namespace kinesis {
namespace video {
namespace webrtcclient {

class SignalingApiFunctionalityTest : public WebRtcClientTestBase {
  public:
    SignalingApiFunctionalityTest();

    PSignalingClient pActiveClient;
    UINT32 getIceConfigFail;
    UINT32 getIceConfigRecover;
    UINT32 getIceConfigCount;
    STATUS getIceConfigResult;
    UINT32 signalingStatesCounts[SIGNALING_CLIENT_STATE_MAX_VALUE];
    STATUS errStatus;
    CHAR errMsg[1024];

    STATUS connectResult;
    UINT32 connectFail;
    UINT32 connectRecover;
    UINT32 connectCount;

    STATUS describeResult;
    UINT32 describeFail;
    UINT32 describeRecover;
    UINT32 describeCount;

    STATUS describeMediaResult;
    UINT32 describeMediaFail;
    UINT32 describeMediaRecover;
    UINT32 describeMediaCount;

    STATUS getEndpointResult;
    UINT32 getEndpointFail;
    UINT32 getEndpointRecover;
    UINT32 getEndpointCount;
};

STATUS masterMessageReceived(UINT64, PReceivedSignalingMessage);
STATUS signalingClientStateChanged(UINT64, SIGNALING_CLIENT_STATE);
STATUS signalingClientError(UINT64, STATUS, PCHAR, UINT32);
STATUS viewerMessageReceived(UINT64, PReceivedSignalingMessage);
STATUS getIceConfigPreHook(UINT64);
UINT64 getCurrentTimeFastClock(UINT64);
UINT64 getCurrentTimeSlowClock(UINT64);
STATUS connectPreHook(UINT64);
STATUS describePreHook(UINT64);
STATUS describeMediaPreHook(UINT64);
STATUS getEndpointPreHook(UINT64);
VOID setupSignalingStateMachineRetryStrategyCallbacks(PSignalingClientInfoInternal);
VOID setupSignalingStateMachineRetryStrategyCallbacks(PSignalingClientInfo);

} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com

#endif // AMAZON_KINESIS_VIDEO_STREAMS_WEBRTC_SDK_C_SIGNALINGAPIFUNCTIONALITYTEST_H
