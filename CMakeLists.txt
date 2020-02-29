cmake_minimum_required(VERSION 2.8)
project(KinesisVideoWebRTCClient LANGUAGES C)

# User Flags
option(ADD_MUCLIBC "Add -muclibc c flag" OFF)
option(BUILD_DEPENDENCIES "Whether or not to build depending libraries from source" ON)
option(BUILD_OPENSSL "If building dependencies, whether or not building openssl from source" OFF)
option(BUILD_OPENSSL_PLATFORM "If buildng OpenSSL what is the target platform" OFF)
option(BUILD_LIBSRTP_HOST_PLATFORM "If buildng LibSRTP what is the current platform" OFF)
option(BUILD_LIBSRTP_DESTINATION_PLATFORM "If buildng LibSRTP what is the destination platform" OFF)

# Developer Flags
option(BUILD_TEST "Build the testing tree." OFF)
option(CODE_COVERAGE "Enable coverage reporting" OFF)
option(COMPILER_WARNINGS "Enable all compiler warnings." OFF)
option(ADDRESS_SANITIZER "Build with AddressSanitizer." OFF)
option(MEMORY_SANITIZER "Build with MemorySanitizer." OFF)
option(THREAD_SANITIZER "Build with ThreadSanitizer." OFF)
option(UNDEFINED_BEHAVIOR_SANITIZER "Build with UndefinedBehaviorSanitizer." OFF)

set(CMAKE_MACOSX_RPATH TRUE)
get_filename_component(ROOT "${CMAKE_CURRENT_SOURCE_DIR}" ABSOLUTE)

if(NOT KINESIS_VIDEO_PIC_SRC)
  if(DEFINED ENV{KINESIS_VIDEO_PIC_SRC})
    set(KINESIS_VIDEO_PIC_SRC $ENV{KINESIS_VIDEO_PIC_SRC})
  else()
    set(KINESIS_VIDEO_PIC_SRC
        "${CMAKE_CURRENT_SOURCE_DIR}/open-source/amazon-kinesis-video-streams-pic/")
  endif()
endif()

if(NOT KINESIS_VIDEO_PRODUCER_C_SRC)
  if(DEFINED ENV{KINESIS_VIDEO_PRODUCER_C_SRC})
    set(KINESIS_VIDEO_PRODUCER_C_SRC $ENV{KINESIS_VIDEO_PRODUCER_C_SRC})
  else()
    set(KINESIS_VIDEO_PRODUCER_C_SRC
        "${CMAKE_CURRENT_SOURCE_DIR}/open-source/amazon-kinesis-video-streams-producer-c/")
  endif()
endif()

if(NOT KINESIS_VIDEO_WEBRTC_CLIENT_SRC)
  if(DEFINED ENV{KINESIS_VIDEO_WEBRTC_CLIENT_SRC})
    set(KINESIS_VIDEO_WEBRTC_CLIENT_SRC $ENV{KINESIS_VIDEO_WEBRTC_CLIENT_SRC})
  else()
    set(KINESIS_VIDEO_WEBRTC_CLIENT_SRC "${CMAKE_SOURCE_DIR}")
  endif()
endif()

# expecting libjsmn, libwebsockets and gtest to be in
# ${KINESIS_VIDEO_OPEN_SOURCE_SRC}/lib and their headers in
# ${KINESIS_VIDEO_OPEN_SOURCE_SRC}/include
if(NOT KINESIS_VIDEO_OPEN_SOURCE_SRC)
  if(DEFINED ENV{KINESIS_VIDEO_OPEN_SOURCE_SRC})
    set(KINESIS_VIDEO_OPEN_SOURCE_SRC $ENV{KINESIS_VIDEO_OPEN_SOURCE_SRC})
  elseif(BUILD_DEPENDENCIES)
    set(KINESIS_VIDEO_OPEN_SOURCE_SRC ${CMAKE_SOURCE_DIR}/open-source/local)
  else()
    set(KINESIS_VIDEO_OPEN_SOURCE_SRC
        "${CMAKE_CURRENT_SOURCE_DIR}/open-source/downloads/local"
    )
  endif()
endif()

message(STATUS "Kinesis Video PIC path is ${KINESIS_VIDEO_PIC_SRC}")
message(
  STATUS "Kinesis Video C Producer path is ${KINESIS_VIDEO_PRODUCER_C_SRC}")
message(
  STATUS
    "Kinesis Video WebRTC Client path is ${KINESIS_VIDEO_WEBRTC_CLIENT_SRC}")
message(
  STATUS "dependencies install path is ${KINESIS_VIDEO_OPEN_SOURCE_SRC}")

if(NOT EXISTS ${KINESIS_VIDEO_PIC_SRC})
  message(
    FATAL_ERROR
      "KINESIS_VIDEO_PIC_SRC ${KINESIS_VIDEO_PIC_SRC} does not exist")
elseif(NOT EXISTS ${KINESIS_VIDEO_WEBRTC_CLIENT_SRC})
  message(
    FATAL_ERROR
      "KINESIS_VIDEO_WEBRTC_CLIENT_SRC ${KINESIS_VIDEO_WEBRTC_CLIENT_SRC} does not exist"
  )
elseif(NOT EXISTS ${KINESIS_VIDEO_PRODUCER_C_SRC})
  message(
    FATAL_ERROR
      "KINESIS_VIDEO_PRODUCER_C_SRC ${KINESIS_VIDEO_PRODUCER_C_SRC} does not exist"
  )
endif()

# pass ca cert location to sdk
add_definitions(-DKVS_CA_CERT_PATH="${CMAKE_SOURCE_DIR}/certs/cert.pem")
add_definitions(-DCMAKE_DETECTED_CACERT_PATH)

if(BUILD_DEPENDENCIES)
  set(OPEN_SOURCE_DIR ${CMAKE_SOURCE_DIR}/open-source)

  if(NOT EXISTS ${OPEN_SOURCE_DIR})
    file(MAKE_DIRECTORY ${OPEN_SOURCE_DIR}/local)
  endif()

  set(OPEN_SRC_INSTALL_PREFIX ${CMAKE_SOURCE_DIR}/open-source/local)

  if(BUILD_OPENSSL)
    if(NOT EXISTS ${OPEN_SOURCE_DIR}/libopenssl)
      message(STATUS "Rebuilding libraries that depend on openssl")
      file(REMOVE_RECURSE ${OPEN_SOURCE_DIR}/libwebsockets)
      file(REMOVE_RECURSE ${OPEN_SOURCE_DIR}/libsrtp)

      # build libwebsockets
      configure_file(
              ${CMAKE_SOURCE_DIR}/cmake-scripts/libopenssl-CMakeLists.txt
              ${OPEN_SOURCE_DIR}/libopenssl/CMakeLists.txt COPYONLY)
      execute_process(
              COMMAND ${CMAKE_COMMAND} -DBUILD_OPENSSL_PLATFORM=${BUILD_OPENSSL_PLATFORM} -DOPEN_SRC_INSTALL_PREFIX=${OPEN_SRC_INSTALL_PREFIX} -G ${CMAKE_GENERATOR} .
              RESULT_VARIABLE result
              WORKING_DIRECTORY ${OPEN_SOURCE_DIR}/libopenssl)
      if(result)
        message(FATAL_ERROR "CMake step for libopenssl failed: ${result}")
      endif()
      execute_process(
              COMMAND ${CMAKE_COMMAND} --build .
              RESULT_VARIABLE result
              WORKING_DIRECTORY ${OPEN_SOURCE_DIR}/libopenssl)
      if(result)
        message(FATAL_ERROR "CMake step for libopenssl failed: ${result}")
      endif()
    endif()
    set(OPENSSL_ROOT_DIR ${OPEN_SRC_INSTALL_PREFIX})

  else()
    if(EXISTS ${OPEN_SOURCE_DIR}/libopenssl)
      # delete openssl built from source otherwise build wont work
      file(REMOVE_RECURSE ${OPEN_SOURCE_DIR}/libopenssl)
      message(STATUS "Rebuilding libraries that depend on openssl")
      file(REMOVE_RECURSE ${OPEN_SOURCE_DIR}/libwebsockets)
      file(REMOVE_RECURSE ${OPEN_SOURCE_DIR}/libsrtp)
    endif()

    if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
      set(OPENSSL_ROOT_DIR "/usr/local/opt/openssl")
    endif()
  endif()

  find_package(OpenSSL REQUIRED)
  # get openssl dir and pass it to libwebsocket and libsrtp
  get_filename_component(OPENSSL_DIR ${OPENSSL_INCLUDE_DIR} DIRECTORY)

  if(NOT EXISTS ${OPEN_SOURCE_DIR}/libwebsockets)
    # build libwebsockets
    configure_file(
      ${CMAKE_SOURCE_DIR}/cmake-scripts/libwebsockets-CMakeLists.txt
      ${OPEN_SOURCE_DIR}/libwebsockets/CMakeLists.txt COPYONLY)
    execute_process(
      COMMAND ${CMAKE_COMMAND} -DOPENSSL_DIR=${OPENSSL_DIR} -G ${CMAKE_GENERATOR} .
      RESULT_VARIABLE result
      WORKING_DIRECTORY ${OPEN_SOURCE_DIR}/libwebsockets)
    if(result)
      message(FATAL_ERROR "CMake step for libwebsockets failed: ${result}")
    endif()
    execute_process(
      COMMAND ${CMAKE_COMMAND} --build .
      RESULT_VARIABLE result
      WORKING_DIRECTORY ${OPEN_SOURCE_DIR}/libwebsockets)
    if(result)
      message(FATAL_ERROR "CMake step for libwebsockets failed: ${result}")
    endif()
  endif()

  if(NOT EXISTS ${OPEN_SOURCE_DIR}/libsrtp)
    # build libsrtp
    configure_file(${CMAKE_SOURCE_DIR}/cmake-scripts/libsrtp-CMakeLists.txt
                   ${OPEN_SOURCE_DIR}/libsrtp/CMakeLists.txt COPYONLY)
    execute_process(
      COMMAND ${CMAKE_COMMAND} -DOPENSSL_DIR=${OPENSSL_DIR} -DBUILD_LIBSRTP_HOST_PLATFORM=${BUILD_LIBSRTP_HOST_PLATFORM} -DBUILD_LIBSRTP_DESTINATION_PLATFORM=${BUILD_LIBSRTP_DESTINATION_PLATFORM} -DOPEN_SRC_INSTALL_PREFIX=${OPEN_SRC_INSTALL_PREFIX} -G ${CMAKE_GENERATOR} .
      RESULT_VARIABLE result
      WORKING_DIRECTORY ${OPEN_SOURCE_DIR}/libsrtp)
    if(result)
      message(FATAL_ERROR "CMake step for libsrtp failed: ${result}")
    endif()
    execute_process(
      COMMAND ${CMAKE_COMMAND} --build .
      RESULT_VARIABLE result
      WORKING_DIRECTORY ${OPEN_SOURCE_DIR}/libsrtp)
    if(result)
      message(FATAL_ERROR "CMake step for libsrtp failed: ${result}")
    endif()
  endif()

  if(NOT EXISTS ${OPEN_SOURCE_DIR}/libusrsctp)
    # build libusrsctp
    configure_file(${CMAKE_SOURCE_DIR}/cmake-scripts/libusrsctp-CMakeLists.txt
                   ${OPEN_SOURCE_DIR}/libusrsctp/CMakeLists.txt COPYONLY)
    execute_process(
      COMMAND ${CMAKE_COMMAND} -G ${CMAKE_GENERATOR} .
      RESULT_VARIABLE result
      WORKING_DIRECTORY ${OPEN_SOURCE_DIR}/libusrsctp)
    if(result)
      message(FATAL_ERROR "CMake step for libusrsctp failed: ${result}")
    endif()
    execute_process(
      COMMAND ${CMAKE_COMMAND} --build .
      RESULT_VARIABLE result
      WORKING_DIRECTORY ${OPEN_SOURCE_DIR}/libusrsctp)
    if(result)
      message(FATAL_ERROR "CMake step for libusrsctp failed: ${result}")
    endif()
  endif()

  if(NOT EXISTS ${OPEN_SOURCE_DIR}/libjsmn)
    # build libjsmn
    configure_file(${CMAKE_SOURCE_DIR}/cmake-scripts/libjsmn-CMakeLists.txt
                   ${OPEN_SOURCE_DIR}/libjsmn/CMakeLists.txt COPYONLY)
    execute_process(
      COMMAND ${CMAKE_COMMAND} -G ${CMAKE_GENERATOR} .
      RESULT_VARIABLE result
      WORKING_DIRECTORY ${OPEN_SOURCE_DIR}/libjsmn)
    if(result)
      message(FATAL_ERROR "CMake step for libjsmn failed: ${result}")
    endif()
    execute_process(
      COMMAND ${CMAKE_COMMAND} --build .
      RESULT_VARIABLE result
      WORKING_DIRECTORY ${OPEN_SOURCE_DIR}/libjsmn)
    if(result)
      message(FATAL_ERROR "CMake step for libjsmn failed: ${result}")
    endif()
  endif()

  if(NOT EXISTS ${OPEN_SOURCE_DIR}/libgtest)
    # build gtest
    configure_file(${CMAKE_SOURCE_DIR}/cmake-scripts/libgtest-CMakeLists.txt
                   ${OPEN_SOURCE_DIR}/libgtest/CMakeLists.txt COPYONLY)
    execute_process(
      COMMAND ${CMAKE_COMMAND} -G ${CMAKE_GENERATOR} .
      RESULT_VARIABLE result
      WORKING_DIRECTORY ${OPEN_SOURCE_DIR}/libgtest)
    if(result)
      message(FATAL_ERROR "CMake step for libgtest failed: ${result}")
    endif()
    execute_process(
      COMMAND ${CMAKE_COMMAND} --build .
      RESULT_VARIABLE result
      WORKING_DIRECTORY ${OPEN_SOURCE_DIR}/libgtest)
    if(result)
      message(FATAL_ERROR "CMake step for libgtest failed: ${result}")
    endif()
  endif()
endif()

if(NOT EXISTS ${KINESIS_VIDEO_OPEN_SOURCE_SRC})
  set(DEPENDENCY_DIR_NOT_FOUND_MSG
      "KINESIS_VIDEO_OPEN_SOURCE_SRC ${KINESIS_VIDEO_OPEN_SOURCE_SRC} does not exist."
  )
  if(BUILD_DEPENDENCIES)
    set(DEPENDENCY_DIR_NOT_FOUND_MSG
        "${DEPENDENCY_DIR_NOT_FOUND_MSG} Try delete ${KINESIS_VIDEO_WEBRTC_CLIENT_SRC}/open-source and run \"cmake -DBUILD_DEPENDENCIES=TRUE .\" again"
    )
  endif()
  message(FATAL_ERROR "${DEPENDENCY_DIR_NOT_FOUND_MSG}")
endif()

if(CMAKE_SIZEOF_VOID_P STREQUAL 4)
  message(STATUS "Bitness 32 bits")
  set(KINESIS_VIDEO_BUILD_BITNESS "x86")
  set(KINESIS_VIDEO_BUILD_BITNESS_TYPE "Win32")
  set(KINESIS_VIDEO_BUILD_BITNESS_NAME "x86")
elseif(CMAKE_SIZEOF_VOID_P STREQUAL 8)
  message(STATUS "Bitness 64 bits")
  set(KINESIS_VIDEO_BUILD_BITNESS "x64")
  set(KINESIS_VIDEO_BUILD_BITNESS_TYPE "x64")
  set(KINESIS_VIDEO_BUILD_BITNESS_NAME "x86_64")
else()
  message(FATAL_ERROR "Unknown bitness")
endif()

function(enableSanitizer SANITIZER)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -g -fsanitize=${SANITIZER} -fno-omit-frame-pointer" PARENT_SCOPE)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -g -fsanitize=${SANITIZER} -fno-omit-frame-pointer -fno-optimize-sibling-calls" PARENT_SCOPE)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=${SANITIZER}" PARENT_SCOPE)
endfunction()

if("${CMAKE_C_COMPILER_ID}" MATCHES "GNU|Clang")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")

  if(ADD_MUCLIBC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -muclibc")
  endif()

  if(CODE_COVERAGE)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -g -fprofile-arcs -ftest-coverage")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
  endif()

  if(ADDRESS_SANITIZER)
    enableSanitizer("address")
  endif()
  if(MEMORY_SANITIZER)
    enableSanitizer("memory")
  endif()
  if(THREAD_SANITIZER)
    enableSanitizer("thread")
  endif()
  if(UNDEFINED_BEHAVIOR_SANITIZER)
    enableSanitizer("undefined")
  endif()

  find_package(Threads)
  find_package(PkgConfig REQUIRED)
  set(ENV{PKG_CONFIG_PATH}
      "$ENV{PKG_CONFIG_PATH}:${KINESIS_VIDEO_OPEN_SOURCE_SRC}/lib/pkgconfig")
  pkg_check_modules(LIBWEBSOCKET REQUIRED libwebsockets)
  pkg_check_modules(LIBSRTP REQUIRED libsrtp2)

  # usrsctp dont support pkgconfig yet
  find_library(
    Usrsctp
    NAMES libusrsctp.${CMAKE_SHARED_LIBRARY_SUFFIX} usrsctp REQUIRED
    PATHS ${KINESIS_VIDEO_OPEN_SOURCE_SRC}/lib
    NO_DEFAULT_PATH)

  set(OPEN_SRC_INCLUDE_DIRS ${LIBSRTP_INCLUDE_DIRS} ${OPENSSL_INCLUDE_DIR}
                            ${CURL_INCLUDE_DIRS} ${LIBWEBSOCKET_INCLUDE_DIRS})

  link_directories(${LIBSRTP_LIBRARY_DIRS})
  link_directories(${LIBWEBSOCKET_LIBRARY_DIRS})

  if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(ENV{PKG_CONFIG_PATH}
        "$ENV{PKG_CONFIG_PATH}:/usr/local/opt/libffi/lib/pkgconfig")
    find_library(Intl "intl" REQUIRED PATHS "/usr/local/opt/gettext/lib")
    link_directories("/usr/local/opt/gettext/lib")
  endif()

  pkg_check_modules(GST gstreamer-1.0)
  if(GST_FOUND)
    pkg_check_modules(GLIB2 REQUIRED glib-2.0)
    pkg_check_modules(GST_APP REQUIRED gstreamer-app-1.0)
    pkg_check_modules(GOBJ2 REQUIRED gobject-2.0)
    message("gstreamer found. Will build gstreamer samples")

    set(OPEN_SRC_INCLUDE_DIRS
        ${OPEN_SRC_INCLUDE_DIRS} ${GLIB2_INCLUDE_DIRS} ${GST_INCLUDE_DIRS}
        ${GST_APP_INCLUDE_DIRS} ${GOBJ2_INCLUDE_DIRS})

    set(GST_SAMPLE_LIBRARIES ${GLIB2_LIBRARIES} ${GST_LIBRARIES}
                             ${GST_APP_LIBRARIES} ${GOBJ2_LIBRARIES} ${Intl})

    link_directories(${GLIB2_LIBRARY_DIRS})
    link_directories(${GST_LIBRARY_DIRS})
    link_directories(${GST_APP_LIBRARY_DIRS})
    link_directories(${GOBJ2_LIBRARY_DIRS})

  else()
    message("gstreamer not found. Will not build gstreamer samples")
  endif()
endif()

find_library(
  Jsmn
  NAMES jsmn REQUIRED
  PATHS ${KINESIS_VIDEO_OPEN_SOURCE_SRC}/lib
  NO_DEFAULT_PATH)

# Uncomment below line for debug heap
# add_definitions(-DHEAP_DEBUG)

# Uncomment below to add thread id in log
# add_definitions(-DENABLE_LOG_THREAD_ID)

# Uncomment below line for very verbose logging
# add_definitions(-DLOG_STREAMING)

file(
  GLOB
  WEBRTC_CLIENT_SOURCE_FILES
  "src/source/Dtls/*.c"
  "src/source/Ice/*.c"
  "src/source/PeerConnection/*.c"
  "src/source/Rtcp/*.c"
  "src/source/Rtp/*.c"
  "src/source/Rtp/Codecs/*.c"
  "src/source/Sdp/*.c"
  "src/source/Srtp/*.c"
  "src/source/Stun/*.c"
  "src/source/Sctp/*.c")

file(GLOB WEBRTC_SIGNALING_CLIENT_SOURCE_FILES "src/source/Signaling/*.c")

file(GLOB PIC_HEADERS "${KINESIS_VIDEO_PIC_SRC}/src/*/include")

file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/samples/opusSampleFrames" DESTINATION .)
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/samples/h264SampleFrames" DESTINATION .)

include_directories(${PIC_HEADERS})
include_directories(${OPEN_SRC_INCLUDE_DIRS})
include_directories(${KINESIS_VIDEO_OPEN_SOURCE_SRC}/include)
include_directories(${KINESIS_VIDEO_WEBRTC_CLIENT_SRC}/src/include)
include_directories(${KINESIS_VIDEO_WEBRTC_CLIENT_SRC}/src/ice)
include_directories(${KINESIS_VIDEO_PRODUCER_C_SRC}/src/include)

set(BUILD_COMMON_LWS TRUE CACHE BOOL "Build ProducerC with LWS Support" FORCE)
set(BUILD_COMMON_CURL FALSE CACHE BOOL "Build ProducerC without CURL Support" FORCE)
add_subdirectory(${KINESIS_VIDEO_PRODUCER_C_SRC} ./kvscproducer EXCLUDE_FROM_ALL)

add_library(kvsWebrtcClient SHARED ${WEBRTC_CLIENT_SOURCE_FILES})

target_link_libraries(
  kvsWebrtcClient
  kvspicUtils
  kvspicClient
  kvspicState
  ${OPENSSL_SSL_LIBRARY}
  ${OPENSSL_CRYPTO_LIBRARY}
  ${LIBSRTP_LIBRARIES}
  ${Jsmn}
  ${Usrsctp})

add_library(kvsWebrtcSignalingClient SHARED
            ${WEBRTC_SIGNALING_CLIENT_SOURCE_FILES})

target_link_libraries(kvsWebrtcSignalingClient kvspicUtils kvspicState
                      kvspicClient kvsCommonLws)

add_executable(
  kvsWebrtcClientMaster
  ${KINESIS_VIDEO_WEBRTC_CLIENT_SRC}/samples/Common.c
  ${KINESIS_VIDEO_WEBRTC_CLIENT_SRC}/samples/kvsWebRTCClientMaster.c)
target_link_libraries(kvsWebrtcClientMaster kvsWebrtcClient
                      kvsWebrtcSignalingClient)

add_executable(
  kvsWebrtcClientViewer
  ${KINESIS_VIDEO_WEBRTC_CLIENT_SRC}/samples/Common.c
  ${KINESIS_VIDEO_WEBRTC_CLIENT_SRC}/samples/kvsWebRTCClientViewer.c)
target_link_libraries(kvsWebrtcClientViewer kvsWebrtcClient
                      kvsWebrtcSignalingClient)

if(COMPILER_WARNINGS)
  target_compile_options(kvsWebrtcClient PUBLIC -Wall -Werror -pedantic -Wextra -Wno-unknown-warning-option)
  target_compile_options(kvsWebrtcSignalingClient PUBLIC -Wall -Werror -pedantic -Wextra -Wno-unknown-warning-option)
endif()

if(GST_FOUND)
  add_executable(
    kvsWebrtcClientMasterGstSample
    ${KINESIS_VIDEO_WEBRTC_CLIENT_SRC}/samples/Common.c
    ${KINESIS_VIDEO_WEBRTC_CLIENT_SRC}/samples/kvsWebRTCClientMasterGstreamerSample.c
  )
  target_link_libraries(kvsWebrtcClientMasterGstSample kvsWebrtcClient
                        kvsWebrtcSignalingClient ${GST_SAMPLE_LIBRARIES})
endif()

if(BUILD_TEST)
  set(ENV{KVS_GTEST_ROOT} ${KINESIS_VIDEO_OPEN_SOURCE_SRC})
  add_subdirectory(tst)
endif()
