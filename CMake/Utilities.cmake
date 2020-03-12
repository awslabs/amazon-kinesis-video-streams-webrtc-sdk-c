# build library from source
function(build_dependency lib_name)
  set(supported_libs
      gtest
      jsmn
      openssl
      srtp
      usrsctp
      websockets
      curl
      mbedtls)
  list(FIND supported_libs ${lib_name} index)
  if(${index} EQUAL -1)
    message(WARNING "${lib_name} is not supported to build from source")
    return()
  endif()

  set(lib_file_name ${lib_name})
  if (${lib_name} STREQUAL "openssl")
  	set(lib_file_name ssl)
  elseif(${lib_name} STREQUAL "srtp")
    set(lib_file_name srtp2)
  endif()
  set(library_found NOTFOUND)
  find_library(
    library_found
    NAMES ${lib_file_name}
    PATHS ${OPEN_SRC_INSTALL_PREFIX}/lib
    NO_DEFAULT_PATH)
  if(library_found)
    message(STATUS "${lib_name} already built")
    return()
  endif()

  # anything after lib_name(${ARGN}) are assumed to be arguments passed over to
  # library building cmake.
  set(build_args ${ARGN})

  file(REMOVE_RECURSE ${KINESIS_VIDEO_OPEN_SOURCE_SRC}/lib${lib_name})

  # build library
  configure_file(
    ${CMAKE_SOURCE_DIR}/CMake/Dependencies/lib${lib_name}-CMakeLists.txt
    ${KINESIS_VIDEO_OPEN_SOURCE_SRC}/lib${lib_name}/CMakeLists.txt COPYONLY)
  execute_process(
    COMMAND ${CMAKE_COMMAND} ${build_args}
            -DOPEN_SRC_INSTALL_PREFIX=${OPEN_SRC_INSTALL_PREFIX} -G
            ${CMAKE_GENERATOR} .
    RESULT_VARIABLE result
    WORKING_DIRECTORY ${KINESIS_VIDEO_OPEN_SOURCE_SRC}/lib${lib_name})
  if(result)
    message(FATAL_ERROR "CMake step for lib${lib_name} failed: ${result}")
  endif()
  execute_process(
    COMMAND ${CMAKE_COMMAND} --build .
    RESULT_VARIABLE result
    WORKING_DIRECTORY ${KINESIS_VIDEO_OPEN_SOURCE_SRC}/lib${lib_name})
  if(result)
    message(FATAL_ERROR "CMake step for lib${lib_name} failed: ${result}")
  endif()

  file(REMOVE_RECURSE ${KINESIS_VIDEO_OPEN_SOURCE_SRC}/lib${lib_name})

endfunction()

function(enableSanitizer SANITIZER)
  set(CMAKE_CXX_FLAGS
      "${CMAKE_CXX_FLAGS} -O0 -g -fsanitize=${SANITIZER} -fno-omit-frame-pointer"
      PARENT_SCOPE)
  set(CMAKE_C_FLAGS
      "${CMAKE_C_FLAGS} -O0 -g -fsanitize=${SANITIZER} -fno-omit-frame-pointer -fno-optimize-sibling-calls"
      PARENT_SCOPE)
  set(CMAKE_EXE_LINKER_FLAGS
      "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=${SANITIZER}"
      PARENT_SCOPE)
endfunction()

