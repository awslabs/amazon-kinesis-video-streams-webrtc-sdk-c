#define LOG_CLASS "TLS"
#include "../Include_i.h"

STATUS tlsSessionChangeState(PTlsSession pTlsSession, TLS_SESSION_STATE newState)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pTlsSession != NULL, STATUS_NULL_ARG);
    CHK(pTlsSession->state != newState, retStatus);

    pTlsSession->state = newState;

#ifdef KVS_USE_AWS_LC
    /* Report the negotiated key exchange group once, on the transition into
     * CONNECTED. This is the TURN-over-TLS connection, so it is the SDK-owned
     * half of the post-quantum posture (the signaling half is negotiated inside
     * libwebsockets). KVS_USE_AWS_LC implies KVS_USE_OPENSSL, so pSsl exists. */
    if (newState == TLS_SESSION_STATE_CONNECTED) {
        awsLcRecordNegotiatedGroup(pTlsSession->pSsl, (PCHAR) "TURNS");
    }
#endif

    if (pTlsSession->callbacks.stateChangeFn != NULL) {
        pTlsSession->callbacks.stateChangeFn(pTlsSession->callbacks.stateChangeFnCustomData, newState);
    }

CleanUp:

    LEAVES();
    return retStatus;
}
