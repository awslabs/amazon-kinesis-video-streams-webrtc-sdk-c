idf_component_register(
    SRCS "src/signaling_bridge_adapter.c"
    INCLUDE_DIRS "include"
    REQUIRES kvs_webrtc signaling_serializer esp_common network_coprocessor webrtc_bridge
)
