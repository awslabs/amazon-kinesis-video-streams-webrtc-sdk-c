#include <benchmark/benchmark.h>
#include "../src/source/Include_i.h"
#include <thread>
#include <mutex>
#include <queue>
#include <atomic>

#define MAX_BENCHMARK_AWAIT_DURATION (2 * HUNDREDS_OF_NANOS_IN_A_SECOND)

namespace com {
namespace amazonaws {
namespace kinesis {
namespace video {
namespace webrtcclient {

class WebRtcClientBenchmarkBase : public benchmark::Fixture {
  protected:
    virtual VOID SetUp(const ::benchmark::State& state);
    virtual VOID TearDown(const ::benchmark::State& state);
};

} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com
