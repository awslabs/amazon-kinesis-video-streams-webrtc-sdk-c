cmake_minimum_required(VERSION 3.5)

# Include common KVS SDK path setting
include(${CMAKE_CURRENT_SOURCE_DIR}/../app_common/set_kvs_sdk_path.cmake)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(webrtc_classic)
option(ENABLE_DATA_CHANNEL "Enable data_channel support" ON)
