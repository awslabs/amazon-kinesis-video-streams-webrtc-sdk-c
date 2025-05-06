# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

# Include common KVS SDK path setting
include(${CMAKE_CURRENT_SOURCE_DIR}/../app_common/set_kvs_sdk_path.cmake)

option(ENABLE_STREAMING_ONLY "enable only the streaming" ON)
option(ENABLE_DATA_CHANNEL "enable data channel support" ON)
option(ENABLE_SIGNALLING_ONLY "enable only the streaming" OFF)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

idf_build_set_property(__BUILD_COMPONENT_DEPGRAPH_ENABLED 1)

project(streaming_only)
