#define LOG_CLASS "Crypto"
#include "../Include_i.h"

STATUS generateMd5Digest(PBYTE srcBuffer, UINT64 len, PBYTE destBuffer)
{
    STATUS retStatus = STATUS_SUCCESS;
    DLOGI("New MD5 digest");
    EVP_MD_CTX* mdctx;
    const EVP_MD* md;
    CHK_ERR(md = EVP_MD_fetch(NULL, "MD5", NULL), STATUS_INVALID_OPERATION, "Failed to fetch MD5 provider");
    mdctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(mdctx, md, NULL);
    EVP_DigestUpdate(mdctx, srcBuffer, len);
    EVP_DigestFinal_ex(mdctx, destBuffer, NULL);
CleanUp:
    EVP_MD_CTX_free(mdctx);
    EVP_MD_free((EVP_MD*) md);
    return retStatus;
}

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
