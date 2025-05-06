idf_component_register(
    SRCS "webrtc_main.c"
    INCLUDE_DIRS "."
    EMBED_FILES "embedded_files/h264_samples/frame-0001.h264"
                "embedded_files/opus_samples/sample-001.opus"
    REQUIRES "nvs_flash"
             "esp_wifi"
             "esp_timer"
             "esp_netif"
             "esp_event"
             "mbedtls"
             "lwip"
             "app_common"
    )

target_compile_definitions(${COMPONENT_LIB} PRIVATE
    -DESP_PLATFORM
)

if (CONFIG_ENABLE_DATA_CHANNEL)
    add_definitions(-DENABLE_DATA_CHANNEL)
endif()
