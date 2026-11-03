# build library from source
function(build_dependency lib_name)
  if (WIN32 OR NOT PARALLEL_BUILD)
    set(PARALLEL_BUILD "")
  else()
    set(PARALLEL_BUILD "--parallel")
  endif()
  set(supported_libs
      gperftools
      gtest
      awscpp
      benchmark
      jsmn
      openssl
      awslc
      srtp
      usrsctp
      websockets
      curl
      mbedtls
      kvspic
      kvsCommonLws)
  list(FIND supported_libs ${lib_name} index)
  if(${index} EQUAL -1)
    message(WARNING "${lib_name} is not supported to build from source")
    return()
  endif()

  set(lib_file_name ${lib_name})
  if (${lib_name} STREQUAL "openssl")
    set(lib_file_name ssl)
  elseif(${lib_name} STREQUAL "awslc")
    # AWS-LC installs OpenSSL-compatible artifacts: lib/libssl.* and
    # lib/libcrypto.*. The already-built probe below therefore looks for "ssl",
    # exactly as it does for OpenSSL.
    set(lib_file_name ssl)
  elseif(${lib_name} STREQUAL "srtp")
    if(USE_LIBSRTP3)
      set(lib_file_name srtp3)
    else()
      set(lib_file_name srtp2)
    endif()
  elseif(${lib_name} STREQUAL "gperftools")
    set(lib_file_name profiler)
  elseif(${lib_name} STREQUAL "awscpp")
    set(lib_file_name aws-cpp-sdk-core)
  endif()

  # OpenSSL and AWS-LC install libraries under the SAME names (libssl,
  # libcrypto) into the same prefix, so the find_library() probe below cannot
  # distinguish them: it would report "already built" for AWS-LC when the prefix
  # actually holds OpenSSL from an earlier configure, silently skip the AWS-LC
  # build, and then fail much later in find_package(AWSLC) with an unhelpful
  # "missing AWSLC_INCLUDE_DIR".
  #
  # Resolve it by identity rather than by filename. Only AWS-LC defines
  # OPENSSL_IS_AWSLC, and <openssl/base.h> does not exist in any OpenSSL
  # release, so the header is an unambiguous discriminator.
  if((${lib_name} STREQUAL "awslc") OR (${lib_name} STREQUAL "openssl"))
    set(prefix_holds_awslc FALSE)
    if(EXISTS "${OPEN_SRC_INSTALL_PREFIX}/include/openssl/base.h")
      file(STRINGS "${OPEN_SRC_INSTALL_PREFIX}/include/openssl/base.h" awslc_marker
           REGEX "^#define[ \t]+OPENSSL_IS_AWSLC")
      if(awslc_marker)
        set(prefix_holds_awslc TRUE)
      endif()
    endif()

    # Deliberately file(GLOB) rather than find_library(): find_library caches its
    # result, so once it recorded a path it would keep reporting it even after
    # the user deleted the prefix, making the error below impossible to clear
    # without also deleting the build directory.
    file(GLOB existing_ssl_libs
         "${OPEN_SRC_INSTALL_PREFIX}/lib/libssl.*"
         "${OPEN_SRC_INSTALL_PREFIX}/lib64/libssl.*"
         "${OPEN_SRC_INSTALL_PREFIX}/lib/ssl.lib"
         "${OPEN_SRC_INSTALL_PREFIX}/lib/libssl.lib")

    if(existing_ssl_libs AND (${lib_name} STREQUAL "awslc") AND NOT prefix_holds_awslc)
      message(FATAL_ERROR
        "USE_AWS_LC=ON, but ${OPEN_SRC_INSTALL_PREFIX} already contains a non-AWS-LC "
        "libssl (${existing_ssl_libs}) left over from a previous OpenSSL build. "
        "AWS-LC and OpenSSL install the same library names, so reusing this prefix "
        "would link the wrong crypto library and produce a binary with neither "
        "post-quantum key exchange nor FIPS.\n"
        "Remove the dependency prefix and reconfigure:\n"
        "    rm -rf ${OPEN_SRC_INSTALL_PREFIX}\n"
        "Or build into a separate prefix:\n"
        "    cmake .. -DUSE_AWS_LC=ON -DOPEN_SRC_INSTALL_PREFIX=<new-dir>")
    endif()

    if(existing_ssl_libs AND (${lib_name} STREQUAL "openssl") AND prefix_holds_awslc)
      message(FATAL_ERROR
        "USE_OPENSSL=ON, but ${OPEN_SRC_INSTALL_PREFIX} already contains AWS-LC. "
        "AWS-LC presents the OpenSSL 1.1.1 API, so this would appear to work while "
        "actually linking AWS-LC.\n"
        "Remove the dependency prefix and reconfigure:\n"
        "    rm -rf ${OPEN_SRC_INSTALL_PREFIX}\n"
        "Or build into a separate prefix with -DOPEN_SRC_INSTALL_PREFIX=<new-dir>")
    endif()
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

  file(REMOVE_RECURSE ${OPEN_SRC_INSTALL_PREFIX}/lib${lib_name})

  # build library
  configure_file(
    ./CMake/Dependencies/lib${lib_name}-CMakeLists.txt
    ${OPEN_SRC_INSTALL_PREFIX}/lib${lib_name}/CMakeLists.txt COPYONLY)

  # when OPEN_SRC_INSTALL_PREFIX has non-default value, patch files must be copied to temporary location,
  # otherwise build fails as it couldn't refer to the caller's CMake process directory.
  file(GLOB LIB_PATCHES "./CMake/Dependencies/lib${lib_name}-*.patch")
  message(STATUS "Copying patches for dependency ${lib_name}: ${LIB_PATCHES}")
  file(COPY ${LIB_PATCHES} DESTINATION ${OPEN_SRC_INSTALL_PREFIX}/lib${lib_name}/)

  execute_process(
    COMMAND ${CMAKE_COMMAND} ${build_args}
            -DOPEN_SRC_INSTALL_PREFIX=${OPEN_SRC_INSTALL_PREFIX} -G
            ${CMAKE_GENERATOR} .
    RESULT_VARIABLE result
    WORKING_DIRECTORY ${OPEN_SRC_INSTALL_PREFIX}/lib${lib_name})
  if(result)
    message(FATAL_ERROR "CMake step for lib${lib_name} failed: ${result}")
  endif()
  execute_process(
    COMMAND ${CMAKE_COMMAND} --build . ${PARALLEL_BUILD}
    RESULT_VARIABLE result
    WORKING_DIRECTORY ${OPEN_SRC_INSTALL_PREFIX}/lib${lib_name})
  if(result)
    message(FATAL_ERROR "CMake step for lib${lib_name} failed: ${result}")
  endif()

  file(REMOVE_RECURSE ${OPEN_SRC_INSTALL_PREFIX}/lib${lib_name})
endfunction()

function(enableSanitizer SANITIZER)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -g -fsanitize=${SANITIZER} -fno-omit-frame-pointer" PARENT_SCOPE)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -g -fsanitize=${SANITIZER} -fno-omit-frame-pointer -fno-optimize-sibling-calls" PARENT_SCOPE)
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=${SANITIZER}" PARENT_SCOPE)
endfunction()

# Returns a list of arguments that evaluate to true
function(count_true output_count_var)
  set(lst_len 0)
  foreach(option_var IN LISTS ARGN)
    if(${option_var})
      math(EXPR lst_len "${lst_len} + 1")
    endif()
  endforeach()
  set(${output_count_var} ${lst_len} PARENT_SCOPE)
endfunction()
