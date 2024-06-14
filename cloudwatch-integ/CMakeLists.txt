cmake_minimum_required(VERSION 3.6.3)

project(KinesisVideoWebRTCClientSamplesCloudwatch LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 11)

message("OPEN_SRC_INSTALL_PREFIX=${OPEN_SRC_INSTALL_PREFIX}")

include_directories(${OPEN_SRC_INSTALL_PREFIX}/include)
include_directories(${OPEN_SRC_INCLUDE_DIRS})
link_directories(${OPEN_SRC_INSTALL_PREFIX}/lib)

find_package(ZLIB REQUIRED)
find_package(AWSSDK REQUIRED COMPONENTS monitoring logs)

set(CW_CONFIG_HEADER "configs/lr_static_h265_openssl.h" CACHE FILEPATH "Config header for the cloudwatch integrated demo")

add_definitions(-DCW_CONFIG_HEADER="${CW_CONFIG_HEADER}")
message(STATUS "Config header set to ${CW_CONFIG_HEADER}")

add_executable(
        kvsWebrtcClientMasterCW
        Cloudwatch.cpp
        CloudwatchLogs.cpp
        CloudwatchMonitoring.cpp
        ../samples/lib/Common.c
        ../samples/lib/Utility.c
        ../samples/lib/MetricsHandling.c
        ../samples/lib/DataChannelHandling.c
        ../samples/lib/SignalingMsgHandler.c
        ../samples/lib/Media.c
        kvsWebRTCClientMasterCloudwatch.cpp)
target_link_libraries(kvsWebrtcClientMasterCW
                      kvsWebrtcClient
                      kvsWebrtcSignalingClient
                      ${EXTRA_DEPS}
                      kvsCommonLws kvspicUtils websockets kvssdp kvsstun
                      ${AWSSDK_LINK_LIBRARIES})

add_executable(
        kvsWebrtcClientViewerCW
        ../samples/lib/Common.c
        ../samples/lib/Utility.c
        ../samples/lib/MetricsHandling.c
        ../samples/lib/DataChannelHandling.c
        ../samples/lib/SignalingMsgHandler.c
        ../samples/lib/Media.c
        Cloudwatch.cpp
        CloudwatchLogs.cpp
        CloudwatchMonitoring.cpp
        kvsWebRTCClientViewerCloudwatch.cpp)
target_link_libraries(kvsWebrtcClientViewerCW
        kvsWebrtcClient
        kvsWebrtcSignalingClient
        ${EXTRA_DEPS}
        kvsCommonLws kvspicUtils websockets kvssdp kvsstun
        ${AWSSDK_LINK_LIBRARIES})
