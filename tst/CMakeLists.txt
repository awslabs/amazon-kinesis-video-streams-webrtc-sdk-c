cmake_minimum_required(VERSION 2.8)

# Enabling the instrumented allocators to track memory
add_definitions(-DINSTRUMENTED_ALLOCATORS)


project (WebRTCClientTest)

set(CMAKE_CXX_STANDARD 11)
set(KINESIS_VIDEO_WebRTCClient_SRC "${CMAKE_CURRENT_SOURCE_DIR}/..")

if (OPEN_SRC_INSTALL_PREFIX)
  find_package(GTest REQUIRED PATHS ${OPEN_SRC_INSTALL_PREFIX})
else()
  find_package(GTest REQUIRED)
endif()

SET(GTEST_LIBNAME GTest::gtest)
if (TARGET GTest::GTest)
  SET(GTEST_LIBNAME GTest::GTest)
endif()

include_directories(${KINESIS_VIDEO_WebRTCClient_SRC})

file(GLOB WEBRTC_CLIENT_TEST_SOURCE_FILES "*.cpp" )

add_executable(webrtc_client_test ${WEBRTC_CLIENT_TEST_SOURCE_FILES})
target_link_libraries(webrtc_client_test
    kvsWebrtcClient
    kvsWebrtcSignalingClient
    ${OPENSSL_CRYPTO_LIBRARY}
    kvspicUtils
    ${GTEST_LIBNAME})
