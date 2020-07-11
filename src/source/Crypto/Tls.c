#define LOG_CLASS "TLS"
#include "../Include_i.h"

STATUS tlsSessionChangeState(PTlsSession pTlsSession, TLS_SESSION_STATE newState)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pTlsSession != NULL, STATUS_NULL_ARG);
    CHK(pTlsSession->state != newState, retStatus);

    pTlsSession->state = newState;
    if (pTlsSession->callbacks.stateChangeFn != NULL) {
        pTlsSession->callbacks.stateChangeFn(pTlsSession->callbacks.stateChangeFnCustomData, newState);
    }

CleanUp:

    LEAVES();
    return retStatus;
}
