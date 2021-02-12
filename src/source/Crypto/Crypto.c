#define LOG_CLASS "Crypto"
#include "../Include_i.h"

STATUS createRtcCertificate(PRtcCertificate* ppRtcCertificate)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PRtcCertificate pRtcCertificate = NULL;

    CHK(ppRtcCertificate != NULL, STATUS_NULL_ARG);

    CHK(NULL != (pRtcCertificate = (PRtcCertificate) MEMCALLOC(1, SIZEOF(RtcCertificate))), STATUS_NOT_ENOUGH_MEMORY);

#ifdef KVS_USE_OPENSSL
    CHK_STATUS(createCertificateAndKey(GENERATED_CERTIFICATE_BITS, FALSE, (X509**) &pRtcCertificate->pCertificate,
                                       (EVP_PKEY**) &pRtcCertificate->pPrivateKey));
#elif KVS_USE_MBEDTLS
    // Need to allocate space for the cert and the key for mbedTLS
    CHK(NULL != (pRtcCertificate->pCertificate = (PBYTE) MEMCALLOC(1, SIZEOF(mbedtls_x509_crt))), STATUS_NOT_ENOUGH_MEMORY);
    CHK(NULL != (pRtcCertificate->pPrivateKey = (PBYTE) MEMCALLOC(1, SIZEOF(mbedtls_pk_context))), STATUS_NOT_ENOUGH_MEMORY);
    pRtcCertificate->certificateSize = SIZEOF(mbedtls_x509_crt);
    pRtcCertificate->privateKeySize = SIZEOF(mbedtls_pk_context);
    CHK_STATUS(createCertificateAndKey(GENERATED_CERTIFICATE_BITS, FALSE, (mbedtls_x509_crt*) pRtcCertificate->pCertificate,
                                       (mbedtls_pk_context*) pRtcCertificate->pPrivateKey));
#else
#error "A Crypto implementation is required."
#endif

    *ppRtcCertificate = pRtcCertificate;

CleanUp:

    CHK_LOG_ERR(retStatus);

    if (STATUS_FAILED(retStatus) && pRtcCertificate != NULL) {
        freeRtcCertificate(pRtcCertificate);
    }

    LEAVES();
    return retStatus;
}

STATUS freeRtcCertificate(PRtcCertificate pRtcCertificate)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    // The call is idempotent
    CHK(pRtcCertificate != NULL, retStatus);

    if (pRtcCertificate->pCertificate != NULL) {
#ifdef KVS_USE_OPENSSL
        X509_free((X509*) pRtcCertificate->pCertificate);
#elif KVS_USE_MBEDTLS
        mbedtls_x509_crt_free((mbedtls_x509_crt*) pRtcCertificate->pCertificate);
        SAFE_MEMFREE(pRtcCertificate->pCertificate);
#else
#error "A Crypto implementation is required."
#endif
    }

    if (pRtcCertificate->pPrivateKey != NULL) {
#ifdef KVS_USE_OPENSSL
        EVP_PKEY_free((EVP_PKEY*) pRtcCertificate->pPrivateKey);
#elif KVS_USE_MBEDTLS
        mbedtls_pk_free((mbedtls_pk_context*) pRtcCertificate->pPrivateKey);
        SAFE_MEMFREE(pRtcCertificate->pPrivateKey);
#else
#error "A Crypto implementation is required."
#endif
    }

    MEMFREE(pRtcCertificate);

CleanUp:
    LEAVES();
    return retStatus;
}
