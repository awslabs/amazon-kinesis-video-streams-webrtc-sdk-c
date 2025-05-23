set(esp_hosted_path ../esp_hosted/)

set(app_srcs "slave_control.c" "${esp_hosted_path}/common/proto/esp_hosted_rpc.pb-c.c" "protocomm_pserial.c" "app_main.c" "slave_bt.c" "mempool.c" "stats.c"  "mempool_ll.c" "host_power_save.c")

set(APP_INCLUDE_DIRS "." "${esp_hosted_path}/common/include" "${esp_hosted_path}/common/protobuf-c" "${esp_hosted_path}/common/proto")

if(CONFIG_ESP_SDIO_HOST_INTERFACE)
    list(APPEND app_srcs sdio_slave_api.c)
else(CONFIG_ESP_SPI_HOST_INTERFACE)
    list(APPEND app_srcs spi_slave_api.c)
endif()

set(priv_requires esp_wifi protobuf-c esp_driver_gpio esp_driver_uart esp_driver_sdio wpa_supplicant app_update)
set(requires esp_wifi nvs_flash protocomm esp_webrtc_utils)

LIST(APPEND APP_INCLUDE_DIRS include)

# register_component()
idf_component_register(SRCS "${app_srcs}"
                    INCLUDE_DIRS "${APP_INCLUDE_DIRS}"
                    PRIV_REQUIRES ${priv_requires}
                    REQUIRES ${requires}
                    )

set(PROJECT_VERSION_MAJOR_1 "-DPROJECT_VERSION_MAJOR_1=0")
set(PROJECT_VERSION_MAJOR_2 "-DPROJECT_VERSION_MAJOR_2=0")
set(PROJECT_VERSION_MINOR "-DPROJECT_VERSION_MINOR=6")
target_compile_definitions(${COMPONENT_LIB} PRIVATE ${PROJECT_VERSION_MAJOR_1} ${PROJECT_VERSION_MAJOR_2} ${PROJECT_VERSION_MINOR})

target_compile_definitions(${COMPONENT_LIB} PRIVATE "-DESP_HOSTED_SLAVE_BUILD=1")

idf_component_get_property(protocomm_dir protocomm COMPONENT_DIR)
target_include_directories(${COMPONENT_LIB} PRIVATE "${protocomm_dir}/src/common")
