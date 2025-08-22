idf_component_register(
    SRCS "src/signaling_bridge_adapter.c" "src/bridge_peer_connection.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_common
)
