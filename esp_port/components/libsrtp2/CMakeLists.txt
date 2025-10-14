set(SRTP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/libsrtp")

# Apply patch to fix srtp_remove_stream signature
execute_process(
    COMMAND patch -p1 -N --dry-run -i ${CMAKE_CURRENT_SOURCE_DIR}/patches/srtp.patch
    WORKING_DIRECTORY ${SRTP_ROOT}
    RESULT_VARIABLE PATCH_RESULT
    ERROR_QUIET
)

if(PATCH_RESULT EQUAL 0)
    execute_process(
        COMMAND patch -p1 -N -i ${CMAKE_CURRENT_SOURCE_DIR}/patches/srtp.patch
        WORKING_DIRECTORY ${SRTP_ROOT}
    )
endif()

idf_component_register(
    SRCS "${SRTP_ROOT}/srtp/srtp.c"
        "${SRTP_ROOT}/crypto/cipher/aes_gcm_mbedtls.c"
        "${SRTP_ROOT}/crypto/cipher/aes_icm_mbedtls.c"
        "${SRTP_ROOT}/crypto/cipher/aes_icm.c"
        "${SRTP_ROOT}/crypto/cipher/aes.c"
        "${SRTP_ROOT}/crypto/cipher/cipher.c"
        "${SRTP_ROOT}/crypto/cipher/cipher_test_cases.c"
        "${SRTP_ROOT}/crypto/cipher/null_cipher.c"
        "${SRTP_ROOT}/crypto/hash/auth.c"
        "${SRTP_ROOT}/crypto/hash/hmac.c"
        "${SRTP_ROOT}/crypto/hash/hmac_mbedtls.c"
        "${SRTP_ROOT}/crypto/hash/auth_test_cases.c"
        "${SRTP_ROOT}/crypto/hash/null_auth.c"
        "${SRTP_ROOT}/crypto/hash/sha1.c"
        "${SRTP_ROOT}/crypto/kernel/crypto_kernel.c"
        "${SRTP_ROOT}/crypto/kernel/alloc.c"
        "${SRTP_ROOT}/crypto/kernel/err.c"
        "${SRTP_ROOT}/crypto/kernel/key.c"
        "${SRTP_ROOT}/crypto/math/datatypes.c"
        "${SRTP_ROOT}/crypto/replay/rdb.c"
        "${SRTP_ROOT}/crypto/replay/rdbx.c"

    PRIV_INCLUDE_DIRS
        "${SRTP_ROOT}/include"
        "${SRTP_ROOT}/crypto/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/port"
    INCLUDE_DIRS
        "${CMAKE_CURRENT_SOURCE_DIR}/include"

    REQUIRES "mbedtls"
)

# Reduce paranoia while building libsrtp2
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wno-error=format
    -Wno-error=unused-function
    -Wno-error=unused-variable
    -Wno-error=unused-but-set-variable
    -Wno-error=sign-compare
    -Wno-error=type-limits
    -Wno-error=pointer-sign
    -Wno-error=implicit-function-declaration
    -Wno-error=strict-prototypes
    -Wno-error=old-style-definition
    -Wno-error=incompatible-pointer-types
    -Wno-conflicting-types
)

target_compile_definitions(${COMPONENT_LIB} PUBLIC
    -DHAVE_CONFIG_H
)
