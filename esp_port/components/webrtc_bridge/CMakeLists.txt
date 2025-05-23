set(srcs "src/webrtc_bridge.c")

set(requires mqtt esp_webrtc_utils)

idf_component_register(SRCS "${srcs}"
                       REQUIRES ${requires}
                       INCLUDE_DIRS "include")

if (ENABLE_SIGNALLING_ONLY)
    add_definitions(-DENABLE_SIGNALLING_ONLY)
elseif (ENABLE_STREAMING_ONLY)
    add_definitions(-DENABLE_STREAMING_ONLY)
endif ()
