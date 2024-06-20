cmake_minimum_required(VERSION 3.12)
project(kvswebrtcmaster VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)

set(AWS_DEPENDENCIES_DIR ${CMAKE_BINARY_DIR}/aws-dependencies)
set(WEBRTC_SAMPLE_SRCS
        kvsWebRTCClientMaster.cpp
        Common.c)

set(WEBRTC_SDK_LIBS_STATIC
        libkvsWebrtcClient.a
        libkvsWebrtcSignalingClient.a
        libkvsCommonLws.a
        libkvspicUtils.a
        libkvspicState.a
        libwebsockets.a
        libsrtp2.a
        libusrsctp.a
        libmbedtls.a
        libmbedx509.a
        libmbedcrypto.a
        pthread
        m)

add_executable(KvsWebrtcMaster ${WEBRTC_SAMPLE_SRCS})
target_include_directories(KvsWebrtcMaster PRIVATE ${AWS_DEPENDENCIES_DIR}/webrtc/include/)
target_link_directories(KvsWebrtcMaster PUBLIC ${AWS_DEPENDENCIES_DIR}/webrtc/lib/)
target_link_libraries(KvsWebrtcMaster PUBLIC ${WEBRTC_SDK_LIBS_STATIC})

include(GNUInstallDirs)

install(TARGETS KvsWebrtcMaster
        RUNTIME DESTINATION ${CMAKE_INSTALL_FULL_BINDIR})