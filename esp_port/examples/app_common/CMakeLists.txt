set(srcs "src/app_wifi.c" "src/app_storage.c")

set(requires esp_wifi fatfs spiffs)

idf_component_register(SRCS "${srcs}"
                       REQUIRES ${requires}
                       INCLUDE_DIRS "include")

if (ENABLE_SIGNALLING_ONLY)
    add_definitions(-DENABLE_SIGNALLING_ONLY)
elseif (ENABLE_STREAMING_ONLY)
    add_definitions(-DENABLE_STREAMING_ONLY)
endif ()

if (CONFIG_ENABLE_DATA_CHANNEL)
    add_definitions(-DENABLE_DATA_CHANNEL)
endif()

if (CONFIG_PREFER_DYNAMIC_ALLOCS)
    add_definitions(-DPREFER_DYNAMIC_ALLOCS=1)
endif()

# Create a SPIFFS image from the contents of the 'spiffs_image' directory
# that fits the partition named 'storage'. FLASH_IN_PROJECT indicates that
# the generated image should be flashed when the entire project is flashed to
# the target with 'idf.py -p PORT flash'.
if (CONFIG_APP_COMMON_USE_SPIFFS)
    spiffs_create_partition_image(storage spiffs_image FLASH_IN_PROJECT)
endif()
