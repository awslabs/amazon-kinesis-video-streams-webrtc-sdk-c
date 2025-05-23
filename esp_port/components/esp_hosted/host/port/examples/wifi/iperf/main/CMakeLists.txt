set(FG_ROOT_DIR "../../../../../..")
set(MCU_DIR "${FG_ROOT_DIR}/host")

set(COMPONENT_ADD_INCLUDEDIRS "${MCU_DIR}" "${MCU_DIR}/port")
set(COMPONENT_SRCS "cmd_wifi.c" "iperf_example_main.c" "${MCU_DIR}/drivers/transport/transport_drv.c" "${MCU_DIR}/drivers/serial/serial_ll_if.c" "${MCU_DIR}/utils/common.c" "${MCU_DIR}/utils/util.c" "${MCU_DIR}/utils/stats.c" "${MCU_DIR}/port/os_wrapper.c" "${MCU_DIR}/drivers/serial/serial_drv.c" "${MCU_DIR}/drivers/netif/wifi_default.c" "${MCU_DIR}/drivers/netif/wifi_netif.c")

list(APPEND COMPONENT_ADD_INCLUDEDIRS "." "${MCU_DIR}/drivers/transport" "${MCU_DIR}/drivers/transport/spi" "${MCU_DIR}/drivers/transport/sdio" "${MCU_DIR}/drivers/netif" "${MCU_DIR}/drivers/serial" "${MCU_DIR}/utils" "${FG_ROOT_DIR}/common/include")

set(NETIF_DIR "${MCU_DIR}/drivers/netif")
list(APPEND COMPONENT_SRCS "${NETIF_DIR}/wifi_default.c" "${NETIF_DIR}/wifi_netif.c" )
list(APPEND COMPONENT_ADD_INCLUDEDIRS "${NETIF_DIR}" "${NETIF_DIR}/" )

#drivers
# rpc lib files
set(RPC_LIB_DIR "${FG_ROOT_DIR}/host/drivers/rpc_lib")
set(RPC_CORE_DIR "${RPC_LIB_DIR}/core")
list(APPEND COMPONENT_SRCS  "${RPC_CORE_DIR}/rpc_core.c" "${RPC_LIB_DIR}/rpc_req.c" "${RPC_LIB_DIR}/rpc_rsp.c" "${RPC_LIB_DIR}/rpc_evt.c" "${RPC_LIB_DIR}/rpc_api.c" "${FG_ROOT_DIR}/host/drivers/virtual_serial_if/serial_if.c")
list(APPEND COMPONENT_ADD_INCLUDEDIRS "${RPC_CORE_DIR}" "${RPC_LIB_DIR}" "${FG_ROOT_DIR}/host/drivers/virtual_serial_if")

list(APPEND COMPONENT_SRCS  "${FG_ROOT_DIR}/host/drivers/rpc_wrapper/rpc_wrapper.c" "${FG_ROOT_DIR}/host/drivers/rpc_wrapper/wifi/wifi_drv.c")
list(APPEND COMPONENT_ADD_INCLUDEDIRS "${FG_ROOT_DIR}/host/drivers" "${FG_ROOT_DIR}/host/drivers/rpc_wrapper" "${FG_ROOT_DIR}/host/drivers/rpc_wrapper/wifi")

# helpers - mempool
list(APPEND COMPONENT_SRCS "${MCU_DIR}/drivers/mempool/mempool.c")
list(APPEND COMPONENT_ADD_INCLUDEDIRS "${MCU_DIR}/drivers/mempool" )

# protobuf files
list(APPEND COMPONENT_SRCS "${FG_ROOT_DIR}/common/protobuf-c/protobuf-c/protobuf-c.c" "${FG_ROOT_DIR}/common/proto/esp_hosted_rpc.pb-c.c" )
list(APPEND COMPONENT_ADD_INCLUDEDIRS "${FG_ROOT_DIR}/common/protobuf-c" "${FG_ROOT_DIR}/common/proto" )

# transport files
if(CONFIG_ESP_SDIO_HOST_INTERFACE)
    list(APPEND COMPONENT_SRCS "${MCU_DIR}/drivers/transport/sdio/sdio_drv.c" "${MCU_DIR}/port/esp/sdio_wrapper.c")
else(CONFIG_ESP_SPI_HOST_INTERFACE)
	list(APPEND COMPONENT_SRCS "${MCU_DIR}/drivers/transport/spi/spi_drv.c" "${MCU_DIR}/port/esp/spi_wrapper.c")
endif()

idf_component_register(SRCS ${COMPONENT_SRCS} EXCLUDE_SRCS ${EXCLUDE_COMPONENT_SRCS} INCLUDE_DIRS ${COMPONENT_ADD_INCLUDEDIRS})
