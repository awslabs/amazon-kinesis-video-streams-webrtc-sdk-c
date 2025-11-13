set(KVS_WEBRTC_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../")

set(component_srcs
    "${KVS_WEBRTC_ROOT}/src/source/Signaling/Client.c"
    "${KVS_WEBRTC_ROOT}/src/source/Signaling/StateMachine.c"
    "${KVS_WEBRTC_ROOT}/src/source/Signaling/FileCache.c"
    "${KVS_WEBRTC_ROOT}/src/source/Signaling/ChannelInfo.c"
    "src/kvs_signaling.c"
)

if(CONFIG_USE_ESP_WEBSOCKET_CLIENT)
    list(APPEND component_srcs
        "src/LwsApiCallsESP.c"
        "src/SignalingESP.c"
        "src/DataBuffer.c")
else()
    list(APPEND component_srcs
        "${KVS_WEBRTC_ROOT}/src/source/Signaling/Signaling.c"
        "${KVS_WEBRTC_ROOT}/src/source/Signaling/LwsApiCalls.c"
    )
endif()

set(component_add_includedirs
    "include"
    "${KVS_WEBRTC_ROOT}/src/include"
    "${KVS_WEBRTC_ROOT}/src/source"
    "${KVS_WEBRTC_ROOT}/src/source/Signaling"
)

set(component_requires
    "esp_common"
    "esp_timer"
    "esp_http_client"
    "json"
    "libwebsockets"
    "mbedtls"
    "kvs_utils"
    "credential"
    "esp_webrtc_utils"
    "state_machine"
)

set(component_priv_requires
    "esp_netif"
    "esp_wifi"
)

idf_component_register(
    SRCS
        "${component_srcs}"
    INCLUDE_DIRS "${component_add_includedirs}"
    PRIV_INCLUDE_DIRS "${component_priv_includes}"
    REQUIRES "${component_requires}"
    PRIV_REQUIRES "${component_priv_requires}"
)

if(CONFIG_USE_ESP_WEBSOCKET_CLIENT)
    target_compile_definitions(${COMPONENT_LIB} PUBLIC -DUSE_ESP_WEBSOCKET -DESP_PLATFORM)
endif()

# Add the necessary compile definitions for KVS signaling
target_compile_definitions(${COMPONENT_LIB} PUBLIC
    -DKVS_USE_MBEDTLS
    -DKVS_BUILD_WITH_MBEDTLS
    -DKVS_USE_POOL_ALLOCATOR
    -DESP_PLATFORM
    -DAWS_IOT_SDK
    -DKVS_PLAT_ESP
    -DLWS_WITH_CUSTOM_HEADERS
    -DENABLE_STREAMING
    -DKVS_USE_PLATFORM_TYPES
    -DKVS_PLAT_ESP_FREERTOS
)

# Suppress warnings for KVS code
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wno-error=format
    -Wno-error=unused-function
    -Wno-error=unused-variable
    -Wno-error=unused-but-set-variable
    -Wno-error=sign-compare
    -Wno-error=cpp
    -Wno-error=nonnull
    -Wno-nonnull
    -Wno-error=undef
    -Wno-error=builtin-macro-redefined
    -Wno-builtin-macro-redefined
    -Wno-error=pointer-sign
    -Wno-error=incompatible-pointer-types
    -Wno-error=pointer-to-int-cast
    -Wno-error=int-to-pointer-cast
    -Wno-pointer-to-int-cast
    -Wno-int-to-pointer-cast
    -Wno-error=format-truncation
    -Wno-error=restrict
    -Wno-error=implicit-fallthrough
    -Wno-error=type-limits
    -Wno-error=array-bounds
    -Wno-error=maybe-uninitialized
    -Wno-maybe-uninitialized
)
