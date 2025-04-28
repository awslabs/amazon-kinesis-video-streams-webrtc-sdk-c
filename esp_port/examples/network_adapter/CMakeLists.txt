# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

# Include common KVS SDK path setting
include(${CMAKE_CURRENT_SOURCE_DIR}/../app_common/set_kvs_sdk_path.cmake)

set(PROJECT_VERSION_MAJOR_1 "0")
set(PROJECT_VERSION_MAJOR_2 "0")
set(PROJECT_VERSION_MINOR "6")
message(*************************************************************************************)
message("                    Building ESP-Hosted-MCU FW :: ${PROJECT_VERSION_MAJOR_1}.${PROJECT_VERSION_MAJOR_2}.${PROJECT_VERSION_MINOR} ")
message(*************************************************************************************)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(network_adapter)
idf_build_set_property(COMPILE_OPTIONS "-fdiagnostics-color=always" APPEND)
