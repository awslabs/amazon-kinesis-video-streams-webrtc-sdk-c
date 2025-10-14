cmake_minimum_required(VERSION 3.5)

set(COMPONENTS main esp_partition nvs_flash esp_timer driver freertos esp_netif newlib esp_event lwip esp_timer log sdmmc)
list(APPEND EXTRA_COMPONENT_DIRS "${COMPONENTS_DIR}")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(hosted_esp32_host)
idf_build_set_property(COMPILE_OPTIONS "-fdiagnostics-color=always" APPEND)
#target_link_options(${mdns} INTERFACE -fsanitize=address -fsanitize=undefined)
