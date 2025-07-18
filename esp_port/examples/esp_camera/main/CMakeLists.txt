idf_component_register(
    SRCS "esp_webrtc_camera_main.c" "apprtc_signaling.c" "apprtc_signaling_interface.c" "webrtc_cli.c" "message_queue.c" "signaling_conversion.c"
    INCLUDE_DIRS "."
    REQUIRES "nvs_flash"
             "esp_wifi"
             "esp_timer"
             "esp_netif"
             "esp_event"
             "mbedtls"
             "lwip"
             "app_common"
             "json"
    )

target_compile_definitions(${COMPONENT_LIB} PRIVATE
    -DESP_PLATFORM
)

if (CONFIG_ENABLE_DATA_CHANNEL)
    add_definitions(-DENABLE_DATA_CHANNEL)
endif()
