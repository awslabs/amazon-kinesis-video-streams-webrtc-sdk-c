idf_component_register(SRCS
                        "streaming_only_main.c"
                        "power_save_handler.c"
                    INCLUDE_DIRS "."
                    REQUIRES
                        "app_common"
                        "kvs_webrtc")
