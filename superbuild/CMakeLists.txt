cmake_minimum_required(VERSION 3.14)
# VERSION required so CMAKE_PROJECT_VERSION is non-empty; libsrtp uses it in
# set_target_properties(srtp2 PROPERTIES VERSION ${CMAKE_PROJECT_VERSION}).
project(KinesisWebRTCSuperbuild VERSION 1.0.0 LANGUAGES C)

option(USE_SYSTEM_OPENSSL "Use system OpenSSL instead of building from submodule" OFF)

set(BUILD_OPENSSL_PLATFORM "" CACHE STRING "OpenSSL cross-compile target platform")
option(OPENSSL_NO_ASM "Disable OpenSSL assembly optimizations" OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
endif()

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# =============================================================================
# Android cross-compilation
# =============================================================================
if(CMAKE_SYSTEM_NAME STREQUAL "Android")
  # Auto-set OpenSSL target platform from ANDROID_ABI when not specified.
  if(NOT BUILD_OPENSSL_PLATFORM OR BUILD_OPENSSL_PLATFORM STREQUAL "")
    if(ANDROID_ABI STREQUAL "arm64-v8a")
      set(BUILD_OPENSSL_PLATFORM "android-arm64"   CACHE STRING "" FORCE)
    elseif(ANDROID_ABI STREQUAL "armeabi-v7a")
      set(BUILD_OPENSSL_PLATFORM "android-arm"     CACHE STRING "" FORCE)
    elseif(ANDROID_ABI STREQUAL "x86_64")
      set(BUILD_OPENSSL_PLATFORM "android-x86_64"  CACHE STRING "" FORCE)
    elseif(ANDROID_ABI STREQUAL "x86")
      set(BUILD_OPENSSL_PLATFORM "android-x86"     CACHE STRING "" FORCE)
    endif()
  endif()

  # OpenSSL's Configure script needs ANDROID_NDK_HOME and NDK clang on PATH.
  if(CMAKE_ANDROID_NDK)
    set(ENV{ANDROID_NDK_HOME} "${CMAKE_ANDROID_NDK}")
  elseif(ANDROID_NDK)
    set(ENV{ANDROID_NDK_HOME} "${ANDROID_NDK}")
  elseif(CMAKE_TOOLCHAIN_FILE)
    # Derive NDK path from toolchain: <NDK>/build/cmake/android.toolchain.cmake
    get_filename_component(_ndk "${CMAKE_TOOLCHAIN_FILE}" DIRECTORY)
    get_filename_component(_ndk "${_ndk}" DIRECTORY)
    get_filename_component(_ndk "${_ndk}" DIRECTORY)
    set(ENV{ANDROID_NDK_HOME} "${_ndk}")
  endif()
  file(GLOB _ndk_bins "$ENV{ANDROID_NDK_HOME}/toolchains/llvm/prebuilt/*/bin")
  list(GET _ndk_bins 0 _ndk_bin)
  set(ENV{PATH} "${_ndk_bin}:$ENV{PATH}")
  message(STATUS "Android NDK for OpenSSL: $ENV{ANDROID_NDK_HOME}")

  # Note: superbuild-stable also adds an INTERFACE `rt` target to paper over
  # kvspic's -lrt on Android bionic.  Not needed here because ENABLE_SELF_CONTAINED=ON
  # uses vendored deps/src/ sources instead of kvspic.
endif()

include(ProcessorCount)
ProcessorCount(NPROC)
if(NPROC EQUAL 0)
  set(NPROC 1)
endif()

set(DEPS_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/deps-install" CACHE PATH
    "Install prefix for autotools-built deps (OpenSSL)")

# =============================================================================
# 1. OpenSSL  (autotools – configure-time, identical to superbuild-stable)
# =============================================================================
if(USE_SYSTEM_OPENSSL)
  find_package(OpenSSL REQUIRED)
else()
  if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/openssl/Configure")
    message(FATAL_ERROR "openssl submodule not initialised. Run: git submodule update --init superbuild/openssl")
  endif()

  find_program(MAKE_EXE NAMES make gmake REQUIRED)
  set(OPENSSL_BUILD_SRC "${CMAKE_BINARY_DIR}/openssl-build")

  if(NOT EXISTS "${DEPS_INSTALL_PREFIX}/lib/libssl.a")
    message(STATUS "Building OpenSSL from submodule...")

    execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/openssl" "${OPENSSL_BUILD_SRC}")

    set(_ossl_flags "")
    if(OPENSSL_NO_ASM)
      list(APPEND _ossl_flags no-asm)
    endif()

    if(BUILD_OPENSSL_PLATFORM AND NOT BUILD_OPENSSL_PLATFORM STREQUAL "")
      execute_process(
          COMMAND ./Configure ${_ossl_flags} no-shared no-dso no-async
                  --prefix=${DEPS_INSTALL_PREFIX} --openssldir=${DEPS_INSTALL_PREFIX}
                  ${BUILD_OPENSSL_PLATFORM}
          WORKING_DIRECTORY "${OPENSSL_BUILD_SRC}" RESULT_VARIABLE _r)
    else()
      if(APPLE AND CMAKE_OSX_SYSROOT)
        set(_ossl_env_cmd ${CMAKE_COMMAND} -E env "CFLAGS=-isysroot ${CMAKE_OSX_SYSROOT}")
      elseif(APPLE)
        execute_process(COMMAND xcrun --show-sdk-path OUTPUT_VARIABLE _osx_sdk OUTPUT_STRIP_TRAILING_WHITESPACE)
        if(_osx_sdk)
          set(_ossl_env_cmd ${CMAKE_COMMAND} -E env "CFLAGS=-isysroot ${_osx_sdk}")
        else()
          set(_ossl_env_cmd "")
        endif()
      else()
        set(_ossl_env_cmd "")
      endif()
      execute_process(
          COMMAND ${_ossl_env_cmd} ./config ${_ossl_flags} no-shared no-dso
                  --prefix=${DEPS_INSTALL_PREFIX} --openssldir=${DEPS_INSTALL_PREFIX}
          WORKING_DIRECTORY "${OPENSSL_BUILD_SRC}" RESULT_VARIABLE _r)
    endif()
    if(NOT _r EQUAL 0)
      message(FATAL_ERROR "OpenSSL configure failed")
    endif()

    execute_process(COMMAND ${MAKE_EXE} -j${NPROC}
        WORKING_DIRECTORY "${OPENSSL_BUILD_SRC}" RESULT_VARIABLE _r)
    if(NOT _r EQUAL 0)
      message(FATAL_ERROR "OpenSSL build failed")
    endif()

    execute_process(COMMAND ${MAKE_EXE} install_sw
        WORKING_DIRECTORY "${OPENSSL_BUILD_SRC}" RESULT_VARIABLE _r)
    if(NOT _r EQUAL 0)
      message(FATAL_ERROR "OpenSSL install failed")
    endif()
  endif()

  # Custom target so ninja/make regenerates OpenSSL if its output is deleted.
  add_custom_command(
      OUTPUT  "${DEPS_INSTALL_PREFIX}/lib/libssl.a"
              "${DEPS_INSTALL_PREFIX}/lib/libcrypto.a"
      COMMAND ${MAKE_EXE} install_sw
      WORKING_DIRECTORY "${OPENSSL_BUILD_SRC}"
      COMMENT "Reinstalling OpenSSL...")
  add_custom_target(openssl_install ALL
      DEPENDS "${DEPS_INSTALL_PREFIX}/lib/libssl.a"
              "${DEPS_INSTALL_PREFIX}/lib/libcrypto.a")

  set(OPENSSL_ROOT_DIR "${DEPS_INSTALL_PREFIX}" CACHE PATH "" FORCE)
  list(APPEND CMAKE_FIND_ROOT_PATH "${DEPS_INSTALL_PREFIX}")
  find_package(OpenSSL REQUIRED)
endif()

# =============================================================================
# 2. usrsctp  (add_subdirectory)
# =============================================================================
set(sctp_werror         OFF CACHE BOOL "" FORCE)
set(sctp_build_programs OFF CACHE BOOL "" FORCE)
add_subdirectory(usrsctp)
# usrsctp.h lives in usrsctplib/ – expose it globally (SDK includes it even
# from targets that don't link usrsctp directly)
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/usrsctp/usrsctplib")

# =============================================================================
# 3. libsrtp  (add_subdirectory)
# =============================================================================
set(ENABLE_OPENSSL ON  CACHE BOOL "" FORCE)
set(TEST_APPS      OFF CACHE BOOL "" FORCE)
if(NOT USE_SYSTEM_OPENSSL)
  set(OPENSSL_ROOT_DIR "${DEPS_INSTALL_PREFIX}" CACHE PATH "" FORCE)
endif()
add_subdirectory(libsrtp)

# SDK expects <srtp2/srtp.h> but libsrtp source has include/srtp.h.
# Mirror the installed header layout in the build tree.
set(_srtp_compat "${CMAKE_BINARY_DIR}/srtp-include/srtp2")
file(MAKE_DIRECTORY "${_srtp_compat}")
foreach(_h include/srtp.h crypto/include/auth.h crypto/include/cipher.h crypto/include/crypto_types.h)
  file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/libsrtp/${_h}" DESTINATION "${_srtp_compat}")
endforeach()
include_directories("${CMAKE_BINARY_DIR}/srtp-include")

# =============================================================================
# 4. googletest  (add_subdirectory)
# =============================================================================
set(BUILD_GMOCK   OFF CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
add_subdirectory(googletest)

# tst/CMakeLists.txt calls find_package(GTest REQUIRED PATHS ${OPEN_SRC_INSTALL_PREFIX}).
# OPEN_SRC_INSTALL_PREFIX points at DEPS_INSTALL_PREFIX (OpenSSL's dir), which has no
# GTest.  CMake would then fall back to the system GTest (e.g. Homebrew), whose
# GTestTargets.cmake tries to import GTest::gtest + GTest::gmock as IMPORTED targets —
# but googletest's add_subdirectory already created GTest::gtest/GTest::gtest_main as
# ALIAS targets, causing a conflict.  Fix: write a minimal stub GTestConfig.cmake into
# DEPS_INSTALL_PREFIX so find_package(GTest PATHS DEPS_INSTALL_PREFIX) succeeds there
# and never reaches the system installation.
file(MAKE_DIRECTORY "${DEPS_INSTALL_PREFIX}/lib/cmake/GTest")
file(WRITE "${DEPS_INSTALL_PREFIX}/lib/cmake/GTest/GTestConfig.cmake"
"# Stub generated by superbuild – targets are already in scope from add_subdirectory(googletest)
set(GTest_FOUND TRUE)
set(GTEST_FOUND TRUE)
")
file(WRITE "${DEPS_INSTALL_PREFIX}/lib/cmake/GTest/GTestConfigVersion.cmake"
"set(PACKAGE_VERSION \"1.12.1\")
set(PACKAGE_VERSION_COMPATIBLE TRUE)
set(PACKAGE_VERSION_EXACT FALSE)
")
# GTest_DIR is checked first (before CMAKE_SYSTEM_PREFIX_PATH which includes
# /opt/homebrew on macOS) so set it explicitly to our stub to prevent cmake
# from finding the system GTest installation.
set(GTest_DIR "${DEPS_INSTALL_PREFIX}/lib/cmake/GTest" CACHE PATH "" FORCE)

# =============================================================================
# 5. WebRTC SDK  (add_subdirectory)
# =============================================================================
set(BUILD_DEPENDENCIES      OFF CACHE BOOL "" FORCE)
set(ENABLE_SELF_CONTAINED   ON  CACHE BOOL "" FORCE)
set(BUILD_SAMPLE            OFF CACHE BOOL "" FORCE)
set(BUILD_STATIC_LIBS       ON  CACHE BOOL "" FORCE)
set(BUILD_TEST              ON  CACHE BOOL "" FORCE)
set(ENABLE_AWS_SDK_IN_TESTS OFF CACHE BOOL "" FORCE)
set(OPEN_SRC_INSTALL_PREFIX "${DEPS_INSTALL_PREFIX}" CACHE PATH "" FORCE)

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/.." "${CMAKE_BINARY_DIR}/sdk")

# Preserve build order: everything that includes OpenSSL headers must wait
# for the configure-time build to be registered by ninja/make.
if(TARGET openssl_install)
  add_dependencies(srtp2           openssl_install)
  add_dependencies(kvsWebrtcClient openssl_install)
  if(TARGET webrtc_client_test)
    add_dependencies(webrtc_client_test openssl_install)
  endif()
endif()
