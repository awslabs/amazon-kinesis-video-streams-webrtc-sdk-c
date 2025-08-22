idf_component_register(
    SRCS
        "src/apprtc_signaling.c"
        "src/apprtc_signaling_internal.h"
        "src/apprtc_signaling_interface.c"
        "src/message_queue.c"
        "src/signaling_conversion.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_client json
)
