# Set KVS_SDK_PATH if not already set in environment
if(NOT DEFINED ENV{KVS_SDK_PATH})
    get_filename_component(KVS_SDK_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
    message(WARNING "KVS_SDK_PATH not set in environment. Setting to ${KVS_SDK_PATH}")
    set(ENV{KVS_SDK_PATH} "${KVS_SDK_PATH}")
endif()
