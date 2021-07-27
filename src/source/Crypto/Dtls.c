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

STATUS dtlsSessionChangeState(PDtlsSession pDtlsSession, RTC_DTLS_TRANSPORT_STATE newState)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pDtlsSession != NULL, STATUS_NULL_ARG);
    CHK(pDtlsSession->state != newState, retStatus);

    if (pDtlsSession->state == RTC_DTLS_TRANSPORT_STATE_CONNECTING && newState == RTC_DTLS_TRANSPORT_STATE_CONNECTED) {
        DLOGD("DTLS init completed. Time taken %" PRIu64 " ms",
              (GETTIME() - pDtlsSession->dtlsSessionStartTime) / HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
    }
    pDtlsSession->state = newState;
    if (pDtlsSession->dtlsSessionCallbacks.stateChangeFn != NULL) {
        pDtlsSession->dtlsSessionCallbacks.stateChangeFn(pDtlsSession->dtlsSessionCallbacks.stateChangeFnCustomData, newState);
    }

CleanUp:

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
