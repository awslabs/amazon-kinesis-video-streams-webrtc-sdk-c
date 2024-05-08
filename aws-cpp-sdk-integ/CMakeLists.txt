cmake_minimum_required(VERSION 3.6.3)

project(KinesisVideoWebRTCClientSamplesCloudwatch LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 11)

message("OPEN_SRC_INSTALL_PREFIX=${OPEN_SRC_INSTALL_PREFIX}")

include_directories(${OPEN_SRC_INSTALL_PREFIX}/include)
include_directories(${OPEN_SRC_INCLUDE_DIRS})
link_directories(${OPEN_SRC_INSTALL_PREFIX}/lib)

find_package(ZLIB REQUIRED)
find_package(AWSSDK REQUIRED COMPONENTS monitoring logs)

add_executable(
        kvsWebrtcClientMasterCW
        ../samples/Common.c
        ../samples/kvsMetricsMonitoring.c
        ../samples/Utility.c
        ../samples/MetricsHandling.c
        Config.cpp
        CloudwatchMetricsMonitoring.cpp
        kvsWebRTCClientMasterCloudwatch.cpp)
target_link_libraries(kvsWebrtcClientMasterCW
                      kvsWebrtcClient
                      kvsWebrtcSignalingClient
                      ${EXTRA_DEPS}
                      kvsCommonLws kvspicUtils websockets kvssdp kvsstun
                      ${AWSSDK_LINK_LIBRARIES})
