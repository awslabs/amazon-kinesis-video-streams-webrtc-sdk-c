idf_component_register(SRCS
                        "signaling_only_main.c"
                        "sleep_command.c"
                    INCLUDE_DIRS "."
                    REQUIRES
                        "app_common"
                        "signaling_bridge_adapter")
