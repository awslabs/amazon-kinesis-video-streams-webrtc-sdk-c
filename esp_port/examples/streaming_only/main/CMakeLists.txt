idf_component_register(SRCS "streaming_only_main.c"
                    INCLUDE_DIRS "."
                    REQUIRES "app_common" "app_webrtc" "kvs_webrtc" "esp_webrtc_utils")

if (ENABLE_DATA_CHANNEL)
    add_definitions(-DENABLE_DATA_CHANNEL)
endif()
