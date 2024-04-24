cmake_minimum_required(VERSION 3.6.3)
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake;${CMAKE_MODULE_PATH}")
include(Utilities)
include(CheckIncludeFiles)
include(CheckFunctionExists)

# The version MUST be updated before every release
project(KinesisVideoWebRTCClient VERSION 1.10.1 LANGUAGES C)

# User Flags
option(ADD_MUCLIBC "Add -muclibc c flag" OFF)
option(BUILD_DEPENDENCIES "Whether or not to build depending libraries from source" ON)
option(USE_OPENSSL "Use openssl as crypto library" ON)
option(USE_MBEDTLS "Use mbedtls as crypto library" OFF)
option(BUILD_STATIC_LIBS "Build all libraries statically. (This includes third-party libraries.)" OFF)
option(BUILD_OPENSSL_PLATFORM "If buildng OpenSSL what is the target platform" OFF)
option(BUILD_LIBSRTP_HOST_PLATFORM "If buildng LibSRTP what is the current platform" OFF)
option(BUILD_LIBSRTP_DESTINATION_PLATFORM "If buildng LibSRTP what is the destination platform" OFF)
option(BUILD_SAMPLE "Build available samples" ON)
option(ENABLE_DATA_CHANNEL "Enable support for data channel" ON)
option(ENABLE_KVS_THREADPOOL "Enable support for KVS thread pool in signaling" ON)
option(INSTRUMENTED_ALLOCATORS "Enable memory instrumentation" OFF)
option(ENABLE_AWS_SDK_IN_TESTS "Enable support for compiling AWS SDKs for tests" ON)

# Developer Flags
option(BUILD_TEST "Build the testing tree." OFF)
option(BUILD_BENCHMARK "Build the benchmark tree." OFF)
option(CODE_COVERAGE "Enable coverage reporting" OFF)
option(COMPILER_WARNINGS "Enable all compiler warnings." OFF)
option(ADDRESS_SANITIZER "Build with AddressSanitizer." OFF)
option(MEMORY_SANITIZER "Build with MemorySanitizer." OFF)
option(THREAD_SANITIZER "Build with ThreadSanitizer." OFF)
option(UNDEFINED_BEHAVIOR_SANITIZER "Build with UndefinedBehaviorSanitizer." OFF)
option(LINK_PROFILER "Link gperftools profiler" OFF)

if(WIN32)
  set(EXT_PTHREAD_INCLUDE_DIR "" CACHE PATH "Path to the PThread dir")
  set(EXT_PTHREAD_LIBRARIES "" CACHE FILEPATH "Path to PThread libraries")
endif()

if(NOT CMAKE_BUILD_TYPE)
  message(STATUS "Setting CMAKE_BUILD_TYPE to Release by default")
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
endif()

execute_process(
        COMMAND git rev-parse HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_COMMIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE)

add_definitions(-DSDK_VERSION=\"${GIT_COMMIT_HASH}\")
add_definitions(-DVERSION_STRING=\"${PROJECT_VERSION}\")
add_definitions(-DDETECTED_GIT_HASH)

if(NOT KVS_STACK_SIZE OR KVS_STACK_SIZE STREQUAL "")
  message(STATUS "Setting default stack size to 64*1024")
  set(KVS_DEFAULT_STACK_SIZE 65536)
  add_definitions(-DKVS_DEFAULT_STACK_SIZE=65536)
else()
  message(STATUS "Setting default stack size to provided value")
  set(KVS_DEFAULT_STACK_SIZE ${KVS_STACK_SIZE})
  add_definitions(-DKVS_DEFAULT_STACK_SIZE=${KVS_STACK_SIZE})
endif()

if(NOT OPEN_SRC_INSTALL_PREFIX OR OPEN_SRC_INSTALL_PREFIX STREQUAL "")
  set(OPEN_SRC_INSTALL_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/open-source" CACHE PATH "Libraries will be downloaded and built in this directory.")
else()
  set(OPEN_SRC_INSTALL_PREFIX ${OPEN_SRC_INSTALL_PREFIX} CACHE PATH "Libraries will be downloaded and built in this directory.")
endif()

if(NOT WIN32)
CHECK_INCLUDE_FILES(ifaddrs.h       KVSWEBRTC_HAVE_IFADDRS_H)
if(NOT KVSWEBRTC_HAVE_IFADDRS_H)
  message(FATAL_ERROR "Platform should support the ifaddrs interface.")
endif()
CHECK_FUNCTION_EXISTS(getifaddrs    KVSWEBRTC_HAVE_GETIFADDRS)
if(NOT KVSWEBRTC_HAVE_GETIFADDRS)
  message(FATAL_ERROR "Platform should support getifaddrs API.")
endif()
endif()

set(CMAKE_MACOSX_RPATH TRUE)
get_filename_component(ROOT "${CMAKE_CURRENT_SOURCE_DIR}" ABSOLUTE)

if (LINK_PROFILER)
  add_link_options("-Wl,--no-as-needed")
endif()


# static settings
if(BUILD_STATIC_LIBS OR WIN32)
  set(LINKAGE STATIC)
  # Force CMake to find static libs
  if(WIN32)
    SET(CMAKE_FIND_LIBRARY_SUFFIXES .lib .a ${CMAKE_FIND_LIBRARY_SUFFIXES})
  else()
    SET(CMAKE_FIND_LIBRARY_SUFFIXES .a ${CMAKE_FIND_LIBRARY_SUFFIXES})
  endif()
else()
  set(LINKAGE SHARED)
endif()

set(KINESIS_VIDEO_WEBRTC_CLIENT_SRC "${CMAKE_CURRENT_SOURCE_DIR}")

if(ADD_MUCLIBC)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -muclibc")
endif()


message(STATUS "Kinesis Video WebRTC Client path is ${KINESIS_VIDEO_WEBRTC_CLIENT_SRC}")
message(STATUS "dependencies install path is ${OPEN_SRC_INSTALL_PREFIX}")

# pass ca cert location to sdk
add_definitions(-DKVS_CA_CERT_PATH="${CMAKE_SOURCE_DIR}/certs/cert.pem")
add_definitions(-DCMAKE_DETECTED_CACERT_PATH)

if (ENABLE_KVS_THREADPOOL)
  add_definitions(-DENABLE_KVS_THREADPOOL)
endif()

if(USE_OPENSSL)
  add_definitions(-DKVS_USE_OPENSSL)
elseif(USE_MBEDTLS)
  add_definitions(-DKVS_USE_MBEDTLS)
  # FIXME: there's probably a better way to inject MBEDTLS_USER_CONFIG_FILE flag without mutating the global CMAKE_C_FLAGS and CMAKE_CXX_FLAGS
  if("${CMAKE_C_COMPILER_ID}" MATCHES "Clang")
    message(STATUS "Detected clang")
    set(CMAKE_C_FLAGS "-I${CMAKE_CURRENT_SOURCE_DIR}/configs -DMBEDTLS_USER_CONFIG_FILE=\"<config_mbedtls.h>\" ${CMAKE_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "-I${CMAKE_CURRENT_SOURCE_DIR}/configs -DMBEDTLS_USER_CONFIG_FILE=\"<config_mbedtls.h>\" ${CMAKE_CXX_FLAGS}")
  elseif("${CMAKE_C_COMPILER_ID}" MATCHES "GNU")
    message(STATUS "Detected gcc")
    set(CMAKE_C_FLAGS "-I${CMAKE_CURRENT_SOURCE_DIR}/configs -DMBEDTLS_USER_CONFIG_FILE=\"<config_mbedtls.h>\" ${CMAKE_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "-I${CMAKE_CURRENT_SOURCE_DIR}/configs -DMBEDTLS_USER_CONFIG_FILE=\"<config_mbedtls.h>\" ${CMAKE_CXX_FLAGS}")
    if(CMAKE_C_COMPILER_VERSION VERSION_GREATER "7.0")
      set(CMAKE_C_FLAGS "-Wno-error=stringop-overflow ${CMAKE_C_FLAGS}")
      set(CMAKE_CXX_FLAGS "-Wno-error=stringop-overflow ${CMAKE_CXX_FLAGS}")
    endif()
  else()
    message(FATAL_ERROR "Detected unsupported compiler: ${CMAKE_C_COMPILER_ID}")
  endif()
endif()


if(BUILD_DEPENDENCIES)
  if(NOT EXISTS ${OPEN_SRC_INSTALL_PREFIX})
    file(MAKE_DIRECTORY ${OPEN_SRC_INSTALL_PREFIX})
  endif()

  set(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:${OPEN_SRC_INSTALL_PREFIX}/lib/pkgconfig")
  set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} ${OPEN_SRC_INSTALL_PREFIX})

  message(STATUS "Begin building dependencies.")

  set(SSL_OPTIONS USE_OPENSSL USE_MBEDTLS)
  count_true(ENABLED_SSL_OPTION_COUNT ${SSL_OPTIONS})

  if(ENABLED_SSL_OPTION_COUNT GREATER "1")
    message(FATAL_ERROR "Only one of ${SSL_OPTIONS} can be enabled")
  endif()

  if (USE_OPENSSL)
    set(BUILD_ARGS -DBUILD_STATIC_LIBS=${BUILD_STATIC_LIBS}
                   -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
                   -DBUILD_OPENSSL_PLATFORM=${BUILD_OPENSSL_PLATFORM}
                   -DOPENSSL_EXTRA=${OPENSSL_EXTRA})
    build_dependency(openssl ${BUILD_ARGS})
    set(OPENSSL_ROOT_DIR ${OPEN_SRC_INSTALL_PREFIX})
  elseif(USE_MBEDTLS)
    set(BUILD_ARGS -DBUILD_STATIC_LIBS=${BUILD_STATIC_LIBS}
                   -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
                    "-DCMAKE_C_FLAGS=${CMAKE_C_FLAGS} -std=c99")
      build_dependency(mbedtls ${BUILD_ARGS})
  endif()


  if(WIN32)
    set(OPENSSL_INCLUDE_DIRS "${OPEN_SRC_INSTALL_PREFIX}/include/")
    set(OPENSSL_CRYPTO_LIBRARY "${OPEN_SRC_INSTALL_PREFIX}/lib/libcrypto.lib")
    set(OPENSSL_SSL_LIBRARY "${OPEN_SRC_INSTALL_PREFIX}/lib/libssl.lib")
    set(OPENSSL_LIBRARIES ${OPENSSL_SSL_LIBRARY} ${OPENSSL_CRYPTO_LIBRARY})
    string(REPLACE ";" "|" OPENSSL_LIBRARIES_ALT_SEP "${OPENSSL_LIBRARIES}")
    set(BUILD_ARGS -DBUILD_STATIC_LIBS=${BUILD_STATIC_LIBS}
                  -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
                  -DUSE_OPENSSL=${USE_OPENSSL}
                  -DUSE_MBEDTLS=${USE_MBEDTLS}
                  -DLWS_EXT_PTHREAD_INCLUDE_DIR=${EXT_PTHREAD_INCLUDE_DIR}
                  -DLWS_EXT_PTHREAD_LIBRARIES=${EXT_PTHREAD_LIBRARIES}
                  -DLWS_OPENSSL_INCLUDE_DIRS=${OPENSSL_INCLUDE_DIRS}
                  -DLWS_OPENSSL_LIBRARIES=${OPENSSL_LIBRARIES_ALT_SEP}
                  -DCMAKE_C_FLAGS=${CMAKE_C_FLAGS})
  else()
    set(BUILD_ARGS -DBUILD_STATIC_LIBS=${BUILD_STATIC_LIBS}
                  -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
                  -DOPENSSL_DIR=${OPEN_SRC_INSTALL_PREFIX}
                  -DUSE_OPENSSL=${USE_OPENSSL}
                  -DUSE_MBEDTLS=${USE_MBEDTLS}
                  -DLWS_OPENSSL_CRYPTO_LIBRARY=${OPENSSL_CRYPTO_LIBRARY}
                  -DLWS_OPENSSL_SSL_LIBRARY=${OPENSSL_SSL_LIBRARY}
                  -DLWS_OPENSSL_INCLUDE_DIRS=${OPENSSL_INCLUDE_DIR}
                  -DCMAKE_C_FLAGS=${CMAKE_C_FLAGS})
  endif()
  build_dependency(websockets ${BUILD_ARGS})

  set(BUILD_ARGS
      -DBUILD_STATIC_LIBS=${BUILD_STATIC_LIBS}
      -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
      -DOPENSSL_DIR=${OPEN_SRC_INSTALL_PREFIX}
      -DBUILD_LIBSRTP_HOST_PLATFORM=${BUILD_LIBSRTP_HOST_PLATFORM}
      -DBUILD_LIBSRTP_DESTINATION_PLATFORM=${BUILD_LIBSRTP_DESTINATION_PLATFORM}
      -DUSE_OPENSSL=${USE_OPENSSL}
      -DUSE_MBEDTLS=${USE_MBEDTLS}
      -DCMAKE_C_FLAGS=${CMAKE_C_FLAGS})

  build_dependency(srtp ${BUILD_ARGS})

  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -I${CMAKE_CURRENT_SOURCE_DIR}/configs")
  set(BUILD_ARGS
      -DBUILD_STATIC_LIBS=${BUILD_STATIC_LIBS}
      -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
      -DCMAKE_C_FLAGS=${CMAKE_C_FLAGS})

  build_dependency(kvssdp ${BUILD_ARGS})

  set(BUILD_ARGS
      -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
      -DCMAKE_C_FLAGS=${CMAKE_C_FLAGS})

  build_dependency(usrsctp ${BUILD_ARGS})


  if(BUILD_TEST)
    build_dependency(gtest)

    if(ENABLE_AWS_SDK_IN_TESTS)
      build_dependency(awscpp)
    endif()

  endif()

  if(BUILD_BENCHMARK)
    build_dependency(benchmark)
  endif()

  if (LINK_PROFILER)
    build_dependency(gperftools)
  endif()
  message(STATUS "Finished building dependencies.")
endif()

# building kvsCommonLws also builds kvspic
set(BUILD_ARGS
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DUSE_OPENSSL=${USE_OPENSSL}
        -DUSE_MBEDTLS=${USE_MBEDTLS}
        -DKVS_DEFAULT_STACK_SIZE=${KVS_DEFAULT_STACK_SIZE}
        -DCMAKE_C_FLAGS=${CMAKE_C_FLAGS})
build_dependency(kvsCommonLws ${BUILD_ARGS})

############# find dependent libraries ############

find_package(Threads)
find_package(PkgConfig REQUIRED)
if (USE_OPENSSL)
  find_package(OpenSSL REQUIRED)
  set(OPEN_SRC_INCLUDE_DIRS ${OPEN_SRC_INCLUDE_DIRS} ${OPENSSL_INCLUDE_DIR})
else()
  find_package(MbedTLS REQUIRED)
  set(OPEN_SRC_INCLUDE_DIRS ${OPEN_SRC_INCLUDE_DIRS} ${MBEDTLS_INCLUDE_DIRS})
endif()

if (OPEN_SRC_INSTALL_PREFIX)
  find_library(SRTP_LIBRARIES srtp2 REQUIRED PATHS ${OPEN_SRC_INSTALL_PREFIX})

  if (LINK_PROFILER)
    find_library(GPERFTOOLS_MALLOC_LIBRARIES tcmalloc REQUIRED PATHS ${OPEN_SRC_INSTALL_PREFIX})
    find_library(GPERFTOOLS_PROFILER_LIBRARIES profiler REQUIRED PATHS ${OPEN_SRC_INSTALL_PREFIX})
  endif()
else()
  find_library(SRTP_LIBRARIES srtp2 REQUIRED )
  if (LINK_PROFILER)
    find_library(GPERFTOOLS_MALLOC_LIBRARIES tcmalloc REQUIRED)
    find_library(GPERFTOOLS_PROFILER_LIBRARIES profiler REQUIRED)
  endif()
endif()

if (WIN32)
  SET(LIBWEBSOCKETS_LIBRARIES "websockets.lib")
else()
  pkg_check_modules(LIBWEBSOCKETS REQUIRED libwebsockets)
endif()

# usrsctp dont support pkgconfig yet
find_library(
  Usrsctp
  NAMES ${USRSCTP_LIBNAME} usrsctp REQUIRED
  PATHS ${OPEN_SRC_INSTALL_PREFIX}/lib ${OPEN_SRC_INSTALL_PREFIX}/lib64)

set(OPEN_SRC_INCLUDE_DIRS ${OPEN_SRC_INCLUDE_DIRS} ${LIBSRTP_INCLUDE_DIRS}
                          ${CURL_INCLUDE_DIRS} ${LIBWEBSOCKETS_INCLUDE_DIRS})

link_directories(${LIBSRTP_LIBRARY_DIRS})
link_directories(${LIBWEBSOCKETS_LIBRARY_DIRS})
link_directories(${OPEN_SRC_INSTALL_PREFIX}/lib)

if(EXISTS ${OPEN_SRC_INSTALL_PREFIX}/lib64)
  link_directories(${OPEN_SRC_INSTALL_PREFIX}/lib64)
endif()
############# find dependent libraries end ############

if("${CMAKE_C_COMPILER_ID}" MATCHES "GNU|Clang")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")

  if(CODE_COVERAGE)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -g -fprofile-arcs -ftest-coverage")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
  endif()

  if(ADDRESS_SANITIZER)
    enableSanitizer("address")
  endif()
  if(MEMORY_SANITIZER)
    enableSanitizer("memory")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize-memory-track-origins")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize-memory-track-origins")
  endif()
  if(THREAD_SANITIZER)
    enableSanitizer("thread")
  endif()
  if(UNDEFINED_BEHAVIOR_SANITIZER)
    enableSanitizer("undefined")
  endif()
endif()

# Uncomment below line for debug heap
# add_definitions(-DHEAP_DEBUG)

# Uncomment below to add thread id in log
# add_definitions(-DENABLE_LOG_THREAD_ID)

# Uncomment below line for very verbose logging
# add_definitions(-DLOG_STREAMING)

if (ENABLE_DATA_CHANNEL)
  add_definitions(-DENABLE_DATA_CHANNEL)
  set (DATACHANNEL_SRC
      src/source/PeerConnection/DataChannel.c
      src/source/Sctp/Sctp.c
      )
endif()

if (INSTRUMENTED_ALLOCATORS)
  add_definitions(-DINSTRUMENTED_ALLOCATORS)
endif()

if(ENABLE_AWS_SDK_IN_TESTS)
  add_definitions(-DENABLE_AWS_SDK_IN_TESTS)
endif()

file(
  GLOB
  WEBRTC_CLIENT_SOURCE_FILES
  "src/source/Crypto/*.c"
  "src/source/Ice/*.c"
  "src/source/PeerConnection/JitterBuffer.c"
  "src/source/PeerConnection/jsmn.c"
  "src/source/PeerConnection/PeerConnection.c"
  "src/source/PeerConnection/Retransmitter.c"
  "src/source/PeerConnection/Rtcp.c"
  "src/source/PeerConnection/Rtp.c"
  "src/source/PeerConnection/SessionDescription.c"
  "src/source/Rtcp/*.c"
  "src/source/Rtp/*.c"
  "src/source/Rtp/Codecs/*.c"
  "src/source/Sdp/*.c"
  "src/source/Srtp/*.c"
  "src/source/Stun/*.c"
  "src/source/Metrics/*.c")

if (USE_OPENSSL)
  list(FILTER WEBRTC_CLIENT_SOURCE_FILES EXCLUDE REGEX ".*_mbedtls\\.c")
elseif (USE_MBEDTLS)
  list(FILTER WEBRTC_CLIENT_SOURCE_FILES EXCLUDE REGEX ".*_openssl\\.c")
endif()

file(GLOB WEBRTC_SIGNALING_CLIENT_SOURCE_FILES "src/source/Signaling/*.c")

include_directories(${OPEN_SRC_INCLUDE_DIRS})
include_directories(${OPEN_SRC_INSTALL_PREFIX}/include)
include_directories(${KINESIS_VIDEO_WEBRTC_CLIENT_SRC}/src/include)

add_library(kvsWebrtcClient ${LINKAGE} ${WEBRTC_CLIENT_SOURCE_FILES} ${DATACHANNEL_SRC})

if(USE_MBEDTLS)
  target_compile_definitions(kvsWebrtcClient PRIVATE LWS_WITH_MBEDTLS)
endif()

if(ENABLE_KVS_THREADPOOL)
  file(GLOB THREADPOOL_SOURCE_FILES "src/source/Threadpool/*.c")
  add_library(kvsWebRtcThreadpool ${LINKAGE} ${THREADPOOL_SOURCE_FILES})
  target_link_libraries(kvsWebRtcThreadpool PRIVATE kvspicUtils)
  set(EXTRA_DEPS ${EXTRA_DEPS} kvsWebRtcThreadpool)
endif()

target_link_libraries(
  kvsWebrtcClient
  PRIVATE kvspicUtils
          kvspicState
          kvssdp
          ${CMAKE_THREAD_LIBS_INIT}
          ${OPENSSL_SSL_LIBRARY}
          ${OPENSSL_CRYPTO_LIBRARY}
          ${SRTP_LIBRARIES}
          ${Usrsctp}
          ${MBEDTLS_LIBRARIES}
          ${GPERFTOOLS_MALLOC_LIBRARIES}
          ${GPERFTOOLS_PROFILER_LIBRARIES}
          ${EXTRA_DEPS})

add_library(kvsWebrtcSignalingClient ${LINKAGE} ${WEBRTC_SIGNALING_CLIENT_SOURCE_FILES})

if(USE_MBEDTLS)
  target_compile_definitions(kvsWebrtcSignalingClient PRIVATE LWS_WITH_MBEDTLS)
endif()

target_link_libraries(
  kvsWebrtcSignalingClient
  PUBLIC
        kvsCommonLws
        ${LIBWEBSOCKETS_LIBRARIES}
        kvssdp
  PRIVATE kvspicUtils
         kvspicState
         ${CMAKE_THREAD_LIBS_INIT}
         ${EXTRA_DEPS}
         ${OPENSSL_SSL_LIBRARY}
         ${OPENSSL_CRYPTO_LIBRARY}
         ${GPERFTOOLS_MALLOC_LIBRARIES}
         ${GPERFTOOLS_PROFILER_LIBRARIES}
         ${MBEDTLS_LIBRARIES})

if (WIN32)
  target_link_libraries(kvsWebrtcClient PRIVATE "Ws2_32" "iphlpapi")
endif()

if(COMPILER_WARNINGS)
  target_compile_options(kvsWebrtcClient PUBLIC -Wall -Werror -pedantic -Wextra -Wno-unknown-warning-option)
  target_compile_options(kvsWebrtcSignalingClient PUBLIC -Wall -Werror -pedantic -Wextra -Wno-unknown-warning-option)
endif()

install(TARGETS kvsWebrtcClient kvsWebrtcSignalingClient
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)

install(DIRECTORY ${KINESIS_VIDEO_WEBRTC_CLIENT_SRC}/src/include/
  DESTINATION include
)

if (BUILD_SAMPLE)
  # copy sample frames to build folder, in case developer runs sample program with command `samples/kvsWebrtcClientMaster` from `build` dir.
  file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/samples/opusSampleFrames" DESTINATION .)
  file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/samples/h264SampleFrames" DESTINATION .)
  file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/samples/aacSampleFrames" DESTINATION .)
  file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/samples/h265SampleFrames" DESTINATION .)

  add_subdirectory(samples)
endif()

if(BUILD_TEST)
  # adding ZLIB because aws sdk static link seems to be broken when zlib is needed
  if(NOT WIN32)
    find_package(ZLIB REQUIRED)
  endif()
  add_subdirectory(tst)
endif()

if(BUILD_BENCHMARK)
  add_subdirectory(bench)
endif()

get_directory_property(clean_files ADDITIONAL_CLEAN_FILES)
list(APPEND clean_files "${OPEN_SRC_INSTALL_PREFIX}")
list(APPEND clean_files "${CMAKE_CURRENT_SOURCE_DIR}/build")
set_directory_properties(PROPERTIES ADDITIONAL_CLEAN_FILES "${clean_files}")

