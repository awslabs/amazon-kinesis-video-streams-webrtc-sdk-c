idf_component_register(SRCS
                        "streaming_only_main.c"
                        "sleep_command.c"
                    INCLUDE_DIRS "."
                    REQUIRES
                        "app_common"
                        "kvs_webrtc")

# if (ENABLE_DATA_CHANNEL)
#     add_definitions(-DENABLE_DATA_CHANNEL)
# endif()
