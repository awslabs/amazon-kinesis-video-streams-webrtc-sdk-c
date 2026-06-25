#define LOG_CLASS "Crypto"
#include "../Include_i.h"

#if defined(KVS_USE_OPENSSL) && OPENSSL_VERSION_NUMBER >= 0x30000000L
STATUS kvsSha1Hmac(const PBYTE pKey, size_t keyLen, const PBYTE pMessage, size_t messageLen, PBYTE pOutput, PUINT32 pOutputLen)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    EVP_MAC* pMac = NULL;
    EVP_MAC_CTX* pCtx = NULL;
    OSSL_PARAM params[2];
    size_t outLen = 0;
    UINT64 sslErr;

    CHK(pKey != NULL && pMessage != NULL && pOutput != NULL && pOutputLen != NULL, STATUS_NULL_ARG);

    params[0] = OSSL_PARAM_construct_utf8_string("digest", "SHA1", 0);
    params[1] = OSSL_PARAM_construct_end();

    if ((pMac = EVP_MAC_fetch(NULL, "HMAC", NULL)) == NULL) {
        LOG_OPENSSL_ERROR("EVP_MAC_fetch");
        retStatus = STATUS_HMAC_GENERATION_ERROR;
        goto CleanUp;
    }
    if ((pCtx = EVP_MAC_CTX_new(pMac)) == NULL) {
        LOG_OPENSSL_ERROR("EVP_MAC_CTX_new");
        retStatus = STATUS_HMAC_GENERATION_ERROR;
        goto CleanUp;
    }
    if (EVP_MAC_init(pCtx, pKey, keyLen, params) != 1) {
        LOG_OPENSSL_ERROR("EVP_MAC_init");
        retStatus = STATUS_HMAC_GENERATION_ERROR;
        goto CleanUp;
    }
    if (EVP_MAC_update(pCtx, pMessage, messageLen) != 1) {
        LOG_OPENSSL_ERROR("EVP_MAC_update");
        retStatus = STATUS_HMAC_GENERATION_ERROR;
        goto CleanUp;
    }
    if (EVP_MAC_final(pCtx, pOutput, &outLen, EVP_MAX_MD_SIZE) != 1) {
        LOG_OPENSSL_ERROR("EVP_MAC_final");
        retStatus = STATUS_HMAC_GENERATION_ERROR;
        goto CleanUp;
    }
    *pOutputLen = (UINT32) outLen;

CleanUp:
    if (pCtx != NULL) {
        EVP_MAC_CTX_free(pCtx);
    }
    if (pMac != NULL) {
        EVP_MAC_free(pMac);
    }

    LEAVES();
    return retStatus;
}
#endif

STATUS createRtcCertificate(PRtcCertificate* ppRtcCertificate)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT64 startTimeInMacro = 0;
    PRtcCertificate pRtcCertificate = NULL;

    CHK(ppRtcCertificate != NULL, STATUS_NULL_ARG);

    CHK(NULL != (pRtcCertificate = (PRtcCertificate) MEMCALLOC(1, SIZEOF(RtcCertificate))), STATUS_NOT_ENOUGH_MEMORY);

#ifdef KVS_USE_OPENSSL
    PROFILE_CALL(CHK_STATUS(createCertificateAndKey(GENERATED_CERTIFICATE_BITS, FALSE, (X509**) &pRtcCertificate->pCertificate,
                                                    (EVP_PKEY**) &pRtcCertificate->pPrivateKey)),
                 "Certificate creation time");
#elif KVS_USE_MBEDTLS
    // Need to allocate space for the cert and the key for mbedTLS
    CHK(NULL != (pRtcCertificate->pCertificate = (PBYTE) MEMCALLOC(1, SIZEOF(mbedtls_x509_crt))), STATUS_NOT_ENOUGH_MEMORY);
    CHK(NULL != (pRtcCertificate->pPrivateKey = (PBYTE) MEMCALLOC(1, SIZEOF(mbedtls_pk_context))), STATUS_NOT_ENOUGH_MEMORY);
    pRtcCertificate->certificateSize = SIZEOF(mbedtls_x509_crt);
    pRtcCertificate->privateKeySize = SIZEOF(mbedtls_pk_context);
    PROFILE_CALL(CHK_STATUS(createCertificateAndKey(GENERATED_CERTIFICATE_BITS, FALSE, (mbedtls_x509_crt*) pRtcCertificate->pCertificate,
                                                    (mbedtls_pk_context*) pRtcCertificate->pPrivateKey)),
                 "Certificate creation time");
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
