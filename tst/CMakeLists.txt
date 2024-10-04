cmake_minimum_required(VERSION 3.6.3)
set(CMAKE_VERBOSE_MAKEFILE ON)

# Enabling the instrumented allocators to track memory
add_definitions(-DINSTRUMENTED_ALLOCATORS)

project (WebRTCClientTest)

set(CMAKE_CXX_STANDARD 11)
set(KINESIS_VIDEO_WebRTCClient_SRC "${CMAKE_CURRENT_SOURCE_DIR}/..")

if (OPEN_SRC_INSTALL_PREFIX)
  find_package(GTest REQUIRED PATHS ${OPEN_SRC_INSTALL_PREFIX})
  if(ENABLE_AWS_SDK_IN_TESTS)
    set(CMAKE_PREFIX_PATH ${OPEN_SRC_INSTALL_PREFIX})
    find_package(AWSSDK REQUIRED PATHS ${OPEN_SRC_INSTALL_PREFIX} COMPONENTS kinesisvideo kinesis-video-webrtc-storage)
  endif()
else()
  find_package(GTest REQUIRED)
  if(ENABLE_AWS_SDK_IN_TESTS)
    find_package(AWSSDK REQUIRED COMPONENTS kinesisvideo kinesis-video-webrtc-storage)
  endif()

endif()

SET(GTEST_LIBNAME GTest::gtest)
if (TARGET GTest::GTest)
  SET(GTEST_LIBNAME GTest::GTest)
endif()

include_directories(${KINESIS_VIDEO_WebRTCClient_SRC})

set(AWS_SDK_TEST_LIBS "")
if(ENABLE_AWS_SDK_IN_TESTS)
  include_directories(${AWS_SDK_INCLUDE_DIR})
  set(AWS_SDK_TEST_LIBS ${AWSSDK_LINK_LIBRARIES})
endif()

file(GLOB WEBRTC_CLIENT_TEST_SOURCE_FILES "*.cpp" )

add_executable(webrtc_client_test ${WEBRTC_CLIENT_TEST_SOURCE_FILES} SignalingApiFunctionalityTest.h)
target_link_libraries(webrtc_client_test
    kvsWebrtcClient
    kvsWebrtcSignalingClient
    ${EXTRA_DEPS}
    kvsCommonLws
    ${OPENSSL_CRYPTO_LIBRARY}
    kvspicUtils
    ${GTEST_LIBNAME}
    ${AWS_SDK_TEST_LIBS})
