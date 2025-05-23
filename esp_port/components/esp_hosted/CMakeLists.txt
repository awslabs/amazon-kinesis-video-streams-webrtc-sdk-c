if(CONFIG_SOC_WIFI_SUPPORTED)
	message(STATUS "Not using Hosted Wi-Fi")
else()
	message(STATUS "Using Hosted Wi-Fi")

	#	#### slave
	#
	#	set(app_name "slave")
	#	set(slave_app_name "network_adapter.bin")
	#	set(slave_build_target "slave-build")
	#	set(slave_flash_target "slave-flash")
	#	set(slave_clean_target "slave-clean")
	#	set(slave_fullclean_target "slave-fullclean")
	#	set(slave_menuconfig_target "slave-menuconfig")
	#	idf_build_get_property(idf_path IDF_PATH)
	#	set(slave_idf_target "${CONFIG_IDF_SLAVE_TARGET}")
	#
	#	set(slave_build_dep "slave_build_dep")
	#	idf_build_get_property(python PYTHON)
	#	idf_build_get_property(extra_cmake_args EXTRA_CMAKE_ARGS)
	#	set(ESPTOOLPY ${python} "$ENV{ESPTOOL_WRAPPER}" "${IDF_PATH}/components/esptool_py/esptool/esptool.py" --chip ${slave_idf_target})
	#
	#	#set(component_dir "${IDF_PATH}/components")
	#	#execute_process (
	#	#COMMAND bash -c "echo IDFPath: ${IDF_PATH};pwd; ls -lrt ${component_dir}; ls -lrt ${component_dir}/esp_slave"
	#	#COMMAND bash -c "echo toolchain: ${CMAKE_TOOLCHAIN_FILE}"
	#	#)
	#
	#	#execute_process ( COMMAND bash -c "echo SDKCONFIG: ${SDKCONFIG}")
	#	#message("CMAKE_CURRENT_BINARY_DIR: ${CMAKE_CURRENT_BINARY_DIR} build dir: ${CMAKE_CURRENT_BINARY_DIR}/${app_name} COMPONENT_DIR: ${component_dir}")
	#	#execute_process ( COMMAND bash -c "echo 1------------- ${CMAKE_CURRENT_BINARY_DIR}/slave")
	#
	#	if(NOT CMAKE_BUILD_EARLY_EXPANSION)
	#
	#	idf_build_set_property(SLAVE_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/${app_name}/build) #/src/${slave_build_target})
	#
	#	include(ExternalProject)
	#	ExternalProject_add(${slave_build_dep}
	#		PREFIX ${CMAKE_CURRENT_BINARY_DIR}/slave
	#		SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/slave
	#		BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/${app_name}/build
	#		INSTALL_COMMAND ""
	#		CMAKE_ARGS  -DCMAKE_GENERATOR=${CMAKE_GENERATOR}
	#		-DIDF_TARGET=${slave_idf_target}
	#		-DIDF_PATH=${idf_path}
	#		-DSDKCONFIG=${CMAKE_CURRENT_BINARY_DIR}/slave/sdkconfig
	#		-DPYTHON=${python}
	#		${extra_cmake_args}
	#		#BUILD_BYPRODUCTS ${slave_artifacts} ${slave_artifacts_extras} ${slave_ps_sources}
	#		#${CMAKE_CURRENT_BINARY_DIR}/${app_name}/${app_name}
	#		#BUILD_ALWAYS 1
	#		# These options are set so that Ninja immediately outputs
	#		# the subproject build to the terminal. Otherwise it looks like the
	#		# build process "hangs" while the subproject is being built.
	#		USES_TERMINAL_DOWNLOAD TRUE
	#		USES_TERMINAL_CONFIGURE TRUE
	#		USES_TERMINAL_BUILD TRUE
	#	)
	#
	#	#add_custom_command(TARGET ${app_name} PRE_BUILD
	#	#COMMAND "${CMAKE_COMMAND} -E remove_directory ${CMAKE_CURRENT_BINARY_DIR}/slave"
	#	#DEPENDS ${PROJECT_DIR}/sdkconfig)
	#	#add_dependencies(${PROJECT_DIR}/sdkconfig ${app_name})
	#
	#	#add_library(host_sdkconfig ${PROJECT_DIR}/sdkconfig)
	#	set_source_files_properties(
	#		${PROJECT_DIR}/sdkconfig
	#		PROPERTIES GENERATED TRUE)
	#
	#	add_custom_target(${slave_build_target}
	#		DEPENDS ${slave_build_dep})
	#
	#	#add_custom_command(TARGET ${app_name} PRE_BUILD
	#	#	#file(REMOVE_RECURSE ${CMAKE_CURRENT_BINARY_DIR}/slave)
	#	#	#COMMAND bash -c "echo ------------- $PWD, ${CMAKE_CURRENT_BINARY_DIR}/slave"
	#	#	COMMAND "${CMAKE_COMMAND} -E rm -r ${CMAKE_CURRENT_BINARY_DIR}/slave/sdkconfig"
	#	#	DEPENDS ../../../../sdkconfig)
	#
	#
	#	#add_custom_target(slave-build
	#	#            DEPENDS ${ULP_APP_NAME} ${ULP_APP_NAME}.bin ${ULP_APP_NAME}.sym
	#	#                    ${CMAKE_CURRENT_BINARY_DIR}/${ULP_APP_NAME}.ld
	#	#                    ${CMAKE_CURRENT_BINARY_DIR}/${ULP_APP_NAME}.h
	#	#            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
	#
	#	# add_custom_target(
	#	#	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/slave
	#	#	#COMMAND "${python} esptool.py --chip ${slave_idf_target} write_flash @build/flash_project_args"
	#	#	COMMAND ${CMAKE_COMMAND}
	#	#    -D ESP_DFU_LIST="1"
	#	#    -P ${idf_path}/tools/cmake/run_dfu_util.cmake
	#	#    USES_TERMINAL
	#	#	VERBATIM)
	#
	#	idf_build_get_property(slave_build_dir SLAVE_BUILD_DIR)
	#	#idf_build_get_property(build_dir ${CMAKE_CURRENT_BINARY_DIR}/slave/src/${slave_build_target})
	#	idf_component_get_property(esptool_py_dir esptool_py COMPONENT_DIR)
	#	idf_component_get_property(main_args esptool_py FLASH_ARGS)
	#	idf_component_get_property(sub_args esptool_py FLASH_SUB_ARGS)
	#
	#	#	CMAKE_POLICY(PUSH)
	#	#	CMAKE_POLICY(SET CMP0005 NEW)
	#	#	ADD_DEFINITIONS(-DSLAVE_ESPPORT=$ENV{SLAVE_ESPPORT})
	#	#	CMAKE_POLICY(POP)
	#
	#	add_custom_target(${slave_flash_target}
	#		COMMAND
	#		${CMAKE_COMMAND} -E env "ESPPORT=$ENV{SLAVE_ESPPORT}"
	#		${CMAKE_COMMAND}
	#		-D "IDF_PATH=${idf_path}"
	#		#-D "ESPPORT=$ENV{SLAVE_ESPPORT}"
	#		-D "SERIAL_TOOL=${ESPTOOLPY}"
	#		-D "SERIAL_TOOL_ARGS=${main_args};write_flash;@flash_project_args"
	#		-D "WORKING_DIRECTORY=${slave_build_dir}"
	#		-P ${esptool_py_dir}/run_serial_tool.cmake
	#		WORKING_DIRECTORY ${slave_build_dir}
	#		USES_TERMINAL
	#		VERBATIM
	#	)
	#	#set_target_properties(${slave_flash_target} PROPERTIES ESPPORT $ENV{SLAVE_ESPPORT})
	#
	#
	#	add_dependencies(${slave_flash_target} ${slave_build_target})
	#
	#
	#	 # Cleanup only slqve build directories. retain sdkconfig
	#	add_custom_target(${slave_clean_target}
	#		COMMAND ${CMAKE_COMMAND} -E remove_directory ${app_name}/build
	#		COMMAND ${CMAKE_COMMAND} -E remove_directory ${app_name}/src
	#		COMMAND ${CMAKE_COMMAND} -E remove_directory ${app_name}/tmp
	#		WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	#		USES_TERMINAL
	#		VERBATIM
	#	)
	#
	#	# Cleanup slave sdkconfig and all build files
	#	add_custom_target(${slave_fullclean_target}
	#		COMMAND ${CMAKE_COMMAND} -E remove_directory ${app_name}
	#		WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	#		USES_TERMINAL
	#		VERBATIM
	#	)
	#
	#	add_custom_target(${slave_menuconfig_target}
	#		COMMAND ${CMAKE_COMMAND}
	#		CMAKE_ARGS  -DCMAKE_GENERATOR=${CMAKE_GENERATOR}
	#		-DIDF_TARGET=${slave_idf_target}
	#		-DIDF_PATH=${idf_path}
	#		-DSDKCONFIG=${CMAKE_CURRENT_BINARY_DIR}/slave/sdkconfig
	#		-DPYTHON=${python}
	#		${extra_cmake_args} --target menuconfig
	#		#COMMAND "ninja menuconfig"
	#		#CMAKE_ARGS  -DCMAKE_GENERATOR=${CMAKE_GENERATOR}
	#		WORKING_DIRECTORY ${slave_build_dir}
	#		USES_TERMINAL
	#		VERBATIM
	#	)
	#	endif()
	#
	#
	#	#add_dependencies(${SLAVE_BUILD_DIR}/network_adapter.bin ${slave_flash_target}
	#
	#	# add_custom_target(set-slave-target DEPENDS set_slave_target)
	#
	#	#add_custom_command(TARGET ${app_name} PRE_BUILD
	#	#	COMMAND
	#	#	if(NOT "${CMAKE_TOOLCHAIN_FILE}" matches "toolchain-${slave_idf_target}")
	#	#		"${CMAKE_COMMAND} -E remove_directory build"
	#	#	endif()
	#	#	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/slave
	#	#	VERBATIM)
	#
	#	# add_custom_target(slave-clean
	#	#	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	#	#	COMMAND "${CMAKE_COMMAND} -E remove_directory slave"
	#	#	VERBATIM)

	### Host
	set(FG_root_dir ".")
	set(host_dir "${FG_root_dir}/host")

	set(COMPONENT_ADD_INCLUDEDIRS "${host_dir}" "${host_dir}/port" "${host_dir}/api")
	set(COMPONENT_SRCS "${host_dir}/esp_hosted_host_init.c" "${host_dir}/api/esp_hosted_api.c" "${host_dir}/drivers/transport/transport_drv.c" "${host_dir}/drivers/serial/serial_ll_if.c" "${host_dir}/utils/common.c" "${host_dir}/utils/util.c" "${host_dir}/utils/stats.c" "${host_dir}/port/os_wrapper.c" "${host_dir}/drivers/serial/serial_drv.c")

	list(APPEND COMPONENT_ADD_INCLUDEDIRS "." "${host_dir}/api" "${host_dir}/drivers/transport" "${host_dir}/drivers/transport/spi" "${host_dir}/drivers/transport/sdio" "${host_dir}/drivers/serial" "${host_dir}/utils")

	# rpc files - wrap -> slaveif -> core
	set(rpc_dir "${host_dir}/drivers/rpc")
	set(rpc_core_dir "${rpc_dir}/core")
	set(rpc_slaveif_dir "${rpc_dir}/slaveif")
	set(rpc_wrap_dir "${rpc_dir}/wrap")
	list(APPEND COMPONENT_SRCS  "${rpc_core_dir}/rpc_core.c" "${rpc_core_dir}/rpc_req.c" "${rpc_core_dir}/rpc_rsp.c" "${rpc_core_dir}/rpc_evt.c"
		"${rpc_slaveif_dir}/rpc_slave_if.c"
		"${rpc_wrap_dir}/rpc_wrap.c")
	list(APPEND COMPONENT_ADD_INCLUDEDIRS "${rpc_core_dir}" "${rpc_slaveif_dir}" "${rpc_wrap_dir}")

	# virtual serial
	set(virt_serial_dir "${host_dir}/drivers/virtual_serial_if")
	list(APPEND COMPONENT_SRCS  "${virt_serial_dir}/serial_if.c")
	list(APPEND COMPONENT_ADD_INCLUDEDIRS "${virt_serial_dir}")

	# mempool
	list(APPEND COMPONENT_SRCS "${host_dir}/drivers/mempool/mempool.c")
	list(APPEND COMPONENT_ADD_INCLUDEDIRS "${host_dir}/drivers/mempool" )

	# slave and host common files
	set(common_dir "${FG_root_dir}/common")
	list(APPEND COMPONENT_SRCS "${common_dir}/protobuf-c/protobuf-c/protobuf-c.c" "${common_dir}/proto/esp_hosted_rpc.pb-c.c" )
	list(APPEND COMPONENT_ADD_INCLUDEDIRS "${common_dir}/include" "${common_dir}/protobuf-c" "${common_dir}/proto" )

	# bt (NimBLE)
	### TODO config for HCI over UART
	list(APPEND COMPONENT_ADD_INCLUDEDIRS "${host_dir}/drivers/bt")
	if(CONFIG_ESP_HCI_VHCI)
		list(APPEND COMPONENT_SRCS "${host_dir}/drivers/bt/vhci_drv.c")
	else()
		list(APPEND COMPONENT_SRCS "${host_dir}/drivers/bt/hci_stub_drv.c")
	endif()

	# power save
	list(APPEND COMPONENT_ADD_INCLUDEDIRS "${host_dir}/drivers/power_save")
	list(APPEND COMPONENT_SRCS "${host_dir}/drivers/power_save/power_save_drv.c")

	# transport files
	if(CONFIG_ESP_SDIO_HOST_INTERFACE)
		list(APPEND COMPONENT_SRCS "${host_dir}/drivers/transport/sdio/sdio_drv.c" "${host_dir}/port/sdio_wrapper.c")
	else(CONFIG_ESP_SPI_HOST_INTERFACE)
		list(APPEND COMPONENT_SRCS "${host_dir}/drivers/transport/spi/spi_drv.c" "${host_dir}/port/spi_wrapper.c")
	endif()
endif()

idf_component_register(SRCS ${COMPONENT_SRCS}
	REQUIRES soc esp_event esp_netif esp_timer driver esp_wifi sdmmc bt esp_http_client esp_gdbstub
	EXCLUDE_SRCS ${EXCLUDE_COMPONENT_SRCS}
	INCLUDE_DIRS ${COMPONENT_ADD_INCLUDEDIRS})

idf_component_set_property(${COMPONENT_NAME} WHOLE_ARCHIVE TRUE)
