set(srcs "src/esp_cli.c" "src/wifi_cli.c" "src/webrtc_mem_utils.c"
         "src/esp_work_queue.c" "src/esp_webrtc_time.c" "src/flash_wrapper.c")

set(requires esp_wifi console mqtt esp_timer esp_common freertos nvs_flash)

idf_component_register(SRCS "${srcs}"
                       REQUIRES ${requires}
                       INCLUDE_DIRS "include")

if (ENABLE_SIGNALLING_ONLY)
    add_definitions(-DENABLE_SIGNALLING_ONLY)
elseif (ENABLE_STREAMING_ONLY)
    add_definitions(-DENABLE_STREAMING_ONLY)
endif ()

target_compile_options(${COMPONENT_LIB} PRIVATE -DCONFIG_SPIRAM_ALLOW_STACK_EXTERNAL_MEMORY)
