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

    SAFE_MEMFREE(pTransactionIdStore);

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

    CHK_LOG_ERR_NV(retStatus);

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
        CHK_STATUS(turnConnectionSendData(pTurnConnection, stunPacketBuffer, stunPacketSize, pDest));
    } else {
        CHK_STATUS(socketConnectionSendData(pSocketConnection, stunPacketBuffer, stunPacketSize, pDest));
    }

CleanUp:

    CHK_LOG_ERR_NV(retStatus);

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
    CHK(STRNLEN(pBuff, KVS_MAX_IPV4_ADDRESS_STRING_LEN) > 0, STATUS_INVALID_ARG);

    curr = pBuff;
    tail = pBuff + STRLEN(pBuff);
    // first 3 octet should always end with a '.', the last octet may end with ' ' or '\0'
    while ((next = STRNCHR(curr, tail - curr, '.')) != NULL && octet < 3) {
        CHK_STATUS(STRTOUI32(curr, curr + (next - curr), 10, &ipValue));
        pKvsIpAddress->address[octet] = (UINT8) ipValue;
        octet++;

        curr = next + 1;
    }

    // work with string containing just ip address and string that has ip address as substring
    if ((next = STRNCHR(curr, tail - curr, ' ')) != NULL || (next = STRNCHR(curr, tail - curr, '\0')) != NULL) {
        CHK_STATUS(STRTOUI32(curr, curr + (next - curr), 10, &ipValue));
        pKvsIpAddress->address[octet] = (UINT8) ipValue;
        octet++;
    }

    CHK(octet == 4, STATUS_ICE_CANDIDATE_STRING_INVALID_IP); // IPv4 MUST have 4 octets
CleanUp:

    CHK_LOG_ERR_NV(retStatus);

    LEAVES();
    return retStatus;
}

STATUS parseIceServer(PIceServer pIceServer, PCHAR url, PCHAR username, PCHAR credential)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PCHAR separator = NULL, urlNoPrefix = NULL;
    UINT32 port = ICE_STUN_DEFAULT_PORT;

    // username and credential is only mandatory for turn server
    CHK(url != NULL && pIceServer != NULL, STATUS_NULL_ARG);

    if (STRNCMP(ICE_URL_PREFIX_STUN, url, STRLEN(ICE_URL_PREFIX_STUN)) == 0) {
        urlNoPrefix = STRCHR(url, ':') + 1;
        pIceServer->isTurn = FALSE;
    } else if (STRNCMP(ICE_URL_PREFIX_TURN, url, STRLEN(ICE_URL_PREFIX_TURN)) == 0 ||
               STRNCMP(ICE_URL_PREFIX_TURN_SECURE, url, STRLEN(ICE_URL_PREFIX_TURN_SECURE)) == 0) {
        CHK(username != NULL && username[0] != '\0', STATUS_ICE_URL_TURN_MISSING_USERNAME);
        CHK(credential != NULL && credential[0] != '\0', STATUS_ICE_URL_TURN_MISSING_CREDENTIAL);

        // TODO after getIceServerConfig no longer give turn: ips, do TLS only for turns:
        STRNCPY(pIceServer->username, username, MAX_ICE_CONFIG_USER_NAME_LEN);
        STRNCPY(pIceServer->credential, credential, MAX_ICE_CONFIG_CREDENTIAL_LEN);
        urlNoPrefix = STRCHR(url, ':') + 1;
        pIceServer->isTurn = TRUE;
    } else {
        CHK(FALSE, STATUS_ICE_URL_INVALID_PREFIX);
    }

    if ((separator = STRCHR(urlNoPrefix, ':')) != NULL) {
        separator++;
        CHK_STATUS(STRTOUI32(separator, separator + STRLEN(separator), 10, &port));
        STRNCPY(pIceServer->url, urlNoPrefix, separator - urlNoPrefix - 1);
        // need to null terminate since we are not copying the entire urlNoPrefix
        pIceServer->url[separator - urlNoPrefix - 1] = '\0';
    } else {
        STRNCPY(pIceServer->url, urlNoPrefix, MAX_ICE_CONFIG_URI_LEN);
    }

    CHK_STATUS(getIpWithHostName(pIceServer->url, &pIceServer->ipAddress));
    pIceServer->ipAddress.port = (UINT16) getInt16((INT16) port);

CleanUp:

    LEAVES();

    return retStatus;
}
