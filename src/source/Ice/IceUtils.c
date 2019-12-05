/**
 * Kinesis Video Producer Ice Utils
 */
#define LOG_CLASS "IceUtils"
#include "../Include_i.h"

STATUS createTransactionIdStore(UINT32 maxIdCount, PTransactionIdStore* ppTransactionIdStore)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PTransactionIdStore pTransactionIdStore = NULL;

    CHK(ppTransactionIdStore != NULL, STATUS_NULL_ARG);
    CHK(maxIdCount < MAX_STORED_TRANSACTION_ID_COUNT && maxIdCount > 0, STATUS_INVALID_ARG);

    pTransactionIdStore = (PTransactionIdStore) MEMCALLOC(1, SIZEOF(TransactionIdStore) + STUN_TRANSACTION_ID_LEN * maxIdCount);
    CHK(pTransactionIdStore != NULL, STATUS_NOT_ENOUGH_MEMORY);

    pTransactionIdStore->transactionIds = (PBYTE) (pTransactionIdStore + 1);
    pTransactionIdStore->maxTransactionIdsCount = maxIdCount;

CleanUp:

    if (STATUS_FAILED(retStatus) && pTransactionIdStore != NULL) {
        MEMFREE(pTransactionIdStore);
        pTransactionIdStore = NULL;
    }

    if (ppTransactionIdStore != NULL) {
        *ppTransactionIdStore = pTransactionIdStore;
    }

    LEAVES();
    return retStatus;
}
STATUS freeTransactionIdStore(PTransactionIdStore* ppTransactionIdStore)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PTransactionIdStore pTransactionIdStore = NULL;

    CHK(ppTransactionIdStore != NULL, STATUS_NULL_ARG);
    pTransactionIdStore = *ppTransactionIdStore;
    CHK(pTransactionIdStore != NULL, retStatus);

    MEMFREE(pTransactionIdStore);

    *ppTransactionIdStore = NULL;

CleanUp:

    LEAVES();
    return retStatus;
}

VOID transactionIdStoreInsert(PTransactionIdStore pTransactionIdStore, PBYTE transactionId)
{
    PBYTE storeLocation = NULL;

    CHECK(pTransactionIdStore != NULL);

    storeLocation = pTransactionIdStore->transactionIds +
            ((pTransactionIdStore->nextTransactionIdIndex % pTransactionIdStore->maxTransactionIdsCount) * STUN_TRANSACTION_ID_LEN);
    MEMCPY(storeLocation, transactionId, STUN_TRANSACTION_ID_LEN);

    pTransactionIdStore->nextTransactionIdIndex = (pTransactionIdStore->nextTransactionIdIndex + 1) % pTransactionIdStore->maxTransactionIdsCount;

    if (pTransactionIdStore->nextTransactionIdIndex == pTransactionIdStore->earliestTransactionIdIndex) {
        pTransactionIdStore->earliestTransactionIdIndex = (pTransactionIdStore->earliestTransactionIdIndex + 1) % pTransactionIdStore->maxTransactionIdsCount;
    }

    pTransactionIdStore->transactionIdCount = MIN(pTransactionIdStore->transactionIdCount + 1, pTransactionIdStore->maxTransactionIdsCount);
}

BOOL transactionIdStoreHasId(PTransactionIdStore pTransactionIdStore, PBYTE transactionId)
{
    BOOL idFound = FALSE;
    UINT32 i, j;

    CHECK(pTransactionIdStore != NULL);

    for(i = pTransactionIdStore->earliestTransactionIdIndex, j = 0; j < pTransactionIdStore->maxTransactionIdsCount && !idFound; ++j) {
        if (MEMCMP(transactionId, pTransactionIdStore->transactionIds + i * STUN_TRANSACTION_ID_LEN, STUN_TRANSACTION_ID_LEN) == 0) {
            idFound = TRUE;
        }

        i = (i + 1) % pTransactionIdStore->maxTransactionIdsCount;
    }

    return idFound;
}

VOID transactionIdStoreClear(PTransactionIdStore pTransactionIdStore)
{
    CHECK(pTransactionIdStore != NULL);

    pTransactionIdStore->nextTransactionIdIndex = 0;
    pTransactionIdStore->earliestTransactionIdIndex = 0;
    pTransactionIdStore->transactionIdCount = 0;
}

STATUS iceUtilsGenerateTransactionId(PBYTE pBuffer, UINT32 bufferLen)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 i;

    CHK(pBuffer != NULL, STATUS_NULL_ARG);
    CHK(bufferLen == STUN_TRANSACTION_ID_LEN, STATUS_INVALID_ARG);

    for(i = 0; i < STUN_TRANSACTION_ID_LEN; ++i) {
        pBuffer[i] = ((BYTE) (RAND() % 0x100));
    }

CleanUp:

    return retStatus;
}

STATUS iceUtilsPackageStunPacket(PStunPacket pStunPacket, PBYTE password, UINT32 passwordLen,
                         PBYTE pBuffer, PUINT32 pBufferLen)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 stunPacketSize = 0;
    BOOL addMessageIntegrity = FALSE;

    CHK(pStunPacket != NULL && pBuffer != NULL && pBufferLen != NULL, STATUS_NULL_ARG);
    CHK((password == NULL && passwordLen == 0) || (password != NULL && passwordLen > 0), STATUS_INVALID_ARG);

    if (password != NULL) {
        addMessageIntegrity = TRUE;
    }

    CHK_STATUS(serializeStunPacket(pStunPacket, password, passwordLen, addMessageIntegrity, TRUE, NULL, &stunPacketSize));
    CHK(stunPacketSize <= *pBufferLen, STATUS_BUFFER_TOO_SMALL);
    CHK_STATUS(serializeStunPacket(pStunPacket, password, passwordLen, addMessageIntegrity, TRUE, pBuffer, &stunPacketSize));
    *pBufferLen = stunPacketSize;

CleanUp:

    CHK_LOG_ERR(retStatus);

    return retStatus;
}

STATUS iceUtilsSendStunPacket(PStunPacket pStunPacket, PBYTE password, UINT32 passwordLen,
                              PKvsIpAddress pDest, PSocketConnection pSocketConnection, PTurnConnection pTurnConnection,
                              BOOL useTurn)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 stunPacketSize = STUN_PACKET_ALLOCATION_SIZE;
    BYTE stunPacketBuffer[STUN_PACKET_ALLOCATION_SIZE];

    CHK((pSocketConnection != NULL && !useTurn) || (pTurnConnection != NULL && useTurn), STATUS_INVALID_ARG);

    CHK_STATUS(iceUtilsPackageStunPacket(pStunPacket, password, passwordLen, stunPacketBuffer, &stunPacketSize));
    if (useTurn) {
        retStatus = turnConnectionSendData(pTurnConnection, stunPacketBuffer, stunPacketSize, pDest);
    } else {
        retStatus = socketConnectionSendData(pSocketConnection, stunPacketBuffer, stunPacketSize, pDest);
    }

    if (STATUS_FAILED(retStatus)) {
        DLOGW("iceUtilsSendStunPacket failed with 0x%08x", retStatus);
        retStatus = STATUS_SUCCESS;
    }

CleanUp:

    return retStatus;
}

// only work with ipv4 right now
STATUS populateIpFromString(PKvsIpAddress pKvsIpAddress, PCHAR pBuff)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PCHAR curr, tail, next;
    UINT8 octet = 0;
    UINT32 ipValue;

    CHK(pKvsIpAddress != NULL && pBuff != NULL, STATUS_NULL_ARG);
    CHK(STRNLEN(pBuff, KVS_MAX_IPV4_ADDRESS_STRING_LEN) > 0 &&
        STRNLEN(pBuff, KVS_MAX_IPV4_ADDRESS_STRING_LEN) < KVS_MAX_IPV4_ADDRESS_STRING_LEN, STATUS_INVALID_ARG);

    curr = pBuff;
    tail = pBuff + STRLEN(pBuff);
    while ((next = STRNCHR(curr, tail - curr, '.')) != NULL) {
        CHK_STATUS(STRTOUI32(curr, curr + (next - curr), 10, &ipValue));
        pKvsIpAddress->address[octet] = (UINT8) ipValue;
        octet++;

        curr = next + 1;
    }

    CHK_STATUS(STRTOUI32(curr, NULL, 10, &ipValue));
    pKvsIpAddress->address[octet] = (UINT8) ipValue;
    octet++;


    CHK(octet == 4, STATUS_ICE_CANDIDATE_STRING_INVALID_IP); // IPv4 MUST have 4 octets
CleanUp:

    LEAVES();
    return retStatus;
}