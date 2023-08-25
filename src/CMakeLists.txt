cmake_minimum_required(VERSION 3.6.3)

project(KinesisVideoWebRTCClient LANGUAGES C)

file(GLOB_RECURSE HEADERS "include/*.h")
file(GLOB_RECURSE SOURCE "source/*.c")

set(INCLUDES_PATH "${CMAKE_CURRENT_SOURCE_DIR}/include/")
include_directories(${INCLUDES_PATH})
include_directories(${BP_BUILD_INCLUDE_DIRS})
link_directories(${BP_BUILD_LIB})
get_property(dirs DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY INCLUDE_DIRECTORIES)

add_library(${PROJECT_NAME} SHARED ${HEADERS} ${SOURCE})
target_compile_definitions(${PROJECT_NAME} PRIVATE KVS_BUILD_WITH_LWS=1)
target_link_libraries(${PROJECT_NAME} ${BP_BUILD_LIB_DIRS}
  client heap trace view mkvgen utils state jsmn crypto ssl)

install(TARGETS ${PROJECT_NAME} DESTINATION lib)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
          DESTINATION .
                FILES_MATCHING PATTERN "Include.h")
