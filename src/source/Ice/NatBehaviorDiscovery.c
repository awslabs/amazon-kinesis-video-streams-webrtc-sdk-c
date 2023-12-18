#define LOG_CLASS "NatTypeDiscovery"
#include "../Include_i.h"

/* Store STUN reponse in bindingResponseList. */
STATUS natTestIncomingDataHandler(UINT64 customData, PSocketConnection pSocketConnection, PBYTE pBuffer, UINT32 bufferLen, PKvsIpAddress pSrc,
                                  PKvsIpAddress pDest)
{
    UNUSED_PARAM(pSocketConnection);
    UNUSED_PARAM(pSrc);
    UNUSED_PARAM(pDest);

    STATUS retStatus = STATUS_SUCCESS;
    PNatTestData pNatTestData = (PNatTestData) customData;
    PStunPacket pStunPacket = NULL;

    MUTEX_LOCK(pNatTestData->lock);
    if (pNatTestData->bindingResponseCount < DEFAULT_NAT_TEST_MAX_BINDING_REQUEST_COUNT * NAT_BEHAVIOR_DISCOVER_PROCESS_TEST_COUNT) {
        CHK_STATUS(deserializeStunPacket(pBuffer, bufferLen, NULL, 0, &pStunPacket));
        pNatTestData->bindingResponseList[pNatTestData->bindingResponseCount++] = pStunPacket;
    }
    MUTEX_UNLOCK(pNatTestData->lock);
    CVAR_SIGNAL(pNatTestData->cvar);

CleanUp:

    return retStatus;
}

/*
 *  Repeatedly send bindingRequest to pDestAddr through pSocketConnection until either
 *  DEFAULT_NAT_TEST_MAX_BINDING_REQUEST_COUNT number of packets have been sent or a STUN
 *  response with testIndex as transaction id is received.
 *
 */
STATUS executeNatTest(PStunPacket bindingRequest, PKvsIpAddress pDestAddr, PSocketConnection pSocketConnection, UINT32 testIndex, PNatTestData pData,
                      PStunPacket* ppTestReponse)
{
    PStunPacket testResponse = NULL;
    UINT32 i, j;
    STATUS retStatus = STATUS_SUCCESS;

    CHK(bindingRequest != NULL && pDestAddr != NULL && pSocketConnection != NULL && pData != NULL && ppTestReponse != NULL, STATUS_NULL_ARG);

    MEMSET(bindingRequest->header.transactionId, 0x00, STUN_TRANSACTION_ID_LEN);

    /* Use testIndex as transactionId. */
    putInt32((PINT32) bindingRequest->header.transactionId, testIndex);
    for (i = 0; i < pData->bindingResponseCount; ++i) {
        freeStunPacket(&pData->bindingResponseList[i]);
    }
    pData->bindingResponseCount = 0;

    /* Send the STUN packet. Retry DEFAULT_NAT_TEST_MAX_BINDING_REQUEST_COUNT many times until a response
     * is received */
    for (i = 0; testResponse == NULL && i < DEFAULT_NAT_TEST_MAX_BINDING_REQUEST_COUNT; ++i) {
        iceUtilsSendStunPacket(bindingRequest, NULL, 0, pDestAddr, pSocketConnection, NULL, FALSE);
        CVAR_WAIT(pData->cvar, pData->lock, DEFAULT_TEST_NAT_TEST_RESPONSE_WAIT_TIME);
        if (pData->bindingResponseCount > 0) {
            for (j = 0; j < pData->bindingResponseCount; ++j) {
                if ((UINT32) getInt32(*(PINT32) pData->bindingResponseList[j]->header.transactionId) == testIndex) {
                    testResponse = pData->bindingResponseList[j];
                    break;
                }
            }
        }
    }

CleanUp:

    if (ppTestReponse != NULL) {
        *ppTestReponse = testResponse;
    }

    CHK_LOG_ERR(retStatus);
    return retStatus;
}

STATUS getMappAddressAttribute(PStunPacket pBindingResponse, PStunAttributeAddress* ppStunAttributeAddress)
{
    STATUS retStatus = STATUS_SUCCESS;
    PStunAttributeAddress pStunAttributeAddress = NULL;

    CHK(pBindingResponse != NULL && ppStunAttributeAddress != NULL, STATUS_NULL_ARG);

    CHK_STATUS(getStunAttribute(pBindingResponse, STUN_ATTRIBUTE_TYPE_XOR_MAPPED_ADDRESS, (PStunAttributeHeader*) &pStunAttributeAddress));
    if (pStunAttributeAddress == NULL) {
        CHK_STATUS(getStunAttribute(pBindingResponse, STUN_ATTRIBUTE_TYPE_MAPPED_ADDRESS, (PStunAttributeHeader*) &pStunAttributeAddress));
    }

    CHK_ERR(pStunAttributeAddress != NULL, STATUS_INVALID_OPERATION,
            "Expect binding response to have mapped address or xor mapped address attribute");

CleanUp:

    if (ppStunAttributeAddress != NULL) {
        *ppStunAttributeAddress = pStunAttributeAddress;
    }

    CHK_LOG_ERR(retStatus);
    return retStatus;
}

/*
 *  Follow the process described in https://tools.ietf.org/html/rfc5780#section-4.3 to discover the NAT mapping behavior
 */
STATUS discoverNatMappingBehavior(PIceServer pStunServer, PNatTestData data, PSocketConnection pSocketConnection, NAT_BEHAVIOR* pNatMappingBehavior)
{
    STATUS retStatus = STATUS_SUCCESS;

    KvsIpAddress mappedAddress, otherAddress, testDestAddress;
    PStunPacket bindingRequest = NULL, bindingResponse = NULL;
    NAT_BEHAVIOR natMappingBehavior = NAT_BEHAVIOR_NONE;
    PStunAttributeAddress pStunAttributeMappedAddress = NULL, pStunAttributeOtherAddress = NULL;
    UINT32 testIndex = 1, i = 0;

    CHK(pStunServer != NULL && data != NULL && pSocketConnection != NULL && pNatMappingBehavior != NULL, STATUS_NULL_ARG);

    MEMSET(&mappedAddress, 0x00, SIZEOF(KvsIpAddress));
    MEMSET(&otherAddress, 0x00, SIZEOF(KvsIpAddress));
    MEMSET(&testDestAddress, 0x00, SIZEOF(KvsIpAddress));

    CHK_STATUS(createStunPacket(STUN_PACKET_TYPE_BINDING_REQUEST, NULL, &bindingRequest));

    /* execute test I */
    DLOGD("Running mapping behavior test I. Send binding request");
    CHK_STATUS(executeNatTest(bindingRequest, &pStunServer->ipAddress, pSocketConnection, testIndex++, data, &bindingResponse));

    if (bindingResponse == NULL) {
        natMappingBehavior = NAT_BEHAVIOR_NO_UDP_CONNECTIVITY;
        CHK(FALSE, retStatus);
    }

    CHK_STATUS(getMappAddressAttribute(bindingResponse, &pStunAttributeMappedAddress));
    CHK_STATUS(getStunAttribute(bindingResponse, STUN_ATTRIBUTE_TYPE_CHANGED_ADDRESS, (PStunAttributeHeader*) &pStunAttributeOtherAddress));
    CHK_ERR(pStunAttributeOtherAddress != NULL, retStatus, "Expect binding response to have other address or changed address attribute");
    mappedAddress = pStunAttributeMappedAddress->address;
    otherAddress = pStunAttributeOtherAddress->address;

    if (isSameIpAddress(&pSocketConnection->hostIpAddr, &pStunAttributeMappedAddress->address, TRUE)) {
        natMappingBehavior = NAT_BEHAVIOR_NOT_BEHIND_ANY_NAT;
        CHK(FALSE, retStatus);
    }

    /* execute test II */
    DLOGD("Running mapping behavior test II. Send binding request to alternate address but primary port");
    testDestAddress = otherAddress;
    testDestAddress.port = pStunServer->ipAddress.port;
    CHK_STATUS(executeNatTest(bindingRequest, &testDestAddress, pSocketConnection, testIndex++, data, &bindingResponse));
    CHK_ERR(bindingResponse != NULL, retStatus, "Expect to receive binding response");

    CHK_STATUS(getMappAddressAttribute(bindingResponse, &pStunAttributeMappedAddress));

    /* compare mapped address from test I and test II */
    if (isSameIpAddress(&mappedAddress, &pStunAttributeMappedAddress->address, TRUE)) {
        natMappingBehavior = NAT_BEHAVIOR_ENDPOINT_INDEPENDENT;
        CHK(FALSE, retStatus);
    }
    mappedAddress = pStunAttributeMappedAddress->address;

    /* execute test III */
    DLOGD("Running mapping behavior test III. Send binding request to alternate address");
    CHK_STATUS(executeNatTest(bindingRequest, &otherAddress, pSocketConnection, testIndex++, data, &bindingResponse));
    CHK_ERR(bindingResponse != NULL, retStatus, "Expect to receive binding response");

    CHK_STATUS(getMappAddressAttribute(bindingResponse, &pStunAttributeMappedAddress));
    /* compare mapped address from test II and test III */
    if (isSameIpAddress(&mappedAddress, &pStunAttributeMappedAddress->address, TRUE)) {
        natMappingBehavior = NAT_BEHAVIOR_ADDRESS_DEPENDENT;
    } else {
        natMappingBehavior = NAT_BEHAVIOR_PORT_DEPENDENT;
    }

CleanUp:

    if (pNatMappingBehavior != NULL) {
        *pNatMappingBehavior = natMappingBehavior;
    }

    if (bindingRequest != NULL) {
        freeStunPacket(&bindingRequest);
    }

    if (data != NULL) {
        for (i = 0; i < data->bindingResponseCount; ++i) {
            freeStunPacket(&data->bindingResponseList[i]);
        }
    }

    CHK_LOG_ERR(retStatus);
    return retStatus;
}

/*
 *  Follow the process described in https://tools.ietf.org/html/rfc5780#section-4.4 to discover the NAT filtering behavior
 */
STATUS discoverNatFilteringBehavior(PIceServer pStunServer, PNatTestData data, PSocketConnection pSocketConnection,
                                    NAT_BEHAVIOR* pNatFilteringBehavior)
{
    STATUS retStatus = STATUS_SUCCESS;
    PStunPacket bindingRequest = NULL, bindingResponse = NULL;
    NAT_BEHAVIOR natFilteringBehavior = NAT_BEHAVIOR_NONE;
    PStunAttributeChangeRequest pStunAttributeChangeRequest = NULL;
    UINT32 testIndex = 1, i = 0;

    CHK(pStunServer != NULL && data != NULL && pSocketConnection != NULL && pNatFilteringBehavior != NULL, STATUS_NULL_ARG);

    CHK_STATUS(createStunPacket(STUN_PACKET_TYPE_BINDING_REQUEST, NULL, &bindingRequest));

    /* execute test I */
    DLOGD("Running filtering behavior test I. Send binding request");
    CHK_STATUS(executeNatTest(bindingRequest, &pStunServer->ipAddress, pSocketConnection, testIndex++, data, &bindingResponse));
    if (bindingResponse == NULL) {
        natFilteringBehavior = NAT_BEHAVIOR_NO_UDP_CONNECTIVITY;
        CHK(FALSE, retStatus);
    }

    /* execute test II */
    DLOGD("Running filtering behavior test II. Send binding request with change ip and change port flag");
    CHK_STATUS(appendStunChangeRequestAttribute(bindingRequest,
                                                STUN_ATTRIBUTE_CHANGE_REQUEST_FLAG_CHANGE_IP | STUN_ATTRIBUTE_CHANGE_REQUEST_FLAG_CHANGE_PORT));

    CHK_STATUS(executeNatTest(bindingRequest, &pStunServer->ipAddress, pSocketConnection, testIndex++, data, &bindingResponse));
    if (bindingResponse != NULL) {
        natFilteringBehavior = NAT_BEHAVIOR_ENDPOINT_INDEPENDENT;
        CHK(FALSE, retStatus);
    }

    /* execute test III */
    DLOGD("Running filtering behavior test III. Send binding request with change port flag");
    CHK_STATUS(getStunAttribute(bindingRequest, STUN_ATTRIBUTE_TYPE_CHANGE_REQUEST, (PStunAttributeHeader*) &pStunAttributeChangeRequest));
    pStunAttributeChangeRequest->changeFlag = STUN_ATTRIBUTE_CHANGE_REQUEST_FLAG_CHANGE_PORT;

    CHK_STATUS(executeNatTest(bindingRequest, &pStunServer->ipAddress, pSocketConnection, testIndex++, data, &bindingResponse));

    if (bindingResponse != NULL) {
        natFilteringBehavior = NAT_BEHAVIOR_ADDRESS_DEPENDENT;
    } else {
        natFilteringBehavior = NAT_BEHAVIOR_PORT_DEPENDENT;
    }

CleanUp:

    if (pNatFilteringBehavior != NULL) {
        *pNatFilteringBehavior = natFilteringBehavior;
    }

    if (bindingRequest != NULL) {
        freeStunPacket(&bindingRequest);
    }

    if (data != NULL) {
        for (i = 0; i < data->bindingResponseCount; ++i) {
            freeStunPacket(&data->bindingResponseList[i]);
        }
    }

    CHK_LOG_ERR(retStatus);
    return retStatus;
}

STATUS discoverNatBehavior(PCHAR stunServer, NAT_BEHAVIOR* pNatMappingBehavior, NAT_BEHAVIOR* pNatFilteringBehavior,
                           IceSetInterfaceFilterFunc filterFunc, UINT64 filterFuncCustomData)
{
    STATUS retStatus = STATUS_SUCCESS;
    IceServer iceServerStun;
    PSocketConnection pSocketConnection = NULL;
    NatTestData customData;
    CVAR cvar = INVALID_CVAR_VALUE;
    MUTEX lock = INVALID_MUTEX_VALUE;
    UINT32 i;
    BOOL locked = FALSE;
    KvsIpAddress localNetworkInterfaces[MAX_LOCAL_NETWORK_INTERFACE_COUNT];
    UINT32 localNetworkInterfaceCount = ARRAY_SIZE(localNetworkInterfaces);
    PKvsIpAddress pSelectedLocalInterface = NULL;
    PConnectionListener pConnectionListener = NULL;

    CHK(stunServer != NULL && pNatMappingBehavior != NULL && pNatFilteringBehavior != NULL, STATUS_NULL_ARG);
    CHK(!IS_EMPTY_STRING(stunServer), STATUS_INVALID_ARG);

    MEMSET(&iceServerStun, 0x00, SIZEOF(IceServer));
    MEMSET(&customData, 0x00, SIZEOF(NatTestData));
    cvar = CVAR_CREATE();
    lock = MUTEX_CREATE(FALSE);
    CHK_STATUS(parseIceServer(&iceServerStun, stunServer, NULL, NULL));

    CHK_STATUS(getLocalhostIpAddresses(localNetworkInterfaces, &localNetworkInterfaceCount, filterFunc, filterFuncCustomData));

    customData.cvar = cvar;
    customData.lock = lock;
    customData.bindingResponseCount = 0;

    /* use the first usable local interface to create socket */
    for (i = 0; i < localNetworkInterfaceCount; ++i) {
        if (localNetworkInterfaces[i].family == iceServerStun.ipAddress.family) {
            pSelectedLocalInterface = &localNetworkInterfaces[i];
            break;
        }
    }
    CHK_WARN(pSelectedLocalInterface != NULL, retStatus, "No usable local interface");

    CHK_STATUS(createSocketConnection(iceServerStun.ipAddress.family, KVS_SOCKET_PROTOCOL_UDP, pSelectedLocalInterface, NULL, (UINT64) &customData,
                                      natTestIncomingDataHandler, 0, &pSocketConnection));
    ATOMIC_STORE_BOOL(&pSocketConnection->receiveData, TRUE);

    CHK_STATUS(createConnectionListener(&pConnectionListener));
    CHK_STATUS(connectionListenerAddConnection(pConnectionListener, pSocketConnection));
    CHK_STATUS(connectionListenerStart(pConnectionListener));

    MUTEX_LOCK(lock);
    locked = TRUE;

    DLOGD("Start NAT mapping behavior test.");
    CHK_STATUS(discoverNatMappingBehavior(&iceServerStun, &customData, pSocketConnection, pNatMappingBehavior));

    MUTEX_UNLOCK(lock);
    locked = FALSE;

    if (*pNatMappingBehavior == NAT_BEHAVIOR_NO_UDP_CONNECTIVITY || *pNatMappingBehavior == NAT_BEHAVIOR_NOT_BEHIND_ANY_NAT) {
        *pNatFilteringBehavior = *pNatMappingBehavior;
        CHK(FALSE, retStatus);
    }

    CHK_STATUS(connectionListenerRemoveAllConnection(pConnectionListener));
    freeSocketConnection(&pSocketConnection);
    CHK_STATUS(createSocketConnection(iceServerStun.ipAddress.family, KVS_SOCKET_PROTOCOL_UDP, pSelectedLocalInterface, NULL, (UINT64) &customData,
                                      natTestIncomingDataHandler, 0, &pSocketConnection));
    ATOMIC_STORE_BOOL(&pSocketConnection->receiveData, TRUE);
    CHK_STATUS(connectionListenerAddConnection(pConnectionListener, pSocketConnection));

    MUTEX_LOCK(lock);
    locked = TRUE;

    DLOGD("Start NAT filtering behavior test.");
    CHK_STATUS(discoverNatFilteringBehavior(&iceServerStun, &customData, pSocketConnection, pNatFilteringBehavior));

CleanUp:

    if (locked) {
        MUTEX_UNLOCK(lock);
    }

    if (pConnectionListener != NULL) {
        connectionListenerRemoveAllConnection(pConnectionListener);
        freeConnectionListener(&pConnectionListener);
    }

    if (pSocketConnection != NULL) {
        freeSocketConnection(&pSocketConnection);
    }

    for (i = 0; i < customData.bindingResponseCount; ++i) {
        freeStunPacket(&customData.bindingResponseList[i]);
    }

    if (cvar != INVALID_CVAR_VALUE) {
        CVAR_FREE(cvar);
    }

    if (lock != INVALID_MUTEX_VALUE) {
        MUTEX_FREE(lock);
    }

    CHK_LOG_ERR(retStatus);

    return retStatus;
}

PCHAR getNatBehaviorStr(NAT_BEHAVIOR natBehavior)
{
    switch (natBehavior) {
        case NAT_BEHAVIOR_NONE:
            return NAT_BEHAVIOR_NONE_STR;
        case NAT_BEHAVIOR_NOT_BEHIND_ANY_NAT:
            return NAT_BEHAVIOR_NOT_BEHIND_ANY_NAT_STR;
        case NAT_BEHAVIOR_NO_UDP_CONNECTIVITY:
            return NAT_BEHAVIOR_NO_UDP_CONNECTIVITY_STR;
        case NAT_BEHAVIOR_ENDPOINT_INDEPENDENT:
            return NAT_BEHAVIOR_ENDPOINT_INDEPENDENT_STR;
        case NAT_BEHAVIOR_ADDRESS_DEPENDENT:
            return NAT_BEHAVIOR_ADDRESS_DEPENDENT_STR;
        case NAT_BEHAVIOR_PORT_DEPENDENT:
            return NAT_BEHAVIOR_PORT_DEPENDENT_STR;
    }
}
