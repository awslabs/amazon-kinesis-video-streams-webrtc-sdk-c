/**
 * CA Certificate Cache
 */
#define LOG_CLASS "CaCertCache"
#include "../Include_i.h"

static PBYTE gCachedCaCertBuf = NULL;
static UINT32 gCachedCaCertBufLen = 0;

STATUS initCaCertCache(PCHAR pCaCertPath)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT64 certLen = 0;

    CHK(pCaCertPath != NULL, STATUS_NULL_ARG);
    CHK(gCachedCaCertBuf == NULL, retStatus); // Already initialized

    DLOGD("Init CA cert cache at %s", pCaCertPath);
    CHK_STATUS(readFile(pCaCertPath, FALSE, NULL, &certLen));
    CHK(certLen > 0, STATUS_INVALID_CERT_PATH_LENGTH);
    gCachedCaCertBuf = (PBYTE) MEMCALLOC(1, certLen + 1);
    CHK(gCachedCaCertBuf != NULL, STATUS_NOT_ENOUGH_MEMORY);
    CHK_STATUS(readFile(pCaCertPath, FALSE, gCachedCaCertBuf, &certLen));
    gCachedCaCertBufLen = (UINT32) certLen;
    DLOGD("CA certificate cache initialized from %s, size %u bytes", pCaCertPath, gCachedCaCertBufLen);

CleanUp:
    CHK_LOG_ERR(retStatus);

    if (STATUS_FAILED(retStatus)) {
        SAFE_MEMFREE(gCachedCaCertBuf);
        gCachedCaCertBufLen = 0;
    }

    LEAVES();
    return retStatus;
}

STATUS deinitCaCertCache(VOID)
{
    ENTERS();
    SAFE_MEMFREE(gCachedCaCertBuf);
    gCachedCaCertBufLen = 0;
    LEAVES();
    return STATUS_SUCCESS;
}

STATUS getCachedCaCert(PBYTE* ppCaCertBuf, PUINT32 pCaCertBufLen)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    CHK(ppCaCertBuf != NULL && pCaCertBufLen != NULL, STATUS_NULL_ARG);

    *ppCaCertBuf = gCachedCaCertBuf;
    *pCaCertBufLen = gCachedCaCertBufLen;

CleanUp:
    CHK_LOG_ERR(retStatus);

    LEAVES();
    return retStatus;
}
