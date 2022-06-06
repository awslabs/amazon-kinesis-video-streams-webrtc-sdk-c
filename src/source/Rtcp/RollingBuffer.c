#define LOG_CLASS "RollingBuffer"

#include "../Include_i.h"

STATUS createRollingBuffer(UINT32 capacity, FreeDataFunc freeDataFunc, PRollingBuffer* ppRollingBuffer)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PRollingBuffer pRollingBuffer = NULL;
    CHK(capacity != 0, STATUS_INVALID_ARG);

    CHK(ppRollingBuffer != NULL, STATUS_NULL_ARG);
    pRollingBuffer = (PRollingBuffer) MEMALLOC(SIZEOF(RollingBuffer) + SIZEOF(UINT64) * capacity);
    CHK(pRollingBuffer != NULL, STATUS_NOT_ENOUGH_MEMORY);
    pRollingBuffer->capacity = capacity;
    pRollingBuffer->headIndex = 0;
    pRollingBuffer->tailIndex = 0;
    pRollingBuffer->freeDataFn = freeDataFunc;
    pRollingBuffer->lock = MUTEX_CREATE(FALSE);
    pRollingBuffer->dataBuffer = (PUINT64) (pRollingBuffer + 1);
    MEMSET(pRollingBuffer->dataBuffer, 0, SIZEOF(UINT64) * pRollingBuffer->capacity);

CleanUp:
    if (STATUS_FAILED(retStatus) && pRollingBuffer != NULL) {
        freeRollingBuffer(&pRollingBuffer);
        pRollingBuffer = NULL;
    }

    if (ppRollingBuffer != NULL) {
        *ppRollingBuffer = pRollingBuffer;
    }
    LEAVES();
    return retStatus;
}

STATUS freeRollingBuffer(PRollingBuffer* ppRollingBuffer)
{
    ENTERS();
    PRollingBuffer pRollingBuffer = NULL;
    PUINT64 pCurData;

    STATUS retStatus = STATUS_SUCCESS;

    CHK(ppRollingBuffer != NULL, STATUS_NULL_ARG);

    pRollingBuffer = *ppRollingBuffer;
    // freeRollingBuffer is idempotent
    CHK(pRollingBuffer != NULL, retStatus);

    MUTEX_LOCK(pRollingBuffer->lock);
    while (pRollingBuffer->tailIndex < pRollingBuffer->headIndex) {
        pCurData = pRollingBuffer->dataBuffer + ROLLING_BUFFER_MAP_INDEX(pRollingBuffer, pRollingBuffer->tailIndex);
        if (pRollingBuffer->freeDataFn != NULL) {
            pRollingBuffer->freeDataFn(pCurData);
            *pCurData = (UINT64) NULL;
        }
        pRollingBuffer->tailIndex++;
    }
    MUTEX_UNLOCK(pRollingBuffer->lock);
    MUTEX_FREE(pRollingBuffer->lock);
    SAFE_MEMFREE(*ppRollingBuffer);
CleanUp:
    CHK_LOG_ERR(retStatus);

    LEAVES();
    return retStatus;
}

STATUS rollingBufferAppendData(PRollingBuffer pRollingBuffer, UINT64 data, PUINT64 pIndex)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    BOOL isLocked = FALSE;

    CHK(pRollingBuffer != NULL, STATUS_NULL_ARG);

    MUTEX_LOCK(pRollingBuffer->lock);
    isLocked = TRUE;

    if (pRollingBuffer->headIndex == pRollingBuffer->tailIndex) {
        // Empty buffer
        pRollingBuffer->dataBuffer[ROLLING_BUFFER_MAP_INDEX(pRollingBuffer, pRollingBuffer->tailIndex)] = data;
        pRollingBuffer->headIndex = pRollingBuffer->tailIndex + 1;
    } else {
        if (pRollingBuffer->headIndex == pRollingBuffer->tailIndex + pRollingBuffer->capacity) {
            if (pRollingBuffer->freeDataFn != NULL) {
                CHK_STATUS(
                    pRollingBuffer->freeDataFn(pRollingBuffer->dataBuffer + ROLLING_BUFFER_MAP_INDEX(pRollingBuffer, pRollingBuffer->tailIndex)));
            }
            pRollingBuffer->tailIndex++;
        }
        pRollingBuffer->dataBuffer[ROLLING_BUFFER_MAP_INDEX(pRollingBuffer, pRollingBuffer->headIndex)] = data;
        pRollingBuffer->headIndex++;
    }
    if (pIndex != NULL) {
        *pIndex = pRollingBuffer->headIndex - 1;
    }
CleanUp:
    if (isLocked) {
        MUTEX_UNLOCK(pRollingBuffer->lock);
    }

    CHK_LOG_ERR(retStatus);

    LEAVES();
    return retStatus;
}

STATUS rollingBufferInsertData(PRollingBuffer pRollingBuffer, UINT64 index, UINT64 data)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    BOOL isLocked = FALSE;
    PUINT64 pData;
    CHK(pRollingBuffer != NULL, STATUS_NULL_ARG);

    MUTEX_LOCK(pRollingBuffer->lock);
    isLocked = TRUE;

    CHK(pRollingBuffer->headIndex > index && pRollingBuffer->tailIndex <= index, STATUS_ROLLING_BUFFER_NOT_IN_RANGE);

    pData = pRollingBuffer->dataBuffer + ROLLING_BUFFER_MAP_INDEX(pRollingBuffer, index);
    if (*pData != (UINT64) NULL && pRollingBuffer->freeDataFn != NULL) {
        pRollingBuffer->freeDataFn(pData);
    }
    *pData = data;

CleanUp:
    if (isLocked) {
        MUTEX_UNLOCK(pRollingBuffer->lock);
    }

    LEAVES();
    return retStatus;
}

STATUS rollingBufferExtractData(PRollingBuffer pRollingBuffer, UINT64 index, PUINT64 pData)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    BOOL isLocked = FALSE;
    CHK(pRollingBuffer != NULL && pData != NULL, STATUS_NULL_ARG);

    MUTEX_LOCK(pRollingBuffer->lock);
    isLocked = TRUE;
    if (pRollingBuffer->headIndex > index && pRollingBuffer->tailIndex <= index) {
        *pData = pRollingBuffer->dataBuffer[ROLLING_BUFFER_MAP_INDEX(pRollingBuffer, index)];
        if (*pData != (UINT64) NULL) {
            pRollingBuffer->dataBuffer[ROLLING_BUFFER_MAP_INDEX(pRollingBuffer, index)] = (UINT64) NULL;
        }
    } else {
        *pData = (UINT64) NULL;
    }
CleanUp:
    if (isLocked) {
        MUTEX_UNLOCK(pRollingBuffer->lock);
    }
    CHK_LOG_ERR(retStatus);

    LEAVES();
    return retStatus;
}

STATUS rollingBufferGetSize(PRollingBuffer pRollingBuffer, PUINT32 pSize)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pRollingBuffer != NULL && pSize != NULL, STATUS_NULL_ARG);
    *pSize = pRollingBuffer->headIndex - pRollingBuffer->tailIndex;
CleanUp:
    CHK_LOG_ERR(retStatus);

    LEAVES();
    return retStatus;
}

STATUS rollingBufferIsEmpty(PRollingBuffer pRollingBuffer, PBOOL pIsEmpty)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    CHK(pRollingBuffer != NULL && pIsEmpty != NULL, STATUS_NULL_ARG);
    *pIsEmpty = (pRollingBuffer->headIndex == pRollingBuffer->tailIndex);

CleanUp:
    CHK_LOG_ERR(retStatus);

    LEAVES();
    return retStatus;
}
