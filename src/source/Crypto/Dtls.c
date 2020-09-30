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
    return STATUS_SUCCESS;
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
    UINT32 i;

    CHK(pRtcCertificates != NULL && pCount != NULL, retStatus);

    for (i = 0, *pCount = 0; pRtcCertificates[i].pCertificate != NULL && i < MAX_RTCCONFIGURATION_CERTIFICATES; i++) {
        CHK(pRtcCertificates[i].privateKeySize == 0 || pRtcCertificates[i].pPrivateKey != NULL, STATUS_SSL_INVALID_CERTIFICATE_BITS);
    }

    *pCount = i;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS dtlsSessionChangeState(PDtlsSession pDtlsSession, RTC_DTLS_TRANSPORT_STATE newState)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pDtlsSession != NULL, STATUS_NULL_ARG);
    CHK(pDtlsSession->state != newState, retStatus);

    if (pDtlsSession->state == CONNECTING && newState == CONNECTED) {
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
