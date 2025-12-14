#include "../Samples.h"

PVOID mediaSenderRoutine(PVOID customData)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleConfiguration pSampleConfiguration = (PSampleConfiguration) customData;
    CHK(pSampleConfiguration != NULL, STATUS_NULL_ARG);
    pSampleConfiguration->videoSenderTid = INVALID_TID_VALUE;
    pSampleConfiguration->audioSenderTid = INVALID_TID_VALUE;

    MUTEX_LOCK(pSampleConfiguration->sampleConfigurationObjLock);
    while (!ATOMIC_LOAD_BOOL(&pSampleConfiguration->connected) && !ATOMIC_LOAD_BOOL(&pSampleConfiguration->appTerminateFlag)) {
        CVAR_WAIT(pSampleConfiguration->cvar, pSampleConfiguration->sampleConfigurationObjLock, 5 * HUNDREDS_OF_NANOS_IN_A_SECOND);
    }
    MUTEX_UNLOCK(pSampleConfiguration->sampleConfigurationObjLock);

    CHK(!ATOMIC_LOAD_BOOL(&pSampleConfiguration->appTerminateFlag), retStatus);

    if (pSampleConfiguration->videoSource != NULL) {
        THREAD_CREATE_WITH_PARAMS(&pSampleConfiguration->videoSenderTid, pSampleConfiguration->videoSource,
                                  KVS_DEFAULT_MEDIA_SENDER_THREAD_STACK_SIZE, (PVOID) pSampleConfiguration);
    }

    if (pSampleConfiguration->audioSource != NULL) {
        THREAD_CREATE_WITH_PARAMS(&pSampleConfiguration->audioSenderTid, pSampleConfiguration->audioSource,
                                  KVS_DEFAULT_MEDIA_SENDER_THREAD_STACK_SIZE, (PVOID) pSampleConfiguration);
    }

    if (pSampleConfiguration->videoSenderTid != INVALID_TID_VALUE) {
        THREAD_JOIN(pSampleConfiguration->videoSenderTid, NULL);
    }

    if (pSampleConfiguration->audioSenderTid != INVALID_TID_VALUE) {
        THREAD_JOIN(pSampleConfiguration->audioSenderTid, NULL);
    }

CleanUp:
    // clean the flag of the media thread.
    ATOMIC_STORE_BOOL(&pSampleConfiguration->mediaThreadStarted, FALSE);
    CHK_LOG_ERR(retStatus);
    return NULL;
}
