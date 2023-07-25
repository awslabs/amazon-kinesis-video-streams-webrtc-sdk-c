cmake_minimum_required(VERSION 3.6.3)

project (WebRTCClientBenchmark)

set(CMAKE_CXX_STANDARD 11)
set(KINESIS_VIDEO_WebRTCClient_SRC "${CMAKE_CURRENT_SOURCE_DIR}/..")

if (OPEN_SRC_INSTALL_PREFIX)
  find_package(benchmark REQUIRED PATHS ${OPEN_SRC_INSTALL_PREFIX})
else()
  find_package(benchmark REQUIRED)
endif()

include_directories(${KINESIS_VIDEO_WebRTCClient_SRC})

file(GLOB WEBRTC_CLIENT_BENCHMARK_SOURCE_FILES "*.cpp" )

add_executable(webrtc_client_benchmark ${WEBRTC_CLIENT_BENCHMARK_SOURCE_FILES})
target_link_libraries(webrtc_client_benchmark
    kvsWebrtcClient
    kvsWebrtcSignalingClient
    kvspicUtils
    benchmark::benchmark)
