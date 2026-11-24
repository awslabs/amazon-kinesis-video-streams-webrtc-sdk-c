#define LOG_CLASS "DTLS"
#include "../Include_i.h"

STATUS dtlsSessionOnOutBoundData(PDtlsSession pDtlsSession, UINT64 customData, DtlsSessionOutboundPacketFunc callbackFn)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pDtlsSession != NULL && callbackFn != NULL, STATUS_NULL_ARG);

    MUTEX_LOCK(pDtlsSession->sslLock);
    pDtlsSession->dtlsSessionCallbacks.outboundPacketFn = callbackFn;
    pDtlsSession->dtlsSessionCallbacks.outBoundPacketFnCustomData = customData;
    MUTEX_UNLOCK(pDtlsSession->sslLock);

CleanUp:
    return retStatus;
}

STATUS dtlsSessionOnStateChange(PDtlsSession pDtlsSession, UINT64 customData, DtlsSessionOnStateChange callbackFn)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pDtlsSession != NULL && callbackFn != NULL, STATUS_NULL_ARG);

    MUTEX_LOCK(pDtlsSession->sslLock);
    pDtlsSession->dtlsSessionCallbacks.stateChangeFn = callbackFn;
    pDtlsSession->dtlsSessionCallbacks.stateChangeFnCustomData = customData;
    MUTEX_UNLOCK(pDtlsSession->sslLock);

CleanUp:
    LEAVES();
    return retStatus;
}

STATUS dtlsValidateRtcCertificates(PRtcCertificate pRtcCertificates, PUINT32 pCount)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 i = 0;

    CHK(pCount != NULL, STATUS_NULL_ARG);

    // No certs have been specified
    CHK(pRtcCertificates != NULL, retStatus);

    for (i = 0, *pCount = 0; pRtcCertificates[i].pCertificate != NULL && i < MAX_RTCCONFIGURATION_CERTIFICATES; i++) {
        CHK(pRtcCertificates[i].privateKeySize == 0 || pRtcCertificates[i].pPrivateKey != NULL, STATUS_SSL_INVALID_CERTIFICATE_BITS);
    }

CleanUp:

    // If pRtcCertificates is NULL, default pCount to 0
    if (pCount != NULL) {
        *pCount = i;
    }

    LEAVES();
    return retStatus;
}

BOOL dtlsPacketIsClientHello(PBYTE pData, UINT32 dataLen)
{
    // Handshake record (22) of a DTLS version (0xFExx) in epoch 0, holding a ClientHello (1) from its first fragment (offset
    // 0). The OpenSSL backend, for one, sends its ClientHello as two records in one datagram.
    return pData != NULL && dataLen >= DTLS_CLIENT_HELLO_MIN_LEN && pData[0] == 22 && pData[1] == 0xFE && pData[3] == 0 && pData[4] == 0 &&
        pData[13] == 1 && pData[19] == 0 && pData[20] == 0 && pData[21] == 0;
}

STATUS dtlsSessionKeepEarlyClientHello(PDtlsSession pDtlsSession, PBYTE pData, UINT32 dataLen)
{
    STATUS retStatus = STATUS_SUCCESS;
    PBYTE pCopy = NULL;

    CHK(pDtlsSession != NULL, STATUS_NULL_ARG);
    CHK(dtlsPacketIsClientHello(pData, dataLen), STATUS_SSL_PACKET_BEFORE_DTLS_READY);
    CHK_ERR(NULL != (pCopy = (PBYTE) MEMALLOC(dataLen)), STATUS_NOT_ENOUGH_MEMORY,
            "Not enough memory to keep an early ClientHello; the client's retransmission will be answered");
    MEMCPY(pCopy, pData, dataLen);
    // The latest one only: the client retransmits the same ClientHello
    SAFE_MEMFREE(pDtlsSession->pEarlyClientHello);
    pDtlsSession->pEarlyClientHello = pCopy;
    pDtlsSession->earlyClientHelloLen = dataLen;
    DLOGD("Keeping a ClientHello that arrived before the DTLS session started");

CleanUp:
    return retStatus;
}

STATUS dtlsSessionChangeState(PDtlsSession pDtlsSession, RTC_DTLS_TRANSPORT_STATE newState)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pDtlsSession != NULL, STATUS_NULL_ARG);
    CHK(pDtlsSession->state != newState, retStatus);

    if (pDtlsSession->state == RTC_DTLS_TRANSPORT_STATE_CONNECTING && newState == RTC_DTLS_TRANSPORT_STATE_CONNECTED) {
        // Need to set this so that we do not calculate the time taken again. We set the new state in 2 different places
        if (pDtlsSession->dtlsSessionStartTime != 0) {
            PROFILE_WITH_START_TIME_OBJ(pDtlsSession->dtlsSessionStartTime, pDtlsSession->dtlsSessionSetupTime, "DTLS initialization completion");
            pDtlsSession->dtlsSessionStartTime = 0;
        }
    }
    pDtlsSession->state = newState;
    if (pDtlsSession->dtlsSessionCallbacks.stateChangeFn != NULL) {
        pDtlsSession->dtlsSessionCallbacks.stateChangeFn(pDtlsSession->dtlsSessionCallbacks.stateChangeFnCustomData, newState);
    }

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS dtlsSessionCopyOptions(PDtlsSession pDtlsSession, PDtlsSessionOptions pDtlsSessionOptions)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 hostnameLen = 0;

    CHK(pDtlsSession != NULL, STATUS_NULL_ARG);

    pDtlsSession->validationMode = DTLS_SESSION_VALIDATION_MODE_RELAXED;
    pDtlsSession->pExpectedServerHostname = NULL;

    CHK(pDtlsSessionOptions != NULL, retStatus);

    pDtlsSession->validationMode = pDtlsSessionOptions->validationMode;
    if (pDtlsSession->validationMode == DTLS_SESSION_VALIDATION_MODE_STRICT_SERVER) {
        CHK(pDtlsSessionOptions->pExpectedServerHostname != NULL && pDtlsSessionOptions->pExpectedServerHostname[0] != '\0', STATUS_INVALID_ARG);
        hostnameLen = (UINT32) STRLEN(pDtlsSessionOptions->pExpectedServerHostname);
#ifdef KVS_USE_MBEDTLS
        CHK(hostnameLen <= MBEDTLS_SSL_MAX_HOST_NAME_LEN, STATUS_INVALID_ARG_LEN);
#endif
        pDtlsSession->pExpectedServerHostname = MEMCALLOC(hostnameLen + 1, SIZEOF(CHAR));
        CHK(pDtlsSession->pExpectedServerHostname != NULL, STATUS_NOT_ENOUGH_MEMORY);
        STRNCPY(pDtlsSession->pExpectedServerHostname, pDtlsSessionOptions->pExpectedServerHostname, hostnameLen);
    }

CleanUp:
    CHK_LOG_ERR(retStatus);
    LEAVES();
    return retStatus;
}

STATUS dtlsFillPseudoRandomBits(PBYTE pBuf, UINT32 bufSize)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 i;

    CHK(pBuf != NULL, STATUS_NULL_ARG);
    CHK(bufSize >= DTLS_CERT_MIN_SERIAL_NUM_SIZE && bufSize <= DTLS_CERT_MAX_SERIAL_NUM_SIZE, retStatus);

    for (i = 0; i < bufSize; i++) {
        *pBuf++ = (BYTE) (RAND() & 0xFF);
    }

CleanUp:

    LEAVES();
    return retStatus;
}
