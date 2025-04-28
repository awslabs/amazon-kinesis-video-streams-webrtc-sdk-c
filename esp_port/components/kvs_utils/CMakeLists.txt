# Option to use flash-based IO implementation
option(CONFIG_USE_FLASH_IO "Use flash-based IO implementation" ON)

set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(srcs
    "${SRC_DIR}/allocators.c"
    "${SRC_DIR}/atomics.c"
    "${SRC_DIR}/crc32.c"
    "${SRC_DIR}/base64.c"
    "${SRC_DIR}/directory.c"
    "${SRC_DIR}/double_linked_list.c"
    "${SRC_DIR}/endianness.c"
    "${SRC_DIR}/filelogger.c"
    "${SRC_DIR}/hash_table.c"
    "${SRC_DIR}/hex.c"
    "${SRC_DIR}/instrumented_allocators.c"
    # "${SRC_DIR}/kvs_config.c"
    "${SRC_DIR}/kvs_string.c"
    "${SRC_DIR}/logger.c"
    "${SRC_DIR}/mutex.c"
    "${SRC_DIR}/semaphore.c"
    "${SRC_DIR}/single_linked_list.c"
    "${SRC_DIR}/stack_queue.c"
    "${SRC_DIR}/tags.c"
    "${SRC_DIR}/thread.c"
    "${SRC_DIR}/time_port.c"
    "${SRC_DIR}/timer_queue.c"
    "${SRC_DIR}/util.c"
    "${SRC_DIR}/version.c"
    "${SRC_DIR}/request_info.c"
    "${SRC_DIR}/math.c"
    "${SRC_DIR}/ThreadSafeBlockingQueue.c"
    "${SRC_DIR}/ThreadPool.c"
    "${SRC_DIR}/backoff.c"
)

if (CONFIG_USE_FLASH_IO)
    list(APPEND srcs "${SRC_DIR}/fileio_flash.c")
else()
    list(APPEND srcs "${SRC_DIR}/fileio.c")
endif()

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS
        "./include"
        "./src"
    REQUIRES esp_system heap pthread esp_webrtc_utils
)
