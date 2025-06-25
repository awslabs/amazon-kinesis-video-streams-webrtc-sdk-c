idf_component_register(
    SRCS "webrtc_main.c"
    INCLUDE_DIRS "."
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
