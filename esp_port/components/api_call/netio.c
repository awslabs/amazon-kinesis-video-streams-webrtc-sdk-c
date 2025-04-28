/*
 * Copyright 2021 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *  http://aws.amazon.com/apache2.0
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */
/******************************************************************************
 * HEADERS
 ******************************************************************************/
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <sys/time.h>
#include <sys/socket.h>

#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include "mbedtls/error.h"
#if MBEDTLS_VERSION_NUMBER < 0x03000000
#include "mbedtls/net.h"
#endif
#include "mbedtls/net_sockets.h"

/* Public headers */
#include "error.h"
#include "platform_utils.h"
/* Internal headers */
#include "netio.h"
#include "fileio.h"

/******************************************************************************
 * DEFINITIONS
 ******************************************************************************/
#define DEFAULT_CONNECTION_TIMEOUT_MS (5 * 1000)

INT32 net_getErrorCode(VOID)
{
    return errno;
}

PCHAR net_getErrorString(INT32 error)
{
    return strerror(error);
}

typedef struct NetIo {
    /* Basic ssl connection parameters */
    mbedtls_net_context xFd;
    mbedtls_ssl_context xSsl;
    mbedtls_ssl_config xConf;
    mbedtls_ctr_drbg_context xCtrDrbg;
    mbedtls_entropy_context xEntropy;

    /* Variables for IoT credential provider. It's optional feature so we declare them as pointers. */
    mbedtls_x509_crt* pRootCA;
    mbedtls_x509_crt* pCert;
    mbedtls_pk_context* pPrivKey;

    /* Options */
    uint32_t uRecvTimeoutMs;
    uint32_t uSendTimeoutMs;
} NetIo_t;

/******************************************************************************
 * FUNCTIONS
 ******************************************************************************/
static int prvCreateX509Cert(NetIo_t* pxNet)
{
    int xRes = STATUS_SUCCESS;

    if (pxNet == NULL || (pxNet->pRootCA = (mbedtls_x509_crt*) MEMALLOC(sizeof(mbedtls_x509_crt))) == NULL ||
        (pxNet->pCert = (mbedtls_x509_crt*) MEMALLOC(sizeof(mbedtls_x509_crt))) == NULL ||
        (pxNet->pPrivKey = (mbedtls_pk_context*) MEMALLOC(sizeof(mbedtls_pk_context))) == NULL) {
        xRes = STATUS_NULL_ARG;
    } else {
        mbedtls_x509_crt_init(pxNet->pRootCA);
        mbedtls_x509_crt_init(pxNet->pCert);
        mbedtls_pk_init(pxNet->pPrivKey);
    }

    return xRes;
}

#if MBEDTLS_VERSION_NUMBER >= 0x03000000
static mbedtls_ctr_drbg_context ctr_drbg;
static mbedtls_entropy_context entropy;
#endif

static int prvReadAndParseCertificate(mbedtls_x509_crt* pCert, const char* pcPath)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT64 cert_len = 0;
    PBYTE cert_buf = NULL;
    CHAR errBuf[128];

    if (readFile(pcPath, FALSE, NULL, &cert_len) != STATUS_SUCCESS) {
        DLOGE("Failed to get cert file size");
        retStatus = STATUS_NULL_ARG;
        goto CleanUp;
    }

    cert_buf = (PBYTE) MEMCALLOC(1, cert_len + 1);
    CHK(cert_buf != NULL, STATUS_NOT_ENOUGH_MEMORY);
    CHK_STATUS(readFile(pcPath, FALSE, cert_buf, &cert_len));
    int ret = mbedtls_x509_crt_parse(pCert, cert_buf, (size_t) (cert_len + 1));
    if (ret != 0) {
        mbedtls_strerror(ret, errBuf, SIZEOF(errBuf));
        DLOGE("mbedtls_x509_crt_parse failed: %s", errBuf);
    }
    CHK(ret == 0, STATUS_INVALID_CA_CERT_PATH);

CleanUp:

    CHK_LOG_ERR(retStatus);
    SAFE_MEMFREE(cert_buf);
    return retStatus;
}

static int prvReadAndParsePrivateKey(mbedtls_pk_context* pPrivKey, const char* pcPath)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT64 key_len = 0;
    PBYTE key_buf = NULL;
    CHAR errBuf[128];

    if (readFile(pcPath, FALSE, NULL, &key_len) != STATUS_SUCCESS) {
        DLOGE("Failed to get private key file size");
        retStatus = STATUS_NULL_ARG;
        goto CleanUp;
    }

    key_buf = (PBYTE) MEMCALLOC(1, key_len + 1);
    CHK(key_buf != NULL, STATUS_NOT_ENOUGH_MEMORY);
    CHK_STATUS(readFile(pcPath, FALSE, key_buf, &key_len));
#if MBEDTLS_VERSION_NUMBER < 0x03000000
    int ret = mbedtls_pk_parse_key(pPrivKey, key_buf, key_len + 1, NULL, 0);
#else
    int ret = mbedtls_pk_parse_key(pPrivKey, key_buf, key_len + 1, NULL, 0, &mbedtls_ctr_drbg_random, &ctr_drbg);
#endif
    if (ret != 0) {
        mbedtls_strerror(ret, errBuf, SIZEOF(errBuf));
        DLOGE("mbedtls_pk_parse_key failed: %s", errBuf);
    }
    CHK(ret == 0, STATUS_FILE_CREDENTIAL_PROVIDER_OPEN_FILE_FAILED);

CleanUp:

    SAFE_MEMFREE(key_buf);
    return retStatus;
}

static int prvInitConfig(NetIo_t* pxNet, const char* pcRootCA, const char* pcCert, const char* pcPrivKey, bool bFilePath)
{
    int xRes = STATUS_SUCCESS;

#if (MBEDTLS_VERSION_NUMBER >= 0x03000000)
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    int mbedtls_ctr_ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, NULL, 0);
    if (mbedtls_ctr_ret != 0) {
        DLOGE("mbedtls_ctr_drbg_seed Error -0x%04x", mbedtls_ctr_ret);
    }
#endif

    if (pxNet == NULL) {
        xRes = STATUS_NULL_ARG;
    } else {
        mbedtls_ssl_set_bio(&(pxNet->xSsl), &(pxNet->xFd), mbedtls_net_send, mbedtls_net_recv, mbedtls_net_recv_timeout);
        if (mbedtls_ssl_config_defaults(&(pxNet->xConf), MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT) != 0) {
            DLOGE("Failed to config ssl");
            xRes = STATUS_NULL_ARG;
        } else {
            mbedtls_ssl_conf_rng(&(pxNet->xConf), mbedtls_ctr_drbg_random, &(pxNet->xCtrDrbg));
            mbedtls_ssl_conf_read_timeout(&(pxNet->xConf), pxNet->uRecvTimeoutMs);
            NetIo_setSendTimeout(pxNet, pxNet->uSendTimeoutMs);

            if (pcRootCA != NULL && pcCert != NULL && pcPrivKey != NULL) {
                if (bFilePath) {
                    // Read and parse certificates from files
                    if (prvReadAndParseCertificate(pxNet->pRootCA, pcRootCA) != STATUS_SUCCESS ||
                        prvReadAndParseCertificate(pxNet->pCert, pcCert) != STATUS_SUCCESS ||
                        prvReadAndParsePrivateKey(pxNet->pPrivKey, pcPrivKey) != STATUS_SUCCESS) {
                        xRes = STATUS_NULL_ARG;
                        goto CleanUp;
                    }
                } else {
                    // Parse certificates from memory buffers
                    if (mbedtls_x509_crt_parse(pxNet->pRootCA, (void*) pcRootCA, strlen(pcRootCA) + 1) != 0 ||
                        mbedtls_x509_crt_parse(pxNet->pCert, (void*) pcCert, strlen(pcCert) + 1) != 0 ||
#if MBEDTLS_VERSION_NUMBER < 0x03000000
                        mbedtls_pk_parse_key(pxNet->pPrivKey, (void*) pcPrivKey, strlen(pcPrivKey) + 1, NULL, 0) != 0) {
#else
                        mbedtls_pk_parse_key(pxNet->pPrivKey, (void*) pcPrivKey, strlen(pcPrivKey) + 1, NULL, 0, &mbedtls_ctr_drbg_random, &ctr_drbg) != 0) {
#endif
                        DLOGE("Failed to parse x509");
                        xRes = STATUS_NULL_ARG;
                        goto CleanUp;
                    }
                }

                mbedtls_ssl_conf_authmode(&(pxNet->xConf), MBEDTLS_SSL_VERIFY_REQUIRED);
                mbedtls_ssl_conf_ca_chain(&(pxNet->xConf), pxNet->pRootCA, NULL);

                if (mbedtls_ssl_conf_own_cert(&(pxNet->xConf), pxNet->pCert, pxNet->pPrivKey) != 0) {
                    DLOGE("Failed to conf own cert");
                    xRes = STATUS_NULL_ARG;
                }
            } else {
                mbedtls_ssl_conf_authmode(&(pxNet->xConf), MBEDTLS_SSL_VERIFY_OPTIONAL);
            }
        }
    }

    if (xRes == STATUS_SUCCESS) {
        if (mbedtls_ssl_setup(&(pxNet->xSsl), &(pxNet->xConf)) != 0) {
            DLOGE("Failed to setup ssl");
            xRes = STATUS_NULL_ARG;
        }
    }

CleanUp:
    return xRes;
}

static int prvConnect(NetIo_t* pxNet, const char* pcHost, const char* pcPort, const char* pcRootCA, const char* pcCert, const char* pcPrivKey,
                      bool bFilePath)
{
    int xRes = STATUS_SUCCESS;
    int ret = 0;
    DLOGD("Reached here: %s %d\n", __func__, __LINE__);
    if (pxNet == NULL || pcHost == NULL || pcPort == NULL) {
        DLOGE("Invalid argument");
        xRes = STATUS_NULL_ARG;
    } else if ((pcRootCA != NULL && pcCert != NULL && pcPrivKey != NULL) && prvCreateX509Cert(pxNet) != STATUS_SUCCESS) {
        DLOGE("Failed to init x509");
        xRes = STATUS_NULL_ARG;
    } else if ((ret = mbedtls_net_connect(&(pxNet->xFd), pcHost, pcPort, MBEDTLS_NET_PROTO_TCP)) != 0) {
        DLOGE("Failed to connect to %s:%s", pcHost, pcPort);
        xRes = STATUS_NULL_ARG;
    } else if ((ret = mbedtls_ssl_set_hostname(&(pxNet->xSsl), pcHost)) != 0) {
        DLOGE("Failed to set hostname %s", pcHost);
        xRes = STATUS_NULL_ARG;
    } else if (prvInitConfig(pxNet, pcRootCA, pcCert, pcPrivKey, bFilePath) != STATUS_SUCCESS) {
        DLOGE("Failed to config ssl");
        xRes = STATUS_NULL_ARG;
    } else if ((ret = mbedtls_ssl_handshake(&(pxNet->xSsl))) != 0) {
        DLOGE("ssl handshake err (%d)", ret);
        xRes = STATUS_NULL_ARG;
    } else {
        /* nop */
        DLOGD("Reached here: %s %d", __func__, __LINE__);
    }
    DLOGD("Reached here: %s %d\n", __func__, __LINE__);
    return xRes;
}

NetIoHandle NetIo_create(void)
{
    NetIo_t* pxNet = NULL;

    if ((pxNet = (NetIo_t*) MEMALLOC(sizeof(NetIo_t))) != NULL) {
        memset(pxNet, 0, sizeof(NetIo_t));

        mbedtls_net_init(&(pxNet->xFd));
        mbedtls_ssl_init(&(pxNet->xSsl));
        mbedtls_ssl_config_init(&(pxNet->xConf));
        mbedtls_ctr_drbg_init(&(pxNet->xCtrDrbg));
        mbedtls_entropy_init(&(pxNet->xEntropy));

        pxNet->uRecvTimeoutMs = DEFAULT_CONNECTION_TIMEOUT_MS;
        pxNet->uSendTimeoutMs = DEFAULT_CONNECTION_TIMEOUT_MS;

        if (mbedtls_ctr_drbg_seed(&(pxNet->xCtrDrbg), mbedtls_entropy_func, &(pxNet->xEntropy), NULL, 0) != 0) {
            NetIo_terminate(pxNet);
            pxNet = NULL;
        }
    }

    return pxNet;
}

void NetIo_terminate(NetIoHandle xNetIoHandle)
{
    NetIo_t* pxNet = (NetIo_t*) xNetIoHandle;

    if (pxNet != NULL) {
        mbedtls_ctr_drbg_free(&(pxNet->xCtrDrbg));
        mbedtls_entropy_free(&(pxNet->xEntropy));
        mbedtls_net_free(&(pxNet->xFd));
        mbedtls_ssl_free(&(pxNet->xSsl));
        mbedtls_ssl_config_free(&(pxNet->xConf));

        if (pxNet->pRootCA != NULL) {
            mbedtls_x509_crt_free(pxNet->pRootCA);
            SAFE_MEMFREE(pxNet->pRootCA);
            pxNet->pRootCA = NULL;
        }

        if (pxNet->pCert != NULL) {
            mbedtls_x509_crt_free(pxNet->pCert);
            SAFE_MEMFREE(pxNet->pCert);
            pxNet->pCert = NULL;
        }

        if (pxNet->pPrivKey != NULL) {
            mbedtls_pk_free(pxNet->pPrivKey);
            SAFE_MEMFREE(pxNet->pPrivKey);
            pxNet->pPrivKey = NULL;
        }
        SAFE_MEMFREE(pxNet);
    }
}

#include "esp_heap_caps.h"

int NetIo_connect(NetIoHandle xNetIoHandle, const char* pcHost, const char* pcPort)
{
    DLOGD("Reached here: %s %d", __func__, __LINE__);
    // if (heap_caps_check_integrity_all(true) == false) {
    //     DLOGE("Heap integrity check failed, line %d", __LINE__);
    // }

    return prvConnect(xNetIoHandle, pcHost, pcPort, NULL, NULL, NULL, false);
}

int NetIo_connectWithX509(NetIoHandle xNetIoHandle, const char* pcHost, const char* pcPort, const char* pcRootCA, const char* pcCert,
                          const char* pcPrivKey)
{
    return prvConnect(xNetIoHandle, pcHost, pcPort, pcRootCA, pcCert, pcPrivKey, false);
}

int NetIo_connectWithX509Path(NetIoHandle xNetIoHandle, const char* pcHost, const char* pcPort, const char* pcRootCA, const char* pcCert,
                              const char* pcPrivKey)
{
    return prvConnect(xNetIoHandle, pcHost, pcPort, pcRootCA, pcCert, pcPrivKey, true);
}

void NetIo_disconnect(NetIoHandle xNetIoHandle)
{
    NetIo_t* pxNet = (NetIo_t*) xNetIoHandle;

    if (pxNet != NULL) {
        mbedtls_ssl_close_notify(&(pxNet->xSsl));
    }
}

int NetIo_send(NetIoHandle xNetIoHandle, const unsigned char* pBuffer, size_t uBytesToSend)
{
    int n = 0;
    int xRes = STATUS_SUCCESS;
    NetIo_t* pxNet = (NetIo_t*) xNetIoHandle;
    size_t uBytesRemaining = uBytesToSend;
    char* pIndex = (char*) pBuffer;

    if (pxNet == NULL || pBuffer == NULL) {
        xRes = -1;
        DLOGE("NetIo_send null arg");
    } else {
        do {
            n = mbedtls_ssl_write(&(pxNet->xSsl), (const unsigned char*) pIndex, uBytesRemaining);
            if (n < 0 || n > uBytesRemaining) {
                DLOGE("SSL send error %d", n);
                xRes = n;
                break;
            }
            uBytesRemaining -= n;
            pIndex += n;
        } while (uBytesRemaining > 0);
    }

    return xRes;
}

int NetIo_recv(NetIoHandle xNetIoHandle, unsigned char* pBuffer, size_t uBufferSize, size_t* puBytesReceived)
{
    int n;
    int xRes = STATUS_SUCCESS;
    NetIo_t* pxNet = (NetIo_t*) xNetIoHandle;

    if (pxNet == NULL || pBuffer == NULL || puBytesReceived == NULL) {
        xRes = STATUS_NULL_ARG;
    } else {
        n = mbedtls_ssl_read(&(pxNet->xSsl), pBuffer, uBufferSize);
        if (n < 0 || n > uBufferSize) {
            // DLOGE("SSL recv error %d", n);
            xRes = n;
            *puBytesReceived = 0;
        } else {
            *puBytesReceived = n;
        }
    }

    return xRes;
}

bool NetIo_isDataAvailable(NetIoHandle xNetIoHandle)
{
    NetIo_t* pxNet = (NetIo_t*) xNetIoHandle;
    bool bDataAvailable = false;
    struct timeval tv = {0};
    fd_set read_fds = {0};
    int fd = 0;

    if (pxNet) {
        fd = pxNet->xFd.fd;
        if (fd >= 0) {
            FD_ZERO(&read_fds);
            FD_SET(fd, &read_fds);

            tv.tv_sec = 0;
            tv.tv_usec = 0;

            if (select(fd + 1, &read_fds, NULL, NULL, &tv) >= 0) {
                if (FD_ISSET(fd, &read_fds)) {
                    bDataAvailable = true;
                }
            }
        }
    }

    return bDataAvailable;
}

int NetIo_setRecvTimeout(NetIoHandle xNetIoHandle, unsigned int uRecvTimeoutMs)
{
    int xRes = STATUS_SUCCESS;
    NetIo_t* pxNet = (NetIo_t*) xNetIoHandle;

    if (pxNet == NULL) {
        xRes = STATUS_NULL_ARG;
    } else {
        pxNet->uRecvTimeoutMs = (uint32_t) uRecvTimeoutMs;
        mbedtls_ssl_conf_read_timeout(&(pxNet->xConf), pxNet->uRecvTimeoutMs);
    }

    return xRes;
}

int NetIo_setSendTimeout(NetIoHandle xNetIoHandle, unsigned int uSendTimeoutMs)
{
    int xRes = STATUS_SUCCESS;
    NetIo_t* pxNet = (NetIo_t*) xNetIoHandle;
    int fd = 0;
    struct timeval tv = {0};

    if (pxNet == NULL) {
        xRes = STATUS_NULL_ARG;
    } else {
        pxNet->uSendTimeoutMs = (uint32_t) uSendTimeoutMs;
        fd = pxNet->xFd.fd;
        tv.tv_sec = uSendTimeoutMs / 1000;
        tv.tv_usec = (uSendTimeoutMs % 1000) * 1000;

        if (fd < 0) {
            /* Do nothing when connection hasn't established. */
        } else if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, (void*) &tv, sizeof(tv)) != 0) {
            xRes = STATUS_NULL_ARG;
        } else {
            /* nop */
        }
    }

    return xRes;
}

uint32_t NetIo_getSocket(NetIoHandle xNetIoHandle)
{
    return xNetIoHandle->xFd.fd;
}
