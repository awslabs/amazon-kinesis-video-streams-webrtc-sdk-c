
idf_component_register(
    SRCS "state_machine.c"
    INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}"
    REQUIRES
        "kvs_utils"
)
