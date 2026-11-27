# FindAWSLC.cmake - locate an AWS-LC installation.
#
# Why this exists instead of just calling find_package(OpenSSL):
#
# AWS-LC installs OpenSSL-compatible headers (include/openssl/*.h) and library
# names (libssl, libcrypto), so CMake's bundled FindOpenSSL module usually does
# find it. But FindOpenSSL then parses OPENSSL_VERSION_NUMBER out of the headers
# and reports "1.1.1" - the compatibility value AWS-LC advertises - which makes
# any downstream version gate silently wrong. Worse, it cannot tell AWS-LC apart
# from a real OpenSSL 1.1.1, so a stale system OpenSSL on the include path can
# be picked up instead of the AWS-LC we just built.
#
# This module keys off OPENSSL_IS_AWSLC in <openssl/base.h>, which only AWS-LC
# defines, so a match is unambiguous.
#
# Sets:
#   AWSLC_FOUND
#   AWSLC_INCLUDE_DIR
#   AWSLC_SSL_LIBRARY
#   AWSLC_CRYPTO_LIBRARY
#   AWSLC_LIBRARIES
#   AWSLC_VERSION          e.g. "5.9.0"
#   AWSLC_API_VERSION      AWS-LC's own ABI counter
#
# Also populates the OPENSSL_* variables the rest of this project already
# consumes, so no downstream CMake needs to know which backend is in play:
#   OPENSSL_INCLUDE_DIR, OPENSSL_SSL_LIBRARY, OPENSSL_CRYPTO_LIBRARY,
#   OPENSSL_LIBRARIES, OPENSSL_FOUND
# and defines the OpenSSL::SSL / OpenSSL::Crypto imported targets.

find_path(AWSLC_INCLUDE_DIR
    NAMES openssl/base.h
    HINTS ${AWSLC_ROOT_DIR} ${OPEN_SRC_INSTALL_PREFIX}
    PATH_SUFFIXES include
    NO_DEFAULT_PATH)

# Fall back to default search paths for a system-installed AWS-LC
# (-DBUILD_DEPENDENCIES=OFF -DUSE_AWS_LC=ON).
if (NOT AWSLC_INCLUDE_DIR)
  find_path(AWSLC_INCLUDE_DIR NAMES openssl/base.h PATH_SUFFIXES include)
endif()

# Reject a non-AWS-LC OpenSSL that happens to sit at the same prefix.
set(_awslc_is_awslc FALSE)
if (AWSLC_INCLUDE_DIR AND EXISTS "${AWSLC_INCLUDE_DIR}/openssl/base.h")
  file(STRINGS "${AWSLC_INCLUDE_DIR}/openssl/base.h" _awslc_marker
       REGEX "^#define[ \t]+OPENSSL_IS_AWSLC")
  if (_awslc_marker)
    set(_awslc_is_awslc TRUE)

    file(STRINGS "${AWSLC_INCLUDE_DIR}/openssl/base.h" _awslc_ver_line
         REGEX "^#define[ \t]+AWSLC_VERSION_NUMBER_STRING[ \t]+\"[^\"]+\"")
    if (_awslc_ver_line)
      string(REGEX REPLACE ".*\"([^\"]+)\".*" "\\1" AWSLC_VERSION "${_awslc_ver_line}")
    endif()

    file(STRINGS "${AWSLC_INCLUDE_DIR}/openssl/base.h" _awslc_api_line
         REGEX "^#define[ \t]+AWSLC_API_VERSION[ \t]+[0-9]+")
    if (_awslc_api_line)
      string(REGEX REPLACE "[^0-9]+([0-9]+).*" "\\1" AWSLC_API_VERSION "${_awslc_api_line}")
    endif()
  endif()
endif()

if (NOT _awslc_is_awslc)
  # Do not leak a non-AWS-LC include dir into the OPENSSL_* variables.
  unset(AWSLC_INCLUDE_DIR CACHE)
endif()

find_library(AWSLC_SSL_LIBRARY
    NAMES ssl libssl
    HINTS ${AWSLC_ROOT_DIR} ${OPEN_SRC_INSTALL_PREFIX}
    PATH_SUFFIXES lib lib64
    NO_DEFAULT_PATH)
if (NOT AWSLC_SSL_LIBRARY)
  find_library(AWSLC_SSL_LIBRARY NAMES ssl libssl PATH_SUFFIXES lib lib64)
endif()

find_library(AWSLC_CRYPTO_LIBRARY
    NAMES crypto libcrypto
    HINTS ${AWSLC_ROOT_DIR} ${OPEN_SRC_INSTALL_PREFIX}
    PATH_SUFFIXES lib lib64
    NO_DEFAULT_PATH)
if (NOT AWSLC_CRYPTO_LIBRARY)
  find_library(AWSLC_CRYPTO_LIBRARY NAMES crypto libcrypto PATH_SUFFIXES lib lib64)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(AWSLC
    REQUIRED_VARS AWSLC_SSL_LIBRARY AWSLC_CRYPTO_LIBRARY AWSLC_INCLUDE_DIR
    VERSION_VAR AWSLC_VERSION
    FAIL_MESSAGE
      "Could not find AWS-LC. AWSLC_INCLUDE_DIR is only accepted when <openssl/base.h> defines OPENSSL_IS_AWSLC, so a plain OpenSSL installation at the same prefix is rejected on purpose rather than silently linked. Searched: ${AWSLC_ROOT_DIR} ${OPEN_SRC_INSTALL_PREFIX}. If the prefix holds a previous OpenSSL build, remove it (rm -rf ${OPEN_SRC_INSTALL_PREFIX}) and reconfigure, or pass -DOPEN_SRC_INSTALL_PREFIX=<new-dir>.")

if (AWSLC_FOUND)
  set(AWSLC_LIBRARIES ${AWSLC_SSL_LIBRARY} ${AWSLC_CRYPTO_LIBRARY})

  # OpenSSL-compatible aliases so the rest of the build is backend-agnostic.
  set(OPENSSL_FOUND TRUE)
  set(OPENSSL_INCLUDE_DIR ${AWSLC_INCLUDE_DIR})
  set(OPENSSL_INCLUDE_DIRS ${AWSLC_INCLUDE_DIR})
  set(OPENSSL_SSL_LIBRARY ${AWSLC_SSL_LIBRARY})
  set(OPENSSL_CRYPTO_LIBRARY ${AWSLC_CRYPTO_LIBRARY})
  set(OPENSSL_LIBRARIES ${AWSLC_LIBRARIES})

  if (NOT TARGET OpenSSL::Crypto)
    add_library(OpenSSL::Crypto UNKNOWN IMPORTED)
    set_target_properties(OpenSSL::Crypto PROPERTIES
        IMPORTED_LOCATION "${AWSLC_CRYPTO_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${AWSLC_INCLUDE_DIR}")
  endif()

  if (NOT TARGET OpenSSL::SSL)
    add_library(OpenSSL::SSL UNKNOWN IMPORTED)
    set_target_properties(OpenSSL::SSL PROPERTIES
        IMPORTED_LOCATION "${AWSLC_SSL_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${AWSLC_INCLUDE_DIR}"
        INTERFACE_LINK_LIBRARIES OpenSSL::Crypto)
  endif()

  message(STATUS "Found AWS-LC ${AWSLC_VERSION} (API ${AWSLC_API_VERSION}) at ${AWSLC_INCLUDE_DIR}")
endif()

mark_as_advanced(AWSLC_INCLUDE_DIR AWSLC_SSL_LIBRARY AWSLC_CRYPTO_LIBRARY)
