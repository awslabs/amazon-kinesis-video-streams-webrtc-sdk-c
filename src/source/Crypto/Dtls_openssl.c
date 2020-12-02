#define LOG_CLASS "DTLS_openssl"
#include "../Include_i.h"

// Allow all certificates since they are checked via fingerprint in SDP later
// https://www.openssl.org/docs/man1.0.2/man3/SSL_CTX_set_verify.html
INT32 dtlsCertificateVerifyCallback(INT32 preverify_ok, X509_STORE_CTX* ctx)
{
    UNUSED_PARAM(preverify_ok);
    UNUSED_PARAM(ctx);
    return 1;
}

STATUS dtlsCertificateFingerprint(X509* pCertificate, PCHAR pBuff)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    BYTE fingerprint[EVP_MAX_MD_SIZE];
    UINT32 size, i;

    CHK(X509_digest(pCertificate, EVP_sha256(), fingerprint, &size) != 0, STATUS_INTERNAL_ERROR);
    for (i = 0; i < size; i++) {
        SPRINTF(pBuff, "%.2X:", fingerprint[i]);
        pBuff += 3;
    }
    *(pBuff - 1) = '\0';

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS dtlsTransmissionTimerCallback(UINT32 timerID, UINT64 currentTime, UINT64 customData)
{
    UNUSED_PARAM(timerID);
    UNUSED_PARAM(currentTime);
    STATUS retStatus = STATUS_SUCCESS;
    PDtlsSession pDtlsSession = (PDtlsSession) customData;
    BOOL locked = FALSE;
    struct timeval timeout;
    UINT64 timeoutValDefaultTimeUnit = 0;
    LONG dtlsTimeoutRet = 0;

    CHK(pDtlsSession != NULL, STATUS_NULL_ARG);

    MEMSET(&timeout, 0x00, SIZEOF(struct timeval));

    MUTEX_LOCK(pDtlsSession->sslLock);
    locked = TRUE;

    /* In case we need to initiate the handshake */
    CHK_STATUS(dtlsCheckOutgoingDataBuffer(pDtlsSession));

    if (SSL_is_init_finished(pDtlsSession->pSsl)) {
        CHK_STATUS(dtlsSessionChangeState(pDtlsSession, RTC_DTLS_TRANSPORT_STATE_CONNECTED));
        ATOMIC_STORE_BOOL(&pDtlsSession->sslInitFinished, TRUE);
        CHK(FALSE, STATUS_TIMER_QUEUE_STOP_SCHEDULING);
    }

    /* https://commondatastorage.googleapis.com/chromium-boringssl-docs/ssl.h.html#DTLSv1_get_timeout */
    dtlsTimeoutRet = DTLSv1_get_timeout(pDtlsSession->pSsl, &timeout);
    if (dtlsTimeoutRet == 0) {
        /* try again on next iteration */
        CHK(FALSE, retStatus);
    } else if (dtlsTimeoutRet < 0) {
        CHK_ERR(FALSE, STATUS_TIMER_QUEUE_STOP_SCHEDULING, "DTLS handshake timed out too many times. Terminating dtls session timer.");
    }

    timeoutValDefaultTimeUnit =
        (UINT64) timeout.tv_sec * HUNDREDS_OF_NANOS_IN_A_SECOND + (UINT64) timeout.tv_usec * HUNDREDS_OF_NANOS_IN_A_MICROSECOND;

    if (timeoutValDefaultTimeUnit == 0) {
        DLOGD("DTLS handshake timeout event");
        /* Retransmit the packet */
        DTLSv1_handle_timeout(pDtlsSession->pSsl);
        CHK_STATUS(dtlsCheckOutgoingDataBuffer(pDtlsSession));
    }

CleanUp:

    if (locked) {
        MUTEX_UNLOCK(pDtlsSession->sslLock);
    }

    return retStatus;
}

STATUS createCertificateAndKey(INT32 certificateBits, BOOL generateRSACertificate, X509** ppCert, EVP_PKEY** ppPkey)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    BIGNUM* pBne = NULL;
    RSA* pRsa = NULL;
    X509_NAME* pX509Name = NULL;
    UINT32 eccGroup = 0;
    EC_KEY* eccKey = NULL;

    CHK(ppCert != NULL && ppPkey != NULL, STATUS_NULL_ARG);
    CHK((*ppPkey = EVP_PKEY_new()) != NULL, STATUS_CERTIFICATE_GENERATION_FAILED);

    if (generateRSACertificate) {
        CHK((pBne = BN_new()) != NULL, STATUS_CERTIFICATE_GENERATION_FAILED);
        CHK(BN_set_word(pBne, KVS_RSA_F4) != 0, STATUS_CERTIFICATE_GENERATION_FAILED);

        CHK((pRsa = RSA_new()) != NULL, STATUS_CERTIFICATE_GENERATION_FAILED);
        CHK(RSA_generate_key_ex(pRsa, certificateBits, pBne, NULL) != 0, STATUS_CERTIFICATE_GENERATION_FAILED);
        CHK((EVP_PKEY_assign_RSA(*ppPkey, pRsa)) != 0, STATUS_CERTIFICATE_GENERATION_FAILED);
        pRsa = NULL;
    } else {
        CHK((eccGroup = OBJ_txt2nid("prime256v1")) != NID_undef, STATUS_CERTIFICATE_GENERATION_FAILED);
        CHK((eccKey = EC_KEY_new_by_curve_name(eccGroup)) != NULL, STATUS_CERTIFICATE_GENERATION_FAILED);

        // void, never fails
        EC_KEY_set_asn1_flag(eccKey, OPENSSL_EC_NAMED_CURVE);

        CHK(EC_KEY_generate_key(eccKey) != 0, STATUS_CERTIFICATE_GENERATION_FAILED);
        CHK(EVP_PKEY_assign_EC_KEY(*ppPkey, eccKey) != 0, STATUS_CERTIFICATE_GENERATION_FAILED);
    }

    CHK((*ppCert = X509_new()) != NULL, STATUS_CERTIFICATE_GENERATION_FAILED);
    X509_set_version(*ppCert, 2);
    ASN1_INTEGER_set(X509_get_serialNumber(*ppCert), GENERATED_CERTIFICATE_SERIAL);
    X509_gmtime_adj(X509_get_notBefore(*ppCert), -1 * GENERATED_CERTIFICATE_DAYS * SECONDS_IN_A_DAY);
    X509_gmtime_adj(X509_get_notAfter(*ppCert), GENERATED_CERTIFICATE_DAYS * SECONDS_IN_A_DAY);
    CHK(X509_set_pubkey(*ppCert, *ppPkey) != 0, STATUS_CERTIFICATE_GENERATION_FAILED);

    CHK((pX509Name = X509_get_subject_name(*ppCert)) != NULL, STATUS_CERTIFICATE_GENERATION_FAILED);
    X509_NAME_add_entry_by_txt(pX509Name, "O", MBSTRING_ASC, (PUINT8) GENERATED_CERTIFICATE_NAME, -1, -1, 0);
    X509_NAME_add_entry_by_txt(pX509Name, "CN", MBSTRING_ASC, (PUINT8) GENERATED_CERTIFICATE_NAME, -1, -1, 0);

    CHK(X509_set_issuer_name(*ppCert, pX509Name) != 0, STATUS_CERTIFICATE_GENERATION_FAILED);
    CHK(X509_sign(*ppCert, *ppPkey, EVP_sha1()) != 0, STATUS_CERTIFICATE_GENERATION_FAILED);

CleanUp:
    if (pBne != NULL) {
        BN_free(pBne);
    }

    if (STATUS_FAILED(retStatus)) {
        if (pRsa != NULL) {
            RSA_free(pRsa);
        }
        freeCertificateAndKey(ppCert, ppPkey);
    }

    LEAVES();
    return retStatus;
}

STATUS createSslCtx(PDtlsSessionCertificateInfo pCertificates, UINT32 certCount, SSL_CTX** ppSslCtx)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    SSL_CTX* pSslCtx = NULL;
    EC_KEY* pEcKey = NULL;
    UINT32 i;

    CHK(pCertificates != NULL && ppSslCtx != NULL, STATUS_NULL_ARG);
    CHK(certCount > 0, STATUS_INTERNAL_ERROR);

#if (OPENSSL_VERSION_NUMBER < 0x10002000L)
    EC_KEY* ecdh = NULL;
#endif

#if (OPENSSL_VERSION_NUMBER >= 0x10100000L)
    pSslCtx = SSL_CTX_new(DTLS_method());
#elif (OPENSSL_VERSION_NUMBER >= 0x10001000L)
    pSslCtx = SSL_CTX_new(DTLSv1_method());
#else
#error "Unsupported OpenSSL Version"
#endif

    CHK(pSslCtx != NULL, STATUS_SSL_CTX_CREATION_FAILED);

#if (OPENSSL_VERSION_NUMBER >= 0x10002000L)
    SSL_CTX_set_ecdh_auto(pSslCtx, TRUE);
#else
    CHK((ecdh = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1)) != NULL, STATUS_SSL_CTX_CREATION_FAILED);
    CHK(SSL_CTX_set_tmp_ecdh(pSslCtx, ecdh) == 1, STATUS_SSL_CTX_CREATION_FAILED);
#endif

    SSL_CTX_set_verify(pSslCtx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, dtlsCertificateVerifyCallback);
    CHK(SSL_CTX_set_tlsext_use_srtp(pSslCtx, "SRTP_AES128_CM_SHA1_32:SRTP_AES128_CM_SHA1_80") == 0, STATUS_SSL_CTX_CREATION_FAILED);

    for (i = 0; i < certCount; i++) {
        CHK(SSL_CTX_use_certificate(pSslCtx, pCertificates[i].pCert) == 1, STATUS_SSL_CTX_CREATION_FAILED);
        CHK(SSL_CTX_use_PrivateKey(pSslCtx, pCertificates[i].pKey) == 1 || SSL_CTX_check_private_key(pSslCtx) == 1, STATUS_SSL_CTX_CREATION_FAILED);
    }

    CHK(SSL_CTX_set_cipher_list(pSslCtx, "HIGH:!aNULL:!MD5:!RC4") == 1, STATUS_SSL_CTX_CREATION_FAILED);

    *ppSslCtx = pSslCtx;

CleanUp:
    if (STATUS_FAILED(retStatus) && pSslCtx != NULL) {
        SSL_CTX_free(pSslCtx);
    }

    if (pEcKey != NULL) {
        EC_KEY_free(pEcKey);
    }

    LEAVES();
    return retStatus;
}

STATUS createSsl(SSL_CTX* pSslCtx, SSL** ppSsl)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    BIO *pReadBIO = NULL, *pWriteBIO = NULL;
    SSL* pSsl = NULL;
    BOOL freeBios = TRUE;

    CHK(pSslCtx != NULL && ppSsl != NULL, STATUS_NULL_ARG);

    CHK((pSsl = SSL_new(pSslCtx)) != NULL, STATUS_SSL_CTX_CREATION_FAILED);
    CHK((pReadBIO = BIO_new(BIO_s_mem())) != NULL, STATUS_SSL_CTX_CREATION_FAILED);
    CHK((pWriteBIO = BIO_new(BIO_s_mem())) != NULL, STATUS_SSL_CTX_CREATION_FAILED);

    BIO_set_mem_eof_return(pReadBIO, -1);
    BIO_set_mem_eof_return(pWriteBIO, -1);
    SSL_set_bio(pSsl, pReadBIO, pWriteBIO);
    freeBios = FALSE;

    *ppSsl = pSsl;

CleanUp:
    if (STATUS_FAILED(retStatus)) {
        if (pSsl != NULL) {
            SSL_free(pSsl);
        }
        if (freeBios && pReadBIO != NULL) {
            BIO_free(pReadBIO);
        }
        if (freeBios && pWriteBIO != NULL) {
            BIO_free(pWriteBIO);
        }
    }

    return STATUS_SUCCESS;
}

STATUS freeCertificateAndKey(X509** ppCert, EVP_PKEY** ppPkey)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    CHK(ppCert != NULL && ppPkey != NULL, STATUS_NULL_ARG);

    if (*ppCert != NULL) {
        X509_free(*ppCert);
    }
    if (*ppPkey != NULL) {
        EVP_PKEY_free(*ppPkey);
    }

    *ppCert = NULL;
    *ppPkey = NULL;

CleanUp:
    LEAVES();
    return retStatus;
}

STATUS createDtlsSession(PDtlsSessionCallbacks pDtlsSessionCallbacks, TIMER_QUEUE_HANDLE timerQueueHandle, INT32 certificateBits,
                         BOOL generateRSACertificate, PRtcCertificate pRtcCertificates, PDtlsSession* ppDtlsSession)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PDtlsSession pDtlsSession = NULL;
    UINT32 i, certCount;
    DtlsSessionCertificateInfo certInfos[MAX_RTCCONFIGURATION_CERTIFICATES];
    MEMSET(certInfos, 0x00, SIZEOF(certInfos));

    CHK(ppDtlsSession != NULL, STATUS_NULL_ARG);
    CHK_STATUS(dtlsValidateRtcCertificates(pRtcCertificates, &certCount));

    pDtlsSession = MEMCALLOC(SIZEOF(DtlsSession), 1);
    CHK(pDtlsSession != NULL, STATUS_NOT_ENOUGH_MEMORY);

    pDtlsSession->timerQueueHandle = timerQueueHandle;
    pDtlsSession->timerId = MAX_UINT32;
    pDtlsSession->sslLock = MUTEX_CREATE(TRUE);
    pDtlsSession->state = RTC_DTLS_TRANSPORT_STATE_NEW;
    ATOMIC_STORE_BOOL(&pDtlsSession->isStarted, FALSE);
    ATOMIC_STORE_BOOL(&pDtlsSession->sslInitFinished, FALSE);

    pDtlsSession->dtlsSessionCallbacks = *pDtlsSessionCallbacks;

    if (certificateBits == 0) {
        certificateBits = GENERATED_CERTIFICATE_BITS;
    }

    if (certCount == 0) {
        CHK_STATUS(createCertificateAndKey(certificateBits, generateRSACertificate, &certInfos[0].pCert, &certInfos[0].pKey));
        certInfos[0].created = TRUE;
        pDtlsSession->certificateCount = 1;
    } else {
        pDtlsSession->certificateCount = certCount;
        for (i = 0; i < certCount; i++) {
            certInfos[i].pCert = (X509*) pRtcCertificates[i].pCertificate;
            certInfos[i].pKey = (EVP_PKEY*) pRtcCertificates[i].pPrivateKey;
            certInfos[i].created = FALSE;
        }
    }

    CHK_STATUS(createSslCtx(certInfos, pDtlsSession->certificateCount, &pDtlsSession->pSslCtx));
    CHK_STATUS(createSsl(pDtlsSession->pSslCtx, &pDtlsSession->pSsl));

    // Generate and store the certificate fingerprints
    CHK_STATUS(dtlsGenerateCertificateFingerprints(pDtlsSession, certInfos));

    *ppDtlsSession = pDtlsSession;

CleanUp:

    CHK_LOG_ERR(retStatus);

    // Free the created cert and private key
    for (i = 0; i < MAX_RTCCONFIGURATION_CERTIFICATES; i++) {
        if (certInfos[i].created) {
            freeCertificateAndKey(&certInfos[i].pCert, &certInfos[i].pKey);
        }
    }

    if (STATUS_FAILED(retStatus)) {
        freeDtlsSession(&pDtlsSession);
    }

    LEAVES();
    return retStatus;
}

STATUS dtlsGenerateCertificateFingerprints(PDtlsSession pDtlsSession, PDtlsSessionCertificateInfo pDtlsSessionCertificateInfo)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 i;

    CHK(pDtlsSession != NULL && pDtlsSessionCertificateInfo != NULL, STATUS_NULL_ARG);

    for (i = 0; i < pDtlsSession->certificateCount; i++) {
        CHK_STATUS(dtlsCertificateFingerprint(pDtlsSessionCertificateInfo[i].pCert, pDtlsSession->certFingerprints[i]));
    }

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS dtlsSessionStart(PDtlsSession pDtlsSession, BOOL isServer)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    BOOL locked = FALSE;
    INT32 sslRet, sslErr;

    CHK(pDtlsSession != NULL && pDtlsSession != NULL, STATUS_NULL_ARG);
    CHK(!ATOMIC_LOAD_BOOL(&pDtlsSession->isStarted), retStatus);

    MUTEX_LOCK(pDtlsSession->sslLock);
    locked = TRUE;

    CHK_STATUS(dtlsSessionChangeState(pDtlsSession, RTC_DTLS_TRANSPORT_STATE_CONNECTING));

    /* Need to set isStarted to TRUE after acquiring the lock to make sure dtlsSessionProcessPacket
     * dont proceed before dtlsSessionStart finish */
    ATOMIC_STORE_BOOL(&pDtlsSession->isStarted, TRUE);

    if (isServer) {
        SSL_set_accept_state(pDtlsSession->pSsl);
    } else {
        SSL_set_connect_state(pDtlsSession->pSsl);
    }
    sslRet = SSL_do_handshake(pDtlsSession->pSsl);
    if (sslRet <= 0) {
        LOG_OPENSSL_ERROR("SSL_do_handshake");
    }

    pDtlsSession->dtlsSessionStartTime = GETTIME();
    CHK_STATUS(timerQueueAddTimer(pDtlsSession->timerQueueHandle, DTLS_SESSION_TIMER_START_DELAY, DTLS_TRANSMISSION_INTERVAL,
                                  dtlsTransmissionTimerCallback, (UINT64) pDtlsSession, &pDtlsSession->timerId));

CleanUp:

    CHK_LOG_ERR(retStatus);

    if (locked) {
        MUTEX_UNLOCK(pDtlsSession->sslLock);
    }

    LEAVES();
    return retStatus;
}

STATUS freeDtlsSession(PDtlsSession* ppDtlsSession)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PDtlsSession pDtlsSession;

    CHK(ppDtlsSession != NULL, STATUS_NULL_ARG);

    pDtlsSession = *ppDtlsSession;

    CHK(pDtlsSession != NULL, retStatus);

    if (pDtlsSession->timerId != MAX_UINT32) {
        timerQueueCancelTimer(pDtlsSession->timerQueueHandle, pDtlsSession->timerId, (UINT64) pDtlsSession);
    }

    if (pDtlsSession->pSsl != NULL) {
        SSL_CTX_free(pDtlsSession->pSslCtx);
    }
    if (pDtlsSession->pSslCtx != NULL) {
        SSL_free(pDtlsSession->pSsl);
    }
    if (IS_VALID_MUTEX_VALUE(pDtlsSession->sslLock)) {
        MUTEX_FREE(pDtlsSession->sslLock);
    }

    SAFE_MEMFREE(pDtlsSession);
    *ppDtlsSession = NULL;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS dtlsSessionProcessPacket(PDtlsSession pDtlsSession, PBYTE pData, PINT32 pDataLen)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    BOOL locked = FALSE, isClosed = FALSE;
    INT32 sslRet = 0, sslErr;
    INT32 dataLen = 0;

    CHK(pDtlsSession != NULL && pDtlsSession != NULL && pDataLen != NULL, STATUS_NULL_ARG);
    CHK(ATOMIC_LOAD_BOOL(&pDtlsSession->isStarted), STATUS_SSL_PACKET_BEFORE_DTLS_READY);

    MUTEX_LOCK(pDtlsSession->sslLock);
    locked = TRUE;

    sslRet = BIO_write(SSL_get_rbio(pDtlsSession->pSsl), pData, *pDataLen);
    if (sslRet <= 0) {
        LOG_OPENSSL_ERROR("BIO_write");
    }

    // should clear error before SSL_read: https://stackoverflow.com/a/47218133
    ERR_clear_error();
    sslRet = SSL_read(pDtlsSession->pSsl, pData, *pDataLen);

    if (sslRet == 0 && SSL_get_error(pDtlsSession->pSsl, sslRet) == SSL_ERROR_ZERO_RETURN) {
        DLOGD("Detected DTLS close_notify alert");
        isClosed = TRUE;
    } else if (sslRet <= 0) {
        LOG_OPENSSL_ERROR("SSL_read");
    }

    if (!ATOMIC_LOAD_BOOL(&pDtlsSession->sslInitFinished)) {
        CHK_STATUS(dtlsCheckOutgoingDataBuffer(pDtlsSession));
    }

    /* if SSL_read failed then set to 0 */
    dataLen = sslRet < 0 ? 0 : sslRet;

    if (isClosed) {
        ATOMIC_STORE_BOOL(&pDtlsSession->shutdown, TRUE);
        CHK_STATUS(dtlsSessionChangeState(pDtlsSession, RTC_DTLS_TRANSPORT_STATE_CLOSED));
    }

CleanUp:

    CHK_LOG_ERR(retStatus);

    if (pDataLen != NULL) {
        *pDataLen = dataLen;
    }

    if (locked) {
        MUTEX_UNLOCK(pDtlsSession->sslLock);
    }

    LEAVES();
    return retStatus;
}

STATUS dtlsSessionPutApplicationData(PDtlsSession pDtlsSession, PBYTE pData, INT32 dataLen)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    INT32 amountWritten;
    BYTE buf[MAX_UDP_PACKET_SIZE];
    BIO* wbio;
    SIZE_T pending;

    MUTEX_LOCK(pDtlsSession->sslLock);
    CHK(!ATOMIC_LOAD_BOOL(&pDtlsSession->shutdown), retStatus);

    if ((amountWritten = SSL_write(pDtlsSession->pSsl, pData, dataLen)) != dataLen &&
        SSL_get_error(pDtlsSession->pSsl, amountWritten) == SSL_ERROR_SSL) {
        DLOGW("SSL_write failed with %s", ERR_error_string(SSL_get_error(pDtlsSession->pSsl, dataLen), NULL));
        CHK(FALSE, STATUS_INTERNAL_ERROR);
    }

    wbio = SSL_get_wbio(pDtlsSession->pSsl);
    if ((pending = BIO_ctrl_pending(wbio)) > 0) {
        pending = BIO_read(wbio, buf, pending);
        pDtlsSession->dtlsSessionCallbacks.outboundPacketFn(pDtlsSession->dtlsSessionCallbacks.outBoundPacketFnCustomData, buf, (UINT32) pending);
    }

CleanUp:
    MUTEX_UNLOCK(pDtlsSession->sslLock);

    LEAVES();
    return retStatus;
}

STATUS dtlsSessionShutdown(PDtlsSession pDtlsSession)
{
    STATUS retStatus = STATUS_SUCCESS;
    BOOL locked = FALSE;

    CHK(pDtlsSession != NULL, STATUS_NULL_ARG);

    MUTEX_LOCK(pDtlsSession->sslLock);
    locked = TRUE;

    CHK(!ATOMIC_LOAD_BOOL(&pDtlsSession->shutdown), retStatus);
    CHK(ATOMIC_LOAD_BOOL(&pDtlsSession->sslInitFinished), retStatus);

    SSL_shutdown(pDtlsSession->pSsl);
    ATOMIC_STORE_BOOL(&pDtlsSession->shutdown, TRUE);
    CHK_STATUS(dtlsCheckOutgoingDataBuffer(pDtlsSession));
    CHK_STATUS(dtlsSessionChangeState(pDtlsSession, RTC_DTLS_TRANSPORT_STATE_CLOSED));

CleanUp:

    if (locked) {
        MUTEX_UNLOCK(pDtlsSession->sslLock);
    }

    return retStatus;
}

STATUS dtlsCheckOutgoingDataBuffer(PDtlsSession pDtlsSession)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    BIO* pWriteBIO = NULL;
    INT32 dataLenWritten = 0, sslErr = 0;

    pWriteBIO = SSL_get_wbio(pDtlsSession->pSsl);
    // proceed if write BIO is not empty
    CHK(BIO_ctrl_pending(pWriteBIO) > 0, retStatus);

    // BIO_read removes read data
    dataLenWritten = BIO_read(pWriteBIO, pDtlsSession->outgoingDataBuffer, ARRAY_SIZE(pDtlsSession->outgoingDataBuffer));
    if (dataLenWritten > 0) {
        pDtlsSession->outgoingDataLen = (UINT32) dataLenWritten;
        pDtlsSession->dtlsSessionCallbacks.outboundPacketFn(pDtlsSession->dtlsSessionCallbacks.outBoundPacketFnCustomData,
                                                            pDtlsSession->outgoingDataBuffer, pDtlsSession->outgoingDataLen);
    } else {
        LOG_OPENSSL_ERROR("BIO_read");
    }

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS dtlsSessionIsInitFinished(PDtlsSession pDtlsSession, PBOOL pIsConnected)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    BOOL locked = FALSE;

    CHK(pDtlsSession != NULL && pIsConnected != NULL, STATUS_NULL_ARG);

    MUTEX_LOCK(pDtlsSession->sslLock);
    locked = TRUE;

    *pIsConnected = SSL_is_init_finished(pDtlsSession->pSsl);

CleanUp:
    if (locked) {
        MUTEX_UNLOCK(pDtlsSession->sslLock);
    }

    LEAVES();
    return retStatus;
}

STATUS dtlsSessionPopulateKeyingMaterial(PDtlsSession pDtlsSession, PDtlsKeyingMaterial pDtlsKeyingMaterial)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 offset = 0;
    BYTE keyingMaterialBuffer[MAX_SRTP_MASTER_KEY_LEN * 2 + MAX_SRTP_SALT_KEY_LEN * 2];
    BOOL locked = FALSE;

    CHK(pDtlsSession != NULL && pDtlsKeyingMaterial != NULL, STATUS_NULL_ARG);

    MUTEX_LOCK(pDtlsSession->sslLock);
    locked = TRUE;

    CHK(SSL_export_keying_material(pDtlsSession->pSsl, keyingMaterialBuffer, SIZEOF(keyingMaterialBuffer), KEYING_EXTRACTOR_LABEL,
                                   ARRAY_SIZE(KEYING_EXTRACTOR_LABEL) - 1, NULL, 0, 0),
        STATUS_INTERNAL_ERROR);

    pDtlsKeyingMaterial->key_length = MAX_SRTP_MASTER_KEY_LEN + MAX_SRTP_SALT_KEY_LEN;

    MEMCPY(pDtlsKeyingMaterial->clientWriteKey, &keyingMaterialBuffer[offset], MAX_SRTP_MASTER_KEY_LEN);
    offset += MAX_SRTP_MASTER_KEY_LEN;

    MEMCPY(pDtlsKeyingMaterial->serverWriteKey, &keyingMaterialBuffer[offset], MAX_SRTP_MASTER_KEY_LEN);
    offset += MAX_SRTP_MASTER_KEY_LEN;

    MEMCPY(pDtlsKeyingMaterial->clientWriteKey + MAX_SRTP_MASTER_KEY_LEN, &keyingMaterialBuffer[offset], MAX_SRTP_SALT_KEY_LEN);
    offset += MAX_SRTP_SALT_KEY_LEN;

    MEMCPY(pDtlsKeyingMaterial->serverWriteKey + MAX_SRTP_MASTER_KEY_LEN, &keyingMaterialBuffer[offset], MAX_SRTP_SALT_KEY_LEN);

    switch (SSL_get_selected_srtp_profile(pDtlsSession->pSsl)->id) {
        case KVS_SRTP_PROFILE_AES128_CM_HMAC_SHA1_32:
        case KVS_SRTP_PROFILE_AES128_CM_HMAC_SHA1_80:
            pDtlsKeyingMaterial->srtpProfile = SSL_get_selected_srtp_profile(pDtlsSession->pSsl)->id;
            break;
        default:
            CHK(FALSE, STATUS_SSL_UNKNOWN_SRTP_PROFILE);
    }

CleanUp:
    if (locked) {
        MUTEX_UNLOCK(pDtlsSession->sslLock);
    }

    LEAVES();
    return retStatus;
}

STATUS dtlsSessionGetLocalCertificateFingerprint(PDtlsSession pDtlsSession, PCHAR pBuff, UINT32 buffLen)
{
    UNUSED_PARAM(buffLen);
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    BOOL locked = FALSE;

    CHK(pDtlsSession != NULL && pBuff != NULL, STATUS_NULL_ARG);
    CHK(buffLen >= CERTIFICATE_FINGERPRINT_LENGTH, STATUS_INVALID_ARG_LEN);

    MUTEX_LOCK(pDtlsSession->sslLock);
    locked = TRUE;

    // Use the 0th certificate for now
    MEMCPY(pBuff, pDtlsSession->certFingerprints[0], CERTIFICATE_FINGERPRINT_LENGTH * SIZEOF(CHAR));

CleanUp:
    if (locked) {
        MUTEX_UNLOCK(pDtlsSession->sslLock);
    }

    LEAVES();
    return retStatus;
}

STATUS dtlsSessionVerifyRemoteCertificateFingerprint(PDtlsSession pDtlsSession, PCHAR pExpectedFingerprint)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    CHAR actualFingerprint[CERTIFICATE_FINGERPRINT_LENGTH];
    X509* pRemoteCertificate = NULL;
    BOOL locked = FALSE;

    CHK(pDtlsSession != NULL && pExpectedFingerprint != NULL, STATUS_NULL_ARG);

    MUTEX_LOCK(pDtlsSession->sslLock);
    locked = TRUE;

    CHK((pRemoteCertificate = SSL_get_peer_certificate(pDtlsSession->pSsl)) != NULL, STATUS_INTERNAL_ERROR);
    CHK_STATUS(dtlsCertificateFingerprint(pRemoteCertificate, actualFingerprint));

    CHK(STRCMP(pExpectedFingerprint, actualFingerprint) == 0, STATUS_SSL_REMOTE_CERTIFICATE_VERIFICATION_FAILED);

CleanUp:
    if (pRemoteCertificate != NULL) {
        X509_free(pRemoteCertificate);
    }

    if (retStatus == STATUS_SSL_REMOTE_CERTIFICATE_VERIFICATION_FAILED) {
        dtlsSessionChangeState(pDtlsSession, RTC_DTLS_TRANSPORT_STATE_FAILED);
    }

    if (locked) {
        MUTEX_UNLOCK(pDtlsSession->sslLock);
    }

    LEAVES();
    return retStatus;
}
