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
        pTransactionIdStore->earliestTransactionIdIndex =
            (pTransactionIdStore->earliestTransactionIdIndex + 1) % pTransactionIdStore->maxTransactionIdsCount;
        return;
    }

    pTransactionIdStore->transactionIdCount = MIN(pTransactionIdStore->transactionIdCount + 1, pTransactionIdStore->maxTransactionIdsCount);
}

BOOL transactionIdStoreHasId(PTransactionIdStore pTransactionIdStore, PBYTE transactionId)
{
    BOOL idFound = FALSE;
    UINT32 i, j;

    CHECK(pTransactionIdStore != NULL);

    for (i = pTransactionIdStore->earliestTransactionIdIndex, j = 0; j < pTransactionIdStore->maxTransactionIdsCount && !idFound; ++j) {
        if (MEMCMP(transactionId, pTransactionIdStore->transactionIds + i * STUN_TRANSACTION_ID_LEN, STUN_TRANSACTION_ID_LEN) == 0) {
            idFound = TRUE;
        }

        i = (i + 1) % pTransactionIdStore->maxTransactionIdsCount;
    }

    return idFound;
}

VOID transactionIdStoreRemove(PTransactionIdStore pTransactionIdStore, PBYTE transactionId)
{
    UINT32 i, j;

    CHECK(pTransactionIdStore != NULL);

    for (i = pTransactionIdStore->earliestTransactionIdIndex, j = 0; j < pTransactionIdStore->maxTransactionIdsCount; ++j) {
        if (MEMCMP(transactionId, pTransactionIdStore->transactionIds + i * STUN_TRANSACTION_ID_LEN, STUN_TRANSACTION_ID_LEN) == 0) {
            MEMSET(pTransactionIdStore->transactionIds + i * STUN_TRANSACTION_ID_LEN, 0x00, STUN_TRANSACTION_ID_LEN);
            return;
        }

        i = (i + 1) % pTransactionIdStore->maxTransactionIdsCount;
    }
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

    for (i = 0; i < STUN_TRANSACTION_ID_LEN; ++i) {
        pBuffer[i] = ((BYTE) (RAND() % 0x100));
    }

CleanUp:

    CHK_LOG_ERR(retStatus);

    return retStatus;
}

STATUS iceUtilsPackageStunPacket(PStunPacket pStunPacket, PBYTE password, UINT32 passwordLen, PBYTE pBuffer, PUINT32 pBufferLen)
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

STATUS iceUtilsSendStunPacket(PStunPacket pStunPacket, PBYTE password, UINT32 passwordLen, PKvsIpAddress pDest, PSocketConnection pSocketConnection,
                              PTurnConnection pTurnConnection, BOOL useTurn)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 stunPacketSize = STUN_PACKET_ALLOCATION_SIZE;
    BYTE stunPacketBuffer[STUN_PACKET_ALLOCATION_SIZE];

    CHAR ipAddrStr[KVS_IP_ADDRESS_STRING_BUFFER_LEN];

    CHK_STATUS(getIpAddrStr(pDest, ipAddrStr, ARRAY_SIZE(ipAddrStr)));

    CHK_STATUS(iceUtilsPackageStunPacket(pStunPacket, password, passwordLen, stunPacketBuffer, &stunPacketSize));
    CHK(pDest != NULL, STATUS_NULL_ARG);
    switch (pStunPacket->header.stunMessageType) {
        case STUN_PACKET_TYPE_BINDING_REQUEST:
            if (pDest->family == KVS_IP_FAMILY_TYPE_IPV4) {
                DLOGD("Sending BINDING_REQUEST on socket id: %d, to ip:%u.%u.%u.%u, port:%u", pSocketConnection->localSocket, pDest->address[0],
                      pDest->address[1], pDest->address[2], pDest->address[3], (UINT16) getInt16(pDest->port));
            } else {
                DLOGD("Sending BINDING_REQUEST on socket id: %d, to ip:%x:%x:%x:%x:%x:%x:%x:%x, port:%u", pSocketConnection->localSocket,
                      (UINT16) ((pDest->address[0] << 8) | pDest->address[1]), (UINT16) ((pDest->address[2] << 8) | pDest->address[3]),
                      (UINT16) ((pDest->address[4] << 8) | pDest->address[5]), (UINT16) ((pDest->address[6] << 8) | pDest->address[7]),
                      (UINT16) ((pDest->address[8] << 8) | pDest->address[9]), (UINT16) ((pDest->address[10] << 8) | pDest->address[11]),
                      (UINT16) ((pDest->address[12] << 8) | pDest->address[13]), (UINT16) ((pDest->address[14] << 8) | pDest->address[15]),
                      (UINT16) getInt16(pDest->port));
            }
            break;
        case STUN_PACKET_TYPE_BINDING_RESPONSE_SUCCESS:
            if (pDest->family == KVS_IP_FAMILY_TYPE_IPV4) {
                DLOGD("Sending BINDING_RESPONSE_SUCCESS on socket id: %d to ip:%u.%u.%u.%u, port:%u", pSocketConnection->localSocket, pDest->address[0],
                      pDest->address[1], pDest->address[2], pDest->address[3], (UINT16) getInt16(pDest->port));
            } else {
                DLOGD("Sending BINDING_RESPONSE_SUCCESS on socket id: %d to ip:%x:%x:%x:%x:%x:%x:%x:%x, port:%u", pSocketConnection->localSocket,
                      (UINT16) ((pDest->address[0] << 8) | pDest->address[1]), (UINT16) ((pDest->address[2] << 8) | pDest->address[3]),
                      (UINT16) ((pDest->address[4] << 8) | pDest->address[5]), (UINT16) ((pDest->address[6] << 8) | pDest->address[7]),
                      (UINT16) ((pDest->address[8] << 8) | pDest->address[9]), (UINT16) ((pDest->address[10] << 8) | pDest->address[11]),
                      (UINT16) ((pDest->address[12] << 8) | pDest->address[13]), (UINT16) ((pDest->address[14] << 8) | pDest->address[15]),
                      (UINT16) getInt16(pDest->port));
            }
            break;
        default:
            break;
    }
    CHK_STATUS(iceUtilsSendData(stunPacketBuffer, stunPacketSize, pDest, pSocketConnection, pTurnConnection, useTurn));

CleanUp:

    CHK_LOG_ERR(retStatus);

    return retStatus;
}

STATUS iceUtilsSendData(PBYTE buffer, UINT32 size, PKvsIpAddress pDest, PSocketConnection pSocketConnection, PTurnConnection pTurnConnection,
                        BOOL useTurn)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK((pSocketConnection != NULL && !useTurn) || (pTurnConnection != NULL && useTurn), STATUS_INVALID_ARG);

    if (useTurn) {
        retStatus = turnConnectionSendData(pTurnConnection, buffer, size, pDest);
    } else {
        retStatus = socketConnectionSendData(pSocketConnection, buffer, size, pDest);
    }

    // Fix-up the not-yet-ready socket
    CHK(STATUS_SUCCEEDED(retStatus) || retStatus == STATUS_SOCKET_CONNECTION_NOT_READY_TO_SEND, retStatus);
    retStatus = STATUS_SUCCESS;

CleanUp:

    CHK_LOG_ERR(retStatus);

    return retStatus;
}

STATUS parseIceServer(PIceServer pIceServer, PCHAR url, PCHAR username, PCHAR credential)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PCHAR separator = NULL, urlNoPrefix = NULL, paramStart = NULL;
    UINT32 port = ICE_STUN_DEFAULT_PORT;
    CHAR addressResolvedIPv4[KVS_IP_ADDRESS_STRING_BUFFER_LEN + 1] = {'\0'};
    CHAR addressResolvedIPv6[KVS_IP_ADDRESS_STRING_BUFFER_LEN + 1] = {'\0'};

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
        pIceServer->isSecure = STRNCMP(ICE_URL_PREFIX_TURN_SECURE, url, STRLEN(ICE_URL_PREFIX_TURN_SECURE)) == 0;

        pIceServer->transport = KVS_SOCKET_PROTOCOL_NONE;
        if (STRSTR(url, ICE_URL_TRANSPORT_UDP) != NULL) {
            pIceServer->transport = KVS_SOCKET_PROTOCOL_UDP;
        } else if (STRSTR(url, ICE_URL_TRANSPORT_TCP) != NULL) {
            pIceServer->transport = KVS_SOCKET_PROTOCOL_TCP;
        }

    } else {
        CHK(FALSE, STATUS_ICE_URL_INVALID_PREFIX);
    }

    if ((separator = STRCHR(urlNoPrefix, ':')) != NULL) {
        separator++;
        paramStart = STRCHR(urlNoPrefix, '?');
        CHK_STATUS(STRTOUI32(separator, paramStart, 10, &port));
        STRNCPY(pIceServer->url, urlNoPrefix, separator - urlNoPrefix - 1);
        // need to null terminate since we are not copying the entire urlNoPrefix
        pIceServer->url[separator - urlNoPrefix - 1] = '\0';
    } else {
        STRNCPY(pIceServer->url, urlNoPrefix, MAX_ICE_CONFIG_URI_LEN);
    }

    if (pIceServer->setIpFn != NULL) {
        retStatus = pIceServer->setIpFn(0, pIceServer->url, &pIceServer->ipAddresses);
    }

    // Adding a NULL_ARG check specifically to cover for the case where early STUN
    // resolution might not be enabled
    // Also cover the case where hostname is not resolved because the request was made too soon
    if (retStatus == STATUS_NULL_ARG || retStatus == STATUS_PEERCONNECTION_EARLY_DNS_RESOLUTION_FAILED || pIceServer->setIpFn == NULL) {
        // Reset the retStatus to ensure the appropriate status code is returned from
        // getIpWithHostName
        retStatus = STATUS_SUCCESS;
        CHK_STATUS(getIpWithHostName(pIceServer->url, &pIceServer->ipAddresses));
    }

    if (pIceServer->ipAddresses.ipv4Address.family != KVS_IP_FAMILY_TYPE_NOT_SET) {
        pIceServer->ipAddresses.ipv4Address.port = (UINT16) getInt16((INT16) port);
        CHK_STATUS(getIpAddrStr(&pIceServer->ipAddresses.ipv4Address, addressResolvedIPv4, ARRAY_SIZE(addressResolvedIPv4)));
        DLOGD("Resolved ICE Server IPv4 address for %s: %s with port: %u", pIceServer->url, addressResolvedIPv4,
              pIceServer->ipAddresses.ipv4Address.port);
    }

    if (pIceServer->ipAddresses.ipv6Address.family != KVS_IP_FAMILY_TYPE_NOT_SET) {
        pIceServer->ipAddresses.ipv6Address.port = (UINT16) getInt16((INT16) port);
        CHK_STATUS(getIpAddrStr(&pIceServer->ipAddresses.ipv6Address, addressResolvedIPv6, ARRAY_SIZE(addressResolvedIPv6)));
        DLOGD("Resolved ICE Server IPv6 address for %s: %s with port: %u", pIceServer->url, addressResolvedIPv6,
              pIceServer->ipAddresses.ipv6Address.port);
    }

CleanUp:

    LEAVES();

    return retStatus;
}
