cmake_minimum_required(VERSION 3.22.1)
project(WebRtcAndroidTest)

# =============================================================================
# Superbuild handles: OpenSSL, usrsctp, libsrtp, gtest, and the SDK.
# All cross-compilation setup (NDK toolchain, OpenSSL platform detection,
# OPEN_SRC_INSTALL_PREFIX) is handled inside the superbuild.
# =============================================================================
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../superbuild superbuild)

# =============================================================================
# JNI test library  (webrtc_test_jni.so)
# =============================================================================
set(WEBRTC_TST_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../tst")
file(GLOB JNI_TEST_SOURCES "${WEBRTC_TST_DIR}/*.cpp")

# Exclude main.cpp — jni_bridge.cpp is the entry point for the JNI library
list(FILTER JNI_TEST_SOURCES EXCLUDE REGEX ".*main\\.cpp$")

# Exclude signaling tests (ENABLE_SIGNALING=OFF, headers/symbols unavailable)
list(FILTER JNI_TEST_SOURCES EXCLUDE REGEX ".*Signaling.*\\.cpp")
list(FILTER JNI_TEST_SOURCES EXCLUDE REGEX ".*IngestionFunctionalityTests\\.cpp")
list(FILTER JNI_TEST_SOURCES EXCLUDE REGEX ".*CustomEndpointTest\\.cpp")
list(FILTER JNI_TEST_SOURCES EXCLUDE REGEX ".*DualStackEndpointsTest\\.cpp")
list(FILTER JNI_TEST_SOURCES EXCLUDE REGEX ".*TurnConnectionFunctionalityTest\\.cpp")
list(FILTER JNI_TEST_SOURCES EXCLUDE REGEX ".*IceConfigParsingTest\\.cpp")

add_library(webrtc_test_jni SHARED
    ${JNI_TEST_SOURCES}
    "${CMAKE_CURRENT_SOURCE_DIR}/jni_bridge.cpp")

# The gtest-android-override dir must come BEFORE the real gtest include so
# our wrapper gtest/gtest.h is found first (it disables EXPECT_DEATH /
# ASSERT_DEATH which use fork() and crash the JVM in JNI context).
target_include_directories(webrtc_test_jni BEFORE PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/gtest-android-override")

# The SDK and superbuild use include_directories() (directory-scoped), which
# does not propagate to targets defined outside their add_subdirectory scope.
# We must explicitly add srtp2 and usrsctp include paths here.
target_include_directories(webrtc_test_jni PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/include"
    "${OPEN_SRC_INSTALL_PREFIX}/include"
    "${CMAKE_BINARY_DIR}/srtp-include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../superbuild/usrsctp/usrsctplib")

# Resolve GTest target name
if(TARGET GTest::GTest)
    set(_GTEST GTest::GTest)
elseif(TARGET GTest::gtest)
    set(_GTEST GTest::gtest)
else()
    set(_GTEST gtest)
endif()

# The SDK uses add_definitions() (directory-scoped) for KVS_USE_OPENSSL,
# ENABLE_DATA_CHANNEL, etc.  Pull those into our target so the internal
# headers compile correctly.
get_directory_property(_SDK_COMPILE_DEFS
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/.." COMPILE_DEFINITIONS)
target_compile_definitions(webrtc_test_jni PRIVATE ${_SDK_COMPILE_DEFS})

target_link_libraries(webrtc_test_jni PRIVATE
    kvsWebrtcClient
    ${OPENSSL_CRYPTO_LIBRARY}
    kvspicUtils
    ${_GTEST}
    log)

if(TARGET openssl_install)
    add_dependencies(webrtc_test_jni openssl_install)
endif()
