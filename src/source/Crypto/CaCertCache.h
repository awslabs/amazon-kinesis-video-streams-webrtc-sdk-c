#ifndef __KINESIS_VIDEO_WEBRTC_CLIENT_CRYPTO_CA_CERT_CACHE__
#define __KINESIS_VIDEO_WEBRTC_CLIENT_CRYPTO_CA_CERT_CACHE__

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the CA certificate cache by reading the cert file once into memory.
 * Must be called before any signaling client or TURN connections are created.
 * NOT THREAD SAFE - call once at application startup before spawning worker threads.
 *
 * @param[in] pCaCertPath Path to the CA certificate PEM file
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success
 */
STATUS initCaCertCache(PCHAR pCaCertPath);

/**
 * @brief Free the CA certificate cache.
 * NOT THREAD SAFE - call once at application shutdown after all connections are closed.
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success
 */
STATUS deinitCaCertCache(VOID);

/**
 * @brief Get the cached CA certificate buffer and length.
 *
 * @param[out] ppCaCertBuf Pointer to the cached cert buffer (PEM). NULL if not initialized.
 * @param[out] pCaCertBufLen Length of the cached cert buffer
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success
 */
STATUS getCachedCaCert(PBYTE* ppCaCertBuf, PUINT32 pCaCertBufLen);

#ifdef __cplusplus
}
#endif
#endif /* __KINESIS_VIDEO_WEBRTC_CLIENT_CRYPTO_CA_CERT_CACHE__ */
