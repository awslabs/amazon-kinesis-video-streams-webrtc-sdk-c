/**
 * Kinesis Video Producer Ice Utils
 */
#define LOG_CLASS "IceUtils"
#include "../Include_i.h"

static PCHAR iceServerSchemeToString(ICE_SERVER_SCHEME scheme)
{
    switch (scheme) {
        case ICE_SERVER_SCHEME_STUN:
            return (PCHAR) "stun";
        case ICE_SERVER_SCHEME_STUNS:
            return (PCHAR) "stuns";
        case ICE_SERVER_SCHEME_TURN:
            return (PCHAR) "turn";
        case ICE_SERVER_SCHEME_TURNS:
            return (PCHAR) "turns";
        default:
            return (PCHAR) "unknown";
    }
}

static PCHAR iceServerTransportToString(KVS_SOCKET_PROTOCOL transport)
{
    switch (transport) {
        case KVS_SOCKET_PROTOCOL_UDP:
            return (PCHAR) ICE_TRANSPORT_TYPE_UDP;
        case KVS_SOCKET_PROTOCOL_TCP:
            return (PCHAR) ICE_TRANSPORT_TYPE_TCP;
        default:
            return (PCHAR) "none";
    }
}

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

    CHAR ipAddrStr[KVS_IP_ADDRESS_PORT_STRING_BUFFER_LEN];

    CHK_STATUS(getIpAddrPortStr(pDest, ipAddrStr, ARRAY_SIZE(ipAddrStr)));

    CHK_STATUS(iceUtilsPackageStunPacket(pStunPacket, password, passwordLen, stunPacketBuffer, &stunPacketSize));
    CHK(pDest != NULL, STATUS_NULL_ARG);
    switch (pStunPacket->header.stunMessageType) {
        case STUN_PACKET_TYPE_BINDING_REQUEST:
            DLOGD("Sending BINDING_REQUEST on socket id: %d, to %s", pSocketConnection->localSocket, ipAddrStr);
            break;
        case STUN_PACKET_TYPE_BINDING_RESPONSE_SUCCESS:
            DLOGD("Sending BINDING_RESPONSE_SUCCESS on socket id: %d to %s", pSocketConnection->localSocket, ipAddrStr);
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
    PCHAR urlNoPrefix = NULL, paramStart = NULL, portSeparator = NULL;
    UINT32 port = 0, hostLen = 0;
    CHAR addressResolvedIPv4[KVS_IP_ADDRESS_STRING_BUFFER_LEN + 1] = {'\0'};
    CHAR addressResolvedIPv6[KVS_IP_ADDRESS_STRING_BUFFER_LEN + 1] = {'\0'};

    CHK(url != NULL && pIceServer != NULL, STATUS_NULL_ARG);

    MEMSET(pIceServer->url, 0x00, SIZEOF(pIceServer->url));
    MEMSET(&pIceServer->ipAddresses, 0x00, SIZEOF(pIceServer->ipAddresses));
    MEMSET(pIceServer->username, 0x00, SIZEOF(pIceServer->username));
    MEMSET(pIceServer->credential, 0x00, SIZEOF(pIceServer->credential));
    pIceServer->isTurn = FALSE;
    pIceServer->isSecure = FALSE;
    pIceServer->scheme = ICE_SERVER_SCHEME_STUN;
    pIceServer->transport = KVS_SOCKET_PROTOCOL_UDP;

    if (STRNCMPI(ICE_URL_PREFIX_STUN_SECURE, url, STRLEN(ICE_URL_PREFIX_STUN_SECURE)) == 0) {
        urlNoPrefix = url + STRLEN(ICE_URL_PREFIX_STUN_SECURE);
        pIceServer->isSecure = TRUE;
        pIceServer->scheme = ICE_SERVER_SCHEME_STUNS;
        pIceServer->transport = KVS_SOCKET_PROTOCOL_UDP;
        port = ICE_STUNS_DEFAULT_PORT;
    } else if (STRNCMPI(ICE_URL_PREFIX_STUN, url, STRLEN(ICE_URL_PREFIX_STUN)) == 0) {
        urlNoPrefix = url + STRLEN(ICE_URL_PREFIX_STUN);
        pIceServer->scheme = ICE_SERVER_SCHEME_STUN;
        pIceServer->transport = KVS_SOCKET_PROTOCOL_UDP;
        port = ICE_STUN_DEFAULT_PORT;
    } else if (STRNCMPI(ICE_URL_PREFIX_TURN_SECURE, url, STRLEN(ICE_URL_PREFIX_TURN_SECURE)) == 0) {
        CHK(username != NULL && username[0] != '\0', STATUS_ICE_URL_TURN_MISSING_USERNAME);
        CHK(credential != NULL && credential[0] != '\0', STATUS_ICE_URL_TURN_MISSING_CREDENTIAL);

        STRNCPY(pIceServer->username, username, MAX_ICE_CONFIG_USER_NAME_LEN);
        STRNCPY(pIceServer->credential, credential, MAX_ICE_CONFIG_CREDENTIAL_LEN);
        urlNoPrefix = url + STRLEN(ICE_URL_PREFIX_TURN_SECURE);
        pIceServer->isTurn = TRUE;
        pIceServer->isSecure = TRUE;
        pIceServer->scheme = ICE_SERVER_SCHEME_TURNS;
        pIceServer->transport = KVS_SOCKET_PROTOCOL_NONE;
        port = ICE_STUN_DEFAULT_PORT;
    } else if (STRNCMPI(ICE_URL_PREFIX_TURN, url, STRLEN(ICE_URL_PREFIX_TURN)) == 0) {
        CHK(username != NULL && username[0] != '\0', STATUS_ICE_URL_TURN_MISSING_USERNAME);
        CHK(credential != NULL && credential[0] != '\0', STATUS_ICE_URL_TURN_MISSING_CREDENTIAL);

        STRNCPY(pIceServer->username, username, MAX_ICE_CONFIG_USER_NAME_LEN);
        STRNCPY(pIceServer->credential, credential, MAX_ICE_CONFIG_CREDENTIAL_LEN);
        urlNoPrefix = url + STRLEN(ICE_URL_PREFIX_TURN);
        pIceServer->isTurn = TRUE;
        pIceServer->scheme = ICE_SERVER_SCHEME_TURN;
        pIceServer->transport = KVS_SOCKET_PROTOCOL_NONE;
        port = ICE_STUN_DEFAULT_PORT;
    } else {
        CHK(FALSE, STATUS_ICE_URL_INVALID_PREFIX);
    }

    CHK(urlNoPrefix != NULL && urlNoPrefix[0] != '\0', STATUS_ICE_URL_MALFORMED);

    paramStart = STRCHR(urlNoPrefix, '?');
    portSeparator = STRCHR(urlNoPrefix, ':');
    if (portSeparator != NULL && paramStart != NULL && portSeparator > paramStart) {
        portSeparator = NULL;
    }

    if (portSeparator != NULL) {
        hostLen = (UINT32) (portSeparator - urlNoPrefix);
        CHK(hostLen != 0 && hostLen <= MAX_ICE_CONFIG_URI_LEN, STATUS_ICE_URL_MALFORMED);
        STRNCPY(pIceServer->url, urlNoPrefix, hostLen);
        pIceServer->url[hostLen] = '\0';
        CHK(portSeparator[1] != '\0' && (paramStart == NULL || portSeparator + 1 < paramStart), STATUS_ICE_URL_MALFORMED);
        CHK_STATUS(STRTOUI32(portSeparator + 1, paramStart, 10, &port));
    } else {
        hostLen = (UINT32) ((paramStart != NULL) ? (paramStart - urlNoPrefix) : STRLEN(urlNoPrefix));
        CHK(hostLen != 0 && hostLen <= MAX_ICE_CONFIG_URI_LEN, STATUS_ICE_URL_MALFORMED);
        STRNCPY(pIceServer->url, urlNoPrefix, hostLen);
        pIceServer->url[hostLen] = '\0';
    }

    CHK(port <= MAX_UINT16, STATUS_ICE_URL_MALFORMED);

    if (pIceServer->scheme == ICE_SERVER_SCHEME_STUNS) {
        CHK(!isIpAddr(pIceServer->url, hostLen), STATUS_ICE_URL_STUNS_IP_LITERAL_NOT_ALLOWED);
    }

    if (pIceServer->isTurn) {
        if (STRSTR(url, ICE_URL_TRANSPORT_UDP) != NULL) {
            pIceServer->transport = KVS_SOCKET_PROTOCOL_UDP;
        } else if (STRSTR(url, ICE_URL_TRANSPORT_TCP) != NULL) {
            pIceServer->transport = KVS_SOCKET_PROTOCOL_TCP;
        }
    } else if (paramStart != NULL && STRSTR(paramStart, ICE_URL_TRANSPORT_TCP) != NULL) {
        DLOGW("Ignoring unsupported transport=tcp parameter for %s URL %s. Using %s for srflx gathering.",
              iceServerSchemeToString(pIceServer->scheme), url, pIceServer->scheme == ICE_SERVER_SCHEME_STUNS ? "udp/dtls" : ICE_TRANSPORT_TYPE_UDP);
    }

    DLOGD("Parsed ICE server config. Input URL: %s. Host: %s. Scheme: %s. Secure: %s. Turn: %s. Transport: %s. Port: %u", url, pIceServer->url,
          iceServerSchemeToString(pIceServer->scheme), pIceServer->isSecure ? "true" : "false", pIceServer->isTurn ? "true" : "false",
          iceServerTransportToString(pIceServer->transport), port);

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
