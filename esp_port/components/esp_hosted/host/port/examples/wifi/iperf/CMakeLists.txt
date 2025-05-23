# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS $ENV{IDF_PATH}/examples/system/console/advanced/components
                         $ENV{IDF_PATH}/examples/common_components/iperf)
set(COMPONENTS main esp_partition nvs_flash esp_timer driver freertos esp_netif newlib esp_event lwip esp_timer log console cmd_system iperf esp_coex sdmmc)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(iperf)
