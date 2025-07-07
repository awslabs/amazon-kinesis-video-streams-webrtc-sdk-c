set(KVS_WEBRTC_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../")

# Define source files
set(kvs_webrtc_srcs
      "${KVS_WEBRTC_ROOT}/src/source/Crypto/Crypto.c"
      "${KVS_WEBRTC_ROOT}/src/source/Crypto/Dtls_mbedtls.c"
      "${KVS_WEBRTC_ROOT}/src/source/Crypto/Dtls.c"
      "${KVS_WEBRTC_ROOT}/src/source/Crypto/IOBuffer.c"
      "${KVS_WEBRTC_ROOT}/src/source/Crypto/Tls_mbedtls.c"
      "${KVS_WEBRTC_ROOT}/src/source/Crypto/Tls.c"
      "${KVS_WEBRTC_ROOT}/src/source/Ice/ConnectionListener.c"
      "${KVS_WEBRTC_ROOT}/src/source/Ice/IceAgent.c"
      "${KVS_WEBRTC_ROOT}/src/source/Ice/IceAgentStateMachine.c"
      "${KVS_WEBRTC_ROOT}/src/source/Ice/IceUtils.c"
      "${KVS_WEBRTC_ROOT}/src/source/Ice/NatBehaviorDiscovery.c"
      "${KVS_WEBRTC_ROOT}/src/source/Ice/Network.c"
      "${KVS_WEBRTC_ROOT}/src/source/Ice/SocketConnection.c"
      "${KVS_WEBRTC_ROOT}/src/source/Ice/TurnConnection.c"
      "${KVS_WEBRTC_ROOT}/src/source/Ice/TurnConnectionStateMachine.c"
      "${KVS_WEBRTC_ROOT}/src/source/Metrics/Metrics.c"
      "${KVS_WEBRTC_ROOT}/src/source/PeerConnection/DataChannel.c"
      "${KVS_WEBRTC_ROOT}/src/source/PeerConnection/JitterBuffer.c"
      "${KVS_WEBRTC_ROOT}/src/source/PeerConnection/jsmn.c"
      "${KVS_WEBRTC_ROOT}/src/source/PeerConnection/PeerConnection.c"
      "${KVS_WEBRTC_ROOT}/src/source/PeerConnection/Retransmitter.c"
      "${KVS_WEBRTC_ROOT}/src/source/PeerConnection/Rtcp.c"
      "${KVS_WEBRTC_ROOT}/src/source/PeerConnection/Rtp.c"
      "${KVS_WEBRTC_ROOT}/src/source/PeerConnection/SessionDescription.c"
      "${KVS_WEBRTC_ROOT}/src/source/Rtcp/RtcpPacket.c"
      "${KVS_WEBRTC_ROOT}/src/source/Rtcp/RollingBuffer.c"
      "${KVS_WEBRTC_ROOT}/src/source/Rtcp/RtpRollingBuffer.c"
      "${KVS_WEBRTC_ROOT}/src/source/Rtp/Codecs/RtpG711Payloader.c"
      "${KVS_WEBRTC_ROOT}/src/source/Rtp/Codecs/RtpH264Payloader.c"
      "${KVS_WEBRTC_ROOT}/src/source/Rtp/Codecs/RtpH265Payloader.c"
      "${KVS_WEBRTC_ROOT}/src/source/Rtp/Codecs/RtpOpusPayloader.c"
      "${KVS_WEBRTC_ROOT}/src/source/Rtp/Codecs/RtpVP8Payloader.c"
      "${KVS_WEBRTC_ROOT}/src/source/Rtp/RtpPacket.c"
      "${KVS_WEBRTC_ROOT}/src/source/Signaling/ChannelInfo.c"
      "${KVS_WEBRTC_ROOT}/src/source/Signaling/FileCache.c"
      "${KVS_WEBRTC_ROOT}/src/source/Signaling/Client.c"
      "${KVS_WEBRTC_ROOT}/src/source/Signaling/StateMachine.c"
      "${KVS_WEBRTC_ROOT}/src/source/Sctp/Sctp.c"
      "${KVS_WEBRTC_ROOT}/src/source/Sdp/Deserialize.c"
      "${KVS_WEBRTC_ROOT}/src/source/Sdp/Serialize.c"
      "${KVS_WEBRTC_ROOT}/src/source/Srtp/SrtpSession.c"
      "${KVS_WEBRTC_ROOT}/src/source/Stun/Stun.c"
      "${KVS_WEBRTC_ROOT}/src/source/Threadpool/ThreadPoolContext.c"
      "${CMAKE_CURRENT_SOURCE_DIR}/src/app_webrtc.c"
)

set(kvs_webrtc_include_dirs
      "${KVS_WEBRTC_ROOT}/src/include"
      "${KVS_WEBRTC_ROOT}/src/source/Crypto"
      "${KVS_WEBRTC_ROOT}/src/source/Ice"
      "${KVS_WEBRTC_ROOT}/src/source/Metrics"
      "${KVS_WEBRTC_ROOT}/src/source/PeerConnection"
      "${KVS_WEBRTC_ROOT}/src/source/Rtcp"
      "${KVS_WEBRTC_ROOT}/src/source/Rtp"
      "${KVS_WEBRTC_ROOT}/src/source/Sdp"
      "${KVS_WEBRTC_ROOT}/src/source/Signaling"
      "${KVS_WEBRTC_ROOT}/src/source/Srtp"
      "${KVS_WEBRTC_ROOT}/src/source/Sctp"
      "${KVS_WEBRTC_ROOT}/src/source/Stun"
      "${KVS_WEBRTC_ROOT}/src/source/Threadpool"
)

# Conditionally add Signaling sources
if(CONFIG_USE_ESP_WEBSOCKET_CLIENT)
    set(kvs_webrtc_srcs
        ${kvs_webrtc_srcs}
        "${CMAKE_CURRENT_SOURCE_DIR}/src/SignalingESP.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/LwsApiCallsESP.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/DataBuffer.c"
    )
    message(STATUS "Using ESP WebSocket client for signaling")
else()
    set(kvs_webrtc_srcs
        ${kvs_webrtc_srcs}
        "${KVS_WEBRTC_ROOT}/src/source/Signaling/Signaling.c"
        "${KVS_WEBRTC_ROOT}/src/source/Signaling/LwsApiCalls.c"
    )
    message(STATUS "Using libwebsockets for signaling")
endif()

idf_component_register(
    SRCS "${kvs_webrtc_srcs}"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/WebRtcLogging.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/app_webrtc.c"

    INCLUDE_DIRS "${kvs_webrtc_include_dirs}"
        "${CMAKE_CURRENT_SOURCE_DIR}/include"

    PRIV_INCLUDE_DIRS "${KVS_WEBRTC_ROOT}/src/source"

    REQUIRES "mbedtls"
             "lwip"
             "esp_timer"
             "esp_wifi"
             "nvs_flash"
             "freertos"
             "esp_system"
             "heap"
             "driver"
             "spi_flash"
             "esp_http_client"
)

if(CONFIG_USE_ESP_WEBSOCKET_CLIENT)
    target_compile_definitions(${COMPONENT_LIB} PUBLIC -DUSE_ESP_WEBSOCKET -DESP_PLATFORM)
endif()

# option(ENABLE_KVS_THREADPOOL "Enable support for KVS thread pool in signaling" OFF)
set(KVS_PLAT_ESP_FREERTOS ON CACHE BOOL "Build for ESP FreeRTOS")
add_definitions(-DKVS_PLAT_ESP_FREERTOS)
# if (ENABLE_KVS_THREADPOOL)
#   add_definitions(-DENABLE_KVS_THREADPOOL)
# endif()

# Check for ifaddrs.h and poll.h
include(CheckIncludeFiles)
CHECK_INCLUDE_FILES(ifaddrs.h HAVE_IFADDRS_H)
if(HAVE_IFADDRS_H)
  add_definitions(-DHAVE_IFADDRS_H=1)
endif()

CHECK_INCLUDE_FILES(poll.h HAVE_POLL_H)
if(HAVE_POLL_H)
  add_definitions(-DHAVE_POLL_H=1)
endif()

# Check for socketpair availability
include(CheckFunctionExists)
CHECK_FUNCTION_EXISTS(socketpair HAVE_SOCKETPAIR)
if(HAVE_SOCKETPAIR)
  add_definitions(-DHAVE_SOCKETPAIR=1)
endif()

if (CONFIG_ENABLE_DATA_CHANNEL)
    message("#####################  ENABLE_DATA_CHANNEL: ${CONFIG_ENABLE_DATA_CHANNEL}  #####################")
    add_definitions(-DENABLE_DATA_CHANNEL)
endif()

target_compile_definitions(${COMPONENT_LIB} PUBLIC
    -DKVS_USE_MBEDTLS
    -DKVS_BUILD_WITH_MBEDTLS
    -DKVS_USE_POOL_ALLOCATOR
    -DESP_PLATFORM
    -DAWS_IOT_SDK
    -DKVS_PLAT_ESP
    # -DLWS_WITH_ESP32
    -DLWS_WITH_CUSTOM_HEADERS
    -DENABLE_STREAMING
    -DKVS_USE_PLATFORM_TYPES
)

target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wno-error=format
    -Wno-error=unused-function
    -Wno-error=unused-variable
    -Wno-error=unused-but-set-variable
    -Wno-error=sign-compare
    -Wno-error=cpp
    -Wno-error=nonnull
    -Wno-error=undef
    -Wno-error=builtin-macro-redefined
    -Wno-builtin-macro-redefined
    -Wno-error=pointer-sign
    -Wno-error=incompatible-pointer-types
    -Wno-error=pointer-to-int-cast
    -Wno-error=int-to-pointer-cast
    -Wno-error=format-truncation
    -Wno-error=restrict
    -Wno-error=implicit-fallthrough
    -Wno-error=type-limits
    -Wno-error=array-bounds
    -Wno-error=maybe-uninitialized
)

# Add definitions for usrsctp
add_definitions(
  "-DHAVE_SCONN_LEN=1"
  "-DSCTP_USE_LWIP=1"
  "-DSCTP_USE_RTOS=1"
  "-DSCTP_SIMPLE_ALLOCATOR"
  "-D__Userspace__"
  "-DINET=1"
  "-DSCTP_DEBUG=1"
  "-DSCTP_USE_IPV6=0"
  "-DSCTP_USE_MBEDTLS_SHA1=1"
)

if (CONFIG_PREFER_DYNAMIC_ALLOCS)
    add_definitions(-DPREFER_DYNAMIC_ALLOCS=1)
endif()
