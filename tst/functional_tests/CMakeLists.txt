cmake_minimum_required(VERSION 2.8)

# Enabling the instrumented allocators to track memory
add_definitions(-DINSTRUMENTED_ALLOCATORS)

project (WebRTCFunctionalTests)

set(CMAKE_CXX_STANDARD 11)
set(KINESIS_VIDEO_WebRTCClient_SRC "..")

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
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/functional_tests/framework/")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/functional_tests/tests/")

file(GLOB WEBRTC_CLIENT_TEST_SOURCE_FILES "*.c*",
        "framework/configuration/*.cpp"
        "framework/logger/*.cpp"
        "framework/metrics/*.cpp"
        "framework/process_manager/*.cpp"
        "framework/utils/*.cpp"
        "framework/orchestrator/*.cpp"
        "framework/peers/*.cpp"
        "tests/*.cpp")

file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/framework/scripts" DESTINATION /tmp)
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_scenario_configs" DESTINATION /tmp)

add_executable(webrtc_functional_tests ${WEBRTC_CLIENT_TEST_SOURCE_FILES})
target_link_libraries(webrtc_functional_tests
    kvsWebrtcClient
    kvsWebrtcSignalingClient
    ${OPENSSL_CRYPTO_LIBRARY}
    kvspicUtils
    ${GTEST_LIBNAME})