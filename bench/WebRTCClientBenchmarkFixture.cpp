#include "WebRTCClientBenchmarkFixture.h"

namespace com {
namespace amazonaws {
namespace kinesis {
namespace video {
namespace webrtcclient {

VOID WebRtcClientBenchmarkBase::SetUp(const ::benchmark::State& state)
{
    UNUSED_PARAM(state);
    SET_LOGGER_LOG_LEVEL(LOG_LEVEL_WARN);
    initKvsWebRtc();
}

VOID WebRtcClientBenchmarkBase::TearDown(const ::benchmark::State& state)
{
    UNUSED_PARAM(state);
    deinitKvsWebRtc();
}

} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com
