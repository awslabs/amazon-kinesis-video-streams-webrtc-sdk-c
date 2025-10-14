set(KVS_WEBRTC_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../")

# Pick generic sources from KVS for app webrtc
# Ideally, we should not pull in anything from KVS
set(kvs_sources_for_app_webrtc
    "${KVS_WEBRTC_ROOT}/src/source/PeerConnection/jsmn.c"
)

set(component_srcs
    "src/app_webrtc.c"
    ${kvs_sources_for_app_webrtc}
)

set(component_add_includedirs
    "include"
    "${KVS_WEBRTC_ROOT}/src/include"
    "${KVS_WEBRTC_ROOT}/src/source/PeerConnection"
)

set(component_requires
    "esp_common"
    "esp_timer"
    "nvs_flash"
    "freertos"
    "esp_system"
    "heap"
    "driver"
    "spi_flash"
    "mbedtls"
    "json"
)

idf_component_register(
    SRCS
        "${component_srcs}"
    INCLUDE_DIRS "${component_add_includedirs}"
    PRIV_INCLUDE_DIRS "${KVS_WEBRTC_ROOT}/src/source"
    REQUIRES "${component_requires}"
)

target_compile_definitions(${COMPONENT_LIB} PUBLIC
    -DKVS_USE_MBEDTLS # To keep the build system happy
)

# Suppress warnings for KVS code
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wno-error=unused-function
    -Wno-error=unused-variable
    -Wno-error=unused-but-set-variable
    -Wno-error=undef
    -Wno-error=pointer-sign
    -Wno-error=incompatible-pointer-types
    -Wno-error=pointer-to-int-cast
    -Wno-error=int-to-pointer-cast
    -Wno-error=maybe-uninitialized
)
