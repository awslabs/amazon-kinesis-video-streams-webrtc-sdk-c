cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
set(COMPONENTS main esp_netif esp_partition nvs_flash esp_event esp_timer driver freertos sdmmc)
#set(COMPONENTS main esp_netif esp_partition nvs_flash esp_event esp_timer driver freertos esp_gdbstub) #with gdbstub
project(esp_hosted_esp32_host)
idf_build_set_property(COMPILE_OPTIONS "-fdiagnostics-color=always" APPEND)
