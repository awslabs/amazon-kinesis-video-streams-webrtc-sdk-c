idf_component_register(SRCS "streaming_only_main.c" "signalling_remote.c"
                    INCLUDE_DIRS "."
                    REQUIRES "app_common" "kvs_webrtc" "esp_webrtc_utils")

if (ENABLE_DATA_CHANNEL)
    add_definitions(-DENABLE_DATA_CHANNEL)
endif()
