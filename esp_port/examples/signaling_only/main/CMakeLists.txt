idf_component_register(SRCS "signaling_only_main.c"
                    INCLUDE_DIRS "."
                    REQUIRES "app_common" "app_webrtc" "esp_webrtc_utils" "signaling_bridge_adapter" "kvs_signaling")
