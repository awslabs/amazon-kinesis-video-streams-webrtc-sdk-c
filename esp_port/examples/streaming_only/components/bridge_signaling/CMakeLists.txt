# Bridge Signaling Component (for streaming_only example)
# Provides bridge-based signaling functionality

idf_component_register(
    SRCS
        "bridge_signaling.c"
        "ice_bridge_client.c"
    INCLUDE_DIRS
        "include"
    REQUIRES
        signaling_serializer
        webrtc_bridge
)
