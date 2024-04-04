#define LOG_CLASS "Stun"
#include "../Include_i.h"
#include "kvsstun/stun_serializer.h"
#include "kvsstun/stun_deserializer.h"

// Convert error code from STUN library to STATUS.
extern STATUS convertStunErrorCode(StunResult_t stunResult);

STATUS stunPackageIpAddr(PStunHeader pStunHeader, STUN_ATTRIBUTE_TYPE type, PKvsIpAddress pAddress, PBYTE pBuffer, PUINT32 pDataLen)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 dataLen = 0;
    UINT16 size;
    PBYTE pCurrentBufferPosition = pBuffer;
    CHK(pDataLen != NULL && pAddress != NULL, STATUS_NULL_ARG);
    KvsIpAddress ipAddress;
    PKvsIpAddress pIndirected = pAddress;

    // Check if we are asked for size only and early return if so
    CHK(pAddress != NULL && pDataLen != NULL && pStunHeader != NULL, STATUS_NULL_ARG);

    /**
     * Mapped address attribute structure
     * https://tools.ietf.org/html/rfc5389#section-15.1
     * - 2 byte attribute type
     * - 2 byte attribute data len
     * - 2 byte address family
     * - 2 byte port
     * - 4 byte or 16 byte ip address
     */
    dataLen += STUN_ATTRIBUTE_HEADER_LEN + STUN_ATTRIBUTE_ADDRESS_HEADER_LEN;
    dataLen += IS_IPV4_ADDR(pIndirected) ? IPV4_ADDRESS_LENGTH : IPV6_ADDRESS_LENGTH;

    // Check if we are asked for size only and early return if so
    CHK(pBuffer != NULL, STATUS_SUCCESS);

    // Check if a large enough buffer had been passed in
    CHK(*pDataLen >= dataLen, STATUS_NOT_ENOUGH_MEMORY);

    // Fix-up the address and port number for the XOR type
    // NOTE: We are not doing it in place to not "dirty" the original
    if (type == STUN_ATTRIBUTE_TYPE_XOR_MAPPED_ADDRESS || type == STUN_ATTRIBUTE_TYPE_XOR_PEER_ADDRESS) {
        // Copy the struct forward so we can mutate
        ipAddress = *pAddress;

        CHK_STATUS(xorIpAddress(&ipAddress, pStunHeader->transactionId));

        pIndirected = &ipAddress;
    }

    size = (UINT16) (dataLen - STUN_ATTRIBUTE_HEADER_LEN);
    PACKAGE_STUN_ATTR_HEADER(pCurrentBufferPosition, type, size);
    pCurrentBufferPosition += STUN_ATTRIBUTE_HEADER_LEN;

    putInt16((PINT16) (pCurrentBufferPosition), pIndirected->family);
    pCurrentBufferPosition += STUN_ATTRIBUTE_ADDRESS_FAMILY_LEN;

    // port is already in network byte order
    MEMCPY(pCurrentBufferPosition, (PBYTE) &pIndirected->port, SIZEOF(pIndirected->port));
    pCurrentBufferPosition += SIZEOF(pIndirected->port);

    MEMCPY(pCurrentBufferPosition, pIndirected->address, IS_IPV4_ADDR(pIndirected) ? IPV4_ADDRESS_LENGTH : IPV6_ADDRESS_LENGTH);

CleanUp:

    if (STATUS_SUCCEEDED(retStatus) && pDataLen != NULL) {
        *pDataLen = dataLen;
    }

    LEAVES();
    return retStatus;
}

STATUS serializeStunPacket(PStunPacket pStunPacket, PBYTE password, UINT32 passwordLen, BOOL generateMessageIntegrity, BOOL generateFingerprint,
                           PBYTE pBuffer, PUINT32 pSize)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    StunResult_t stunResult = STUN_RESULT_OK;
    UINT32 i, crc32 = 0, hmacLen;
    UINT64 data64;
    PStunAttributeHeader pStunAttributeHeader;
    PStunAttributeAddress pStunAttributeAddress;
    PStunAttributeUsername pStunAttributeUsername;
    PStunAttributePriority pStunAttributePriority;
    PStunAttributeLifetime pStunAttributeLifetime;
    PStunAttributeChangeRequest pStunAttributeChangeRequest;
    PStunAttributeRequestedTransport pStunAttributeRequestedTransport;
    PStunAttributeRealm pStunAttributeRealm;
    PStunAttributeNonce pStunAttributeNonce;
    PStunAttributeErrorCode pStunAttributeErrorCode;
    PStunAttributeIceControl pStunAttributeIceControl;
    PStunAttributeData pStunAttributeData;
    PStunAttributeChannelNumber pStunAttributeChannelNumber;
    BOOL fingerprintFound = FALSE, messaageIntegrityFound = FALSE;
    StunContext_t stunContext;
    StunHeader_t stunHeader;
    StunAttributeAddress_t stunMappedAddress;

    CHK(pStunPacket != NULL && (!generateMessageIntegrity || password != NULL) && pSize != NULL, STATUS_NULL_ARG);
    CHK(password == NULL || passwordLen != 0, STATUS_INVALID_ARG);
    CHK(pStunPacket->header.magicCookie == STUN_HEADER_MAGIC_COOKIE, STATUS_STUN_MAGIC_COOKIE_MISMATCH);

    stunHeader.messageType = pStunPacket->header.stunMessageType;
    stunHeader.pTransactionId = pStunPacket->header.transactionId;

    stunResult = StunSerializer_Init(&stunContext, pBuffer, *pSize, &stunHeader);
    CHK(stunResult == STUN_RESULT_OK, convertStunErrorCode(stunResult));

    for (i = 0; i < pStunPacket->attributesCount; i++) {
        // Get the next attribute
        pStunAttributeHeader = pStunPacket->attributeList[i];

        switch (pStunAttributeHeader->type) {
            case STUN_ATTRIBUTE_TYPE_MAPPED_ADDRESS:
            case STUN_ATTRIBUTE_TYPE_XOR_MAPPED_ADDRESS:
            case STUN_ATTRIBUTE_TYPE_RESPONSE_ADDRESS:
            case STUN_ATTRIBUTE_TYPE_SOURCE_ADDRESS:
            case STUN_ATTRIBUTE_TYPE_REFLECTED_FROM:
            case STUN_ATTRIBUTE_TYPE_XOR_PEER_ADDRESS:
            case STUN_ATTRIBUTE_TYPE_CHANGED_ADDRESS:

                // TODO refactor this check, we have it for every attribute.
                CHK(!fingerprintFound && !messaageIntegrityFound, STATUS_STUN_ATTRIBUTES_AFTER_FINGERPRINT_MESSAGE_INTEGRITY);

                pStunAttributeAddress = (PStunAttributeAddress) pStunAttributeHeader;

                stunMappedAddress.family = pStunAttributeAddress->address.family;
                // For backward compatability
                stunMappedAddress.port = getInt16(pStunAttributeAddress->address.port);

                MEMCPY(&stunMappedAddress.address, &pStunAttributeAddress->address.address,
                       pStunAttributeAddress->attribute.length - STUN_ATTRIBUTE_ADDRESS_HEADER_LENGTH);

                stunResult = StunSerializer_AddAttributeAddress(&stunContext, &stunMappedAddress, pStunAttributeHeader->type);
                break;

            case STUN_ATTRIBUTE_TYPE_USERNAME:

                pStunAttributeUsername = (PStunAttributeUsername) pStunAttributeHeader;

                CHK(!fingerprintFound && !messaageIntegrityFound, STATUS_STUN_ATTRIBUTES_AFTER_FINGERPRINT_MESSAGE_INTEGRITY);

                stunResult = StunSerializer_AddAttributeUsername(&stunContext, ((const UINT8*) (pStunAttributeUsername + 1)),
                                                                 STRLEN((const PCHAR)(pStunAttributeUsername + 1)));

                break;

            case STUN_ATTRIBUTE_TYPE_PRIORITY:

                pStunAttributePriority = (PStunAttributePriority) pStunAttributeHeader;

                CHK(!fingerprintFound && !messaageIntegrityFound, STATUS_STUN_ATTRIBUTES_AFTER_FINGERPRINT_MESSAGE_INTEGRITY);

                stunResult = StunSerializer_AddAttributePriority(&stunContext, pStunAttributePriority->priority);

                break;

            case STUN_ATTRIBUTE_TYPE_USE_CANDIDATE:

                CHK(!fingerprintFound && !messaageIntegrityFound, STATUS_STUN_ATTRIBUTES_AFTER_FINGERPRINT_MESSAGE_INTEGRITY);

                stunResult = StunSerializer_AddAttributeUseCandidate(&stunContext);

                break;

            case STUN_ATTRIBUTE_TYPE_DONT_FRAGMENT:

                CHK(!fingerprintFound && !messaageIntegrityFound, STATUS_STUN_ATTRIBUTES_AFTER_FINGERPRINT_MESSAGE_INTEGRITY);

                stunResult = StunSerializer_AddAttributeDontFragment(&stunContext);

                break;

            case STUN_ATTRIBUTE_TYPE_LIFETIME:

                pStunAttributeLifetime = (PStunAttributeLifetime) pStunAttributeHeader;

                CHK(!fingerprintFound && !messaageIntegrityFound, STATUS_STUN_ATTRIBUTES_AFTER_FINGERPRINT_MESSAGE_INTEGRITY);

                stunResult = StunSerializer_AddAttributeLifetime(&stunContext, pStunAttributeLifetime->lifetime);

                break;

            case STUN_ATTRIBUTE_TYPE_CHANGE_REQUEST:

                pStunAttributeChangeRequest = (PStunAttributeChangeRequest) pStunAttributeHeader;

                CHK(!fingerprintFound && !messaageIntegrityFound, STATUS_STUN_ATTRIBUTES_AFTER_FINGERPRINT_MESSAGE_INTEGRITY);

                stunResult = StunSerializer_AddAttributeChangeRequest(&stunContext, pStunAttributeChangeRequest->changeFlag);

                break;

            case STUN_ATTRIBUTE_TYPE_REQUESTED_TRANSPORT:

                pStunAttributeRequestedTransport = (PStunAttributeRequestedTransport) pStunAttributeHeader;

                CHK(!fingerprintFound && !messaageIntegrityFound, STATUS_STUN_ATTRIBUTES_AFTER_FINGERPRINT_MESSAGE_INTEGRITY);

                stunResult = StunSerializer_AddAttributeRequestedTransport(&stunContext, pStunAttributeRequestedTransport->protocol,
                                                                           STUN_ATTRIBUTE_REQUESTED_TRANSPORT_PROTOCOL_LEN);

                break;

            case STUN_ATTRIBUTE_TYPE_REALM:

                pStunAttributeRealm = (PStunAttributeRealm) pStunAttributeHeader;

                CHK(!fingerprintFound && !messaageIntegrityFound, STATUS_STUN_ATTRIBUTES_AFTER_FINGERPRINT_MESSAGE_INTEGRITY);

                stunResult =
                    StunSerializer_AddAttributeRealm(&stunContext, (const UINT8*) pStunAttributeRealm->realm, pStunAttributeRealm->paddedLength);
                break;

            case STUN_ATTRIBUTE_TYPE_NONCE:

                pStunAttributeNonce = (PStunAttributeNonce) pStunAttributeHeader;

                CHK(!fingerprintFound && !messaageIntegrityFound, STATUS_STUN_ATTRIBUTES_AFTER_FINGERPRINT_MESSAGE_INTEGRITY);

                stunResult = StunSerializer_AddAttributeNonce(&stunContext, pStunAttributeNonce->nonce, pStunAttributeNonce->paddedLength);

                break;

            case STUN_ATTRIBUTE_TYPE_ERROR_CODE:

                pStunAttributeErrorCode = (PStunAttributeErrorCode) pStunAttributeHeader;

                CHK(!fingerprintFound && !messaageIntegrityFound, STATUS_STUN_ATTRIBUTES_AFTER_FINGERPRINT_MESSAGE_INTEGRITY);

                stunResult =
                    StunSerializer_AddAttributeErrorCode(&stunContext, pStunAttributeErrorCode->errorCode,
                                                         (const UINT8*) pStunAttributeErrorCode->errorPhrase, pStunAttributeErrorCode->paddedLength);

                break;

            case STUN_ATTRIBUTE_TYPE_ICE_CONTROLLED:
                pStunAttributeIceControl = (PStunAttributeIceControl) pStunAttributeHeader;

                CHK(!fingerprintFound && !messaageIntegrityFound, STATUS_STUN_ATTRIBUTES_AFTER_FINGERPRINT_MESSAGE_INTEGRITY);

                MEMCPY(&data64, (PBYTE) pStunAttributeIceControl + STUN_ATTRIBUTE_HEADER_LEN, SIZEOF(INT64));
                stunResult = StunSerializer_AddAttributeIceControlled(&stunContext, data64);

                break;

            case STUN_ATTRIBUTE_TYPE_ICE_CONTROLLING:

                pStunAttributeIceControl = (PStunAttributeIceControl) pStunAttributeHeader;

                CHK(!fingerprintFound && !messaageIntegrityFound, STATUS_STUN_ATTRIBUTES_AFTER_FINGERPRINT_MESSAGE_INTEGRITY);

                MEMCPY(&data64, (PBYTE) pStunAttributeIceControl + STUN_ATTRIBUTE_HEADER_LEN, SIZEOF(INT64));
                stunResult = StunSerializer_AddAttributeIceControlling(&stunContext, data64);

                break;

            case STUN_ATTRIBUTE_TYPE_DATA:

                pStunAttributeData = (PStunAttributeData) pStunAttributeHeader;

                CHK(!fingerprintFound && !messaageIntegrityFound, STATUS_STUN_ATTRIBUTES_AFTER_FINGERPRINT_MESSAGE_INTEGRITY);

                stunResult = StunSerializer_AddAttributeData(&stunContext, pStunAttributeData->data, pStunAttributeData->paddedLength);

                break;

            case STUN_ATTRIBUTE_TYPE_CHANNEL_NUMBER:

                pStunAttributeChannelNumber = (PStunAttributeChannelNumber) pStunAttributeHeader;

                CHK(!fingerprintFound && !messaageIntegrityFound, STATUS_STUN_ATTRIBUTES_AFTER_FINGERPRINT_MESSAGE_INTEGRITY);

                stunResult = StunSerializer_AddAttributeChannelNumber(&stunContext, pStunAttributeChannelNumber->channelNumber);

                break;

            case STUN_ATTRIBUTE_TYPE_MESSAGE_INTEGRITY:

                // Validate that the integrity is the last one or comes before fingerprint and ignore the attribute
                CHK(i == pStunPacket->attributesCount - 1 || i == pStunPacket->attributesCount - 2, STATUS_STUN_MESSAGE_INTEGRITY_NOT_LAST);

                CHK(!messaageIntegrityFound, STATUS_STUN_MULTIPLE_MESSAGE_INTEGRITY_ATTRIBUTES);
                CHK(!fingerprintFound, STATUS_STUN_MESSAGE_INTEGRITY_AFTER_FINGERPRINT);

                messaageIntegrityFound = TRUE;
                break;

            case STUN_ATTRIBUTE_TYPE_FINGERPRINT:

                // Validate that the fingerprint is the last and ignore the attribute
                CHK(i == pStunPacket->attributesCount - 1, STATUS_STUN_FINGERPRINT_NOT_LAST);
                CHK(!fingerprintFound, STATUS_STUN_MULTIPLE_FINGERPRINT_ATTRIBUTES);

                fingerprintFound = TRUE;
                break;

            default:
                // Do nothing
                break;
        }

        CHK(stunResult == STUN_RESULT_OK, convertStunErrorCode(stunResult));
    }

    // Check if we need to generate the message integrity attribute
    if (generateMessageIntegrity) {
        BYTE messageIntegrity[STUN_HMAC_VALUE_LEN];

        if (pBuffer != NULL) {
            PBYTE pIntegrityBuffer = NULL;
            UINT16 buffereLength;

            stunResult = StunSerializer_GetIntegrityBuffer(&stunContext, &pIntegrityBuffer, &buffereLength);
            CHK(stunResult == STUN_RESULT_OK, convertStunErrorCode(stunResult));
            KVS_SHA1_HMAC(password, (INT32) passwordLen, pIntegrityBuffer, buffereLength, messageIntegrity, &hmacLen);
        }

        stunResult = StunSerializer_AddAttributeIntegrity(&stunContext, messageIntegrity, STUN_HMAC_VALUE_LEN);
        CHK(stunResult == STUN_RESULT_OK, convertStunErrorCode(stunResult));
    }

    // Check if we need to generate the fingerprint attribute
    if (generateFingerprint) {
        if (pBuffer != NULL) {
            PBYTE pFingerprintBuffer = NULL;
            UINT16 buffereLength;

            stunResult = StunSerializer_GetFingerprintBuffer(&stunContext, &pFingerprintBuffer, &buffereLength);
            CHK(stunResult == STUN_RESULT_OK, convertStunErrorCode(stunResult));
            crc32 = COMPUTE_CRC32(pFingerprintBuffer, (UINT32) buffereLength) ^ STUN_FINGERPRINT_ATTRIBUTE_XOR_VALUE;
        }

        stunResult = StunSerializer_AddAttributeFingerprint(&stunContext, crc32);
        CHK(stunResult == STUN_RESULT_OK, convertStunErrorCode(stunResult));
    }

CleanUp:

    if (STATUS_SUCCEEDED(retStatus) && pSize != NULL) {
        stunResult = StunSerializer_Finalize(&stunContext, pSize);
        retStatus = convertStunErrorCode(stunResult);
    }

    LEAVES();
    return retStatus;
}

STATUS deserializeStunPacket(PBYTE pStunBuffer, UINT32 bufferSize, PBYTE password, UINT32 passwordLen, PStunPacket* ppStunPacket)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    StunResult_t stunResult = STUN_RESULT_OK;
    UINT32 attributeCount = 0, allocationSize, attributeSize, i = 0, magicCookie, hmacLen, crc32, crc32Fingerprint;
    UINT32 priority, lifetime, changeFlag;
    UINT16 size, paddedLength, ipFamily, messageLength;
    UINT64 data64;
    PStunAttributeHeader pDestAttribute;
    PStunHeader pStunHeader = (PStunHeader) pStunBuffer;
    PStunPacket pStunPacket = NULL;
    PStunAttributeAddress pStunAttributeAddress;
    PStunAttributeUsername pStunAttributeUsername;
    PStunAttributeMessageIntegrity pStunAttributeMessageIntegrity;
    PStunAttributeFingerprint pStunAttributeFingerprint;
    PStunAttributePriority pStunAttributePriority;
    PStunAttributeLifetime pStunAttributeLifetime;
    PStunAttributeChangeRequest pStunAttributeChangeRequest;
    PStunAttributeRequestedTransport pStunAttributeRequestedTransport;
    PStunAttributeRealm pStunAttributeRealm;
    PStunAttributeNonce pStunAttributeNonce;
    PStunAttributeErrorCode pStunAttributeErrorCode;
    PStunAttributeIceControl pStunAttributeIceControl;
    PStunAttributeData pStunAttributeData;
    PStunAttributeChannelNumber pStunAttributeChannelNumber;
    BOOL fingerprintFound = FALSE, messaageIntegrityFound = FALSE;
    StunContext_t stunContext;
    StunHeader_t stunHeader;
    StunAttribute_t stunAttribute;
    StunAttributeAddress_t stunMappedAddress;
    UINT8 *pErrorPhrase, *pBuffer = NULL;
    UINT16 errorPhraseLength, channelNumber, buffereLength, errorCode;

    CHK(pStunBuffer != NULL && ppStunPacket != NULL, STATUS_NULL_ARG);
    CHK(bufferSize >= STUN_HEADER_LEN, STATUS_INVALID_ARG);

    stunResult = StunDeserializer_Init(&(stunContext), pStunBuffer, bufferSize, &(stunHeader));
    CHK(stunResult == STUN_RESULT_OK, convertStunErrorCode(stunResult));

    // Copy and fix-up the header
    messageLength = (UINT16) getInt16(*(PUINT16) ((PBYTE) pStunHeader + STUN_HEADER_TYPE_LEN));
    magicCookie = (UINT32) getInt32(*(PUINT32) ((PBYTE) pStunHeader + STUN_HEADER_TYPE_LEN + STUN_HEADER_DATA_LEN));

    // Calculate the required size by getting the number of attributes
    allocationSize = SIZEOF(StunPacket);

    stunResult = StunDeserializer_GetNextAttribute(&(stunContext), &(stunAttribute));

    while (stunResult == STUN_RESULT_OK) {
        attributeSize = 0;

        // Check the type, get the allocation size and validate the length for each attribute
        switch (stunAttribute.attributeType) {
            case STUN_ATTRIBUTE_TYPE_MAPPED_ADDRESS:
            case STUN_ATTRIBUTE_TYPE_RESPONSE_ADDRESS:
            case STUN_ATTRIBUTE_TYPE_SOURCE_ADDRESS:
            case STUN_ATTRIBUTE_TYPE_REFLECTED_FROM:
            case STUN_ATTRIBUTE_TYPE_CHANGED_ADDRESS:
            case STUN_ATTRIBUTE_TYPE_XOR_MAPPED_ADDRESS:
            case STUN_ATTRIBUTE_TYPE_XOR_RELAYED_ADDRESS:
            case STUN_ATTRIBUTE_TYPE_XOR_PEER_ADDRESS:
                attributeSize = SIZEOF(StunAttributeAddress);

                stunResult = StunDeserializer_ParseAttributeAddress(&(stunContext), &stunAttribute, &stunMappedAddress);
                CHK(stunResult == STUN_RESULT_OK, convertStunErrorCode(stunResult));

                // Address family
                ipFamily = (UINT16) (stunMappedAddress.family);
                size = STUN_ATTRIBUTE_ADDRESS_HEADER_LEN + ((ipFamily == KVS_IP_FAMILY_TYPE_IPV4) ? IPV4_ADDRESS_LENGTH : IPV6_ADDRESS_LENGTH);

                CHK(stunAttribute.attributeValueLength == size, STATUS_STUN_INVALID_ADDRESS_ATTRIBUTE_LENGTH);
                CHK(!fingerprintFound && !messaageIntegrityFound, STATUS_STUN_ATTRIBUTES_AFTER_FINGERPRINT_MESSAGE_INTEGRITY);
                break;

            case STUN_ATTRIBUTE_TYPE_USERNAME:
                attributeSize = SIZEOF(StunAttributeUsername);

                paddedLength = (UINT16) ROUND_UP(stunAttribute.attributeValueLength, 4);
                // Validate the size of the length against the max value of username
                CHK(stunAttribute.attributeValueLength <= STUN_MAX_USERNAME_LEN, STATUS_STUN_INVALID_USERNAME_ATTRIBUTE_LENGTH);
                CHK(!fingerprintFound && !messaageIntegrityFound, STATUS_STUN_ATTRIBUTES_AFTER_FINGERPRINT_MESSAGE_INTEGRITY);

                // Add the length of the string itself
                attributeSize += paddedLength;
                break;

            case STUN_ATTRIBUTE_TYPE_PRIORITY:
                attributeSize = SIZEOF(StunAttributePriority);

                CHK(stunAttribute.attributeValueLength == STUN_ATTRIBUTE_PRIORITY_LEN, STATUS_STUN_INVALID_PRIORITY_ATTRIBUTE_LENGTH);
                CHK(!fingerprintFound && !messaageIntegrityFound, STATUS_STUN_ATTRIBUTES_AFTER_FINGERPRINT_MESSAGE_INTEGRITY);

                break;

            case STUN_ATTRIBUTE_TYPE_USE_CANDIDATE:
            case STUN_ATTRIBUTE_TYPE_DONT_FRAGMENT:
                attributeSize = SIZEOF(StunAttributeFlag);

                CHK(stunAttribute.attributeValueLength == STUN_ATTRIBUTE_FLAG_LEN, STATUS_STUN_INVALID_USE_CANDIDATE_ATTRIBUTE_LENGTH);
                CHK(!fingerprintFound && !messaageIntegrityFound, STATUS_STUN_ATTRIBUTES_AFTER_FINGERPRINT_MESSAGE_INTEGRITY);

                break;

            case STUN_ATTRIBUTE_TYPE_LIFETIME:
                attributeSize = SIZEOF(StunAttributeLifetime);

                CHK(stunAttribute.attributeValueLength == STUN_ATTRIBUTE_LIFETIME_LEN, STATUS_STUN_INVALID_LIFETIME_ATTRIBUTE_LENGTH);
                CHK(!fingerprintFound && !messaageIntegrityFound, STATUS_STUN_ATTRIBUTES_AFTER_FINGERPRINT_MESSAGE_INTEGRITY);

                break;

            case STUN_ATTRIBUTE_TYPE_CHANGE_REQUEST:
                attributeSize = SIZEOF(StunAttributeChangeRequest);

                CHK(stunAttribute.attributeValueLength == STUN_ATTRIBUTE_CHANGE_REQUEST_FLAG_LEN,
                    STATUS_STUN_INVALID_CHANGE_REQUEST_ATTRIBUTE_LENGTH);
                CHK(!fingerprintFound && !messaageIntegrityFound, STATUS_STUN_ATTRIBUTES_AFTER_FINGERPRINT_MESSAGE_INTEGRITY);

                break;

            case STUN_ATTRIBUTE_TYPE_REQUESTED_TRANSPORT:
                attributeSize = SIZEOF(StunAttributeRequestedTransport);

                CHK(stunAttribute.attributeValueLength == STUN_ATTRIBUTE_REQUESTED_TRANSPORT_PROTOCOL_LEN,
                    STATUS_STUN_INVALID_REQUESTED_TRANSPORT_ATTRIBUTE_LENGTH);
                CHK(!fingerprintFound && !messaageIntegrityFound, STATUS_STUN_ATTRIBUTES_AFTER_FINGERPRINT_MESSAGE_INTEGRITY);

                break;

            case STUN_ATTRIBUTE_TYPE_REALM:
                attributeSize = SIZEOF(StunAttributeRealm);

                paddedLength = (UINT16) ROUND_UP(stunAttribute.attributeValueLength, 4);
                // Validate the size of the length against the max value of realm
                CHK(stunAttribute.attributeValueLength <= STUN_MAX_REALM_LEN, STATUS_STUN_INVALID_REALM_ATTRIBUTE_LENGTH);
                CHK(!fingerprintFound && !messaageIntegrityFound, STATUS_STUN_ATTRIBUTES_AFTER_FINGERPRINT_MESSAGE_INTEGRITY);

                // Add the length of the string itself
                attributeSize += paddedLength;
                break;

            case STUN_ATTRIBUTE_TYPE_NONCE:
                attributeSize = SIZEOF(StunAttributeNonce);

                paddedLength = (UINT16) ROUND_UP(stunAttribute.attributeValueLength, 4);
                // Validate the size of the length against the max value of nonce
                CHK(stunAttribute.attributeValueLength <= STUN_MAX_NONCE_LEN, STATUS_STUN_INVALID_NONCE_ATTRIBUTE_LENGTH);
                CHK(!fingerprintFound && !messaageIntegrityFound, STATUS_STUN_ATTRIBUTES_AFTER_FINGERPRINT_MESSAGE_INTEGRITY);

                // Add the length of the string itself
                attributeSize += paddedLength;
                break;

            case STUN_ATTRIBUTE_TYPE_ERROR_CODE:
                attributeSize = SIZEOF(StunAttributeErrorCode);

                paddedLength = (UINT16) ROUND_UP((stunAttribute.attributeValueLength), 4);

                // Validate the size of the length against the max value of error phrase
                CHK(stunAttribute.attributeValueLength <= STUN_MAX_ERROR_PHRASE_LEN, STATUS_STUN_INVALID_ERROR_CODE_ATTRIBUTE_LENGTH);
                CHK(!fingerprintFound && !messaageIntegrityFound, STATUS_STUN_ATTRIBUTES_AFTER_FINGERPRINT_MESSAGE_INTEGRITY);

                // Add the length of the string itself
                attributeSize += paddedLength;
                break;

            case STUN_ATTRIBUTE_TYPE_ICE_CONTROLLED:
            case STUN_ATTRIBUTE_TYPE_ICE_CONTROLLING:
                attributeSize = SIZEOF(StunAttributeIceControl);

                CHK(stunAttribute.attributeValueLength == STUN_ATTRIBUTE_ICE_CONTROL_LEN, STATUS_STUN_INVALID_ICE_CONTROL_ATTRIBUTE_LENGTH);
                CHK(!fingerprintFound && !messaageIntegrityFound, STATUS_STUN_ATTRIBUTES_AFTER_FINGERPRINT_MESSAGE_INTEGRITY);

                break;

            case STUN_ATTRIBUTE_TYPE_DATA:
                attributeSize = SIZEOF(StunAttributeData);

                paddedLength = (UINT16) ROUND_UP(stunAttribute.attributeValueLength, 4);
                CHK(!fingerprintFound && !messaageIntegrityFound, STATUS_STUN_ATTRIBUTES_AFTER_FINGERPRINT_MESSAGE_INTEGRITY);

                // Add the length of the data itself
                attributeSize += paddedLength;
                break;

            case STUN_ATTRIBUTE_TYPE_CHANNEL_NUMBER:
                attributeSize = SIZEOF(StunAttributeChannelNumber);

                CHK(stunAttribute.attributeValueLength == STUN_ATTRIBUTE_CHANNEL_NUMBER_LEN, STATUS_STUN_INVALID_CHANNEL_NUMBER_ATTRIBUTE_LENGTH);
                CHK(!fingerprintFound && !messaageIntegrityFound, STATUS_STUN_ATTRIBUTES_AFTER_FINGERPRINT_MESSAGE_INTEGRITY);

                break;

            case STUN_ATTRIBUTE_TYPE_MESSAGE_INTEGRITY:
                attributeSize = SIZEOF(StunAttributeMessageIntegrity);

                CHK(stunAttribute.attributeValueLength == STUN_HMAC_VALUE_LEN, STATUS_STUN_INVALID_MESSAGE_INTEGRITY_ATTRIBUTE_LENGTH);
                CHK(!messaageIntegrityFound, STATUS_STUN_MULTIPLE_MESSAGE_INTEGRITY_ATTRIBUTES);
                CHK(!fingerprintFound, STATUS_STUN_MESSAGE_INTEGRITY_AFTER_FINGERPRINT);
                messaageIntegrityFound = TRUE;
                break;

            case STUN_ATTRIBUTE_TYPE_FINGERPRINT:
                attributeSize = SIZEOF(StunAttributeFingerprint);

                CHK(stunAttribute.attributeValueLength == STUN_ATTRIBUTE_FINGERPRINT_LEN, STATUS_STUN_INVALID_FINGERPRINT_ATTRIBUTE_LENGTH);
                CHK(!fingerprintFound, STATUS_STUN_MULTIPLE_FINGERPRINT_ATTRIBUTES);
                fingerprintFound = TRUE;
                break;

            default:
                // Do nothing - skip and decrement the count as it will be incremented below anyway
                attributeCount--;
                break;
        }

        allocationSize += attributeSize;
        attributeCount++;

        CHK(attributeCount <= STUN_ATTRIBUTE_MAX_COUNT, STATUS_STUN_MAX_ATTRIBUTE_COUNT);

        // Get the next attributes pointer
        stunResult = StunDeserializer_GetNextAttribute(&(stunContext), &(stunAttribute));
    }

    CHK(stunResult == STUN_RESULT_NO_MORE_ATTRIBUTE_FOUND, convertStunErrorCode(stunResult));

    // Account for the attribute pointer array
    allocationSize += attributeCount * SIZEOF(PStunAttributeHeader);

    // Allocate the necessary storage and set the pointers for the attributes
    CHK(NULL != (pStunPacket = MEMCALLOC(1, allocationSize)), STATUS_NOT_ENOUGH_MEMORY);

    stunResult = StunDeserializer_Init(&(stunContext), pStunBuffer, bufferSize, &(stunHeader));
    CHK(stunResult == STUN_RESULT_OK, convertStunErrorCode(stunResult));

    // Copy the header
    pStunPacket->header.stunMessageType = stunHeader.messageType;
    pStunPacket->header.messageLength = messageLength;
    pStunPacket->header.magicCookie = magicCookie;

    MEMCPY(pStunPacket->header.transactionId, stunHeader.pTransactionId, STUN_TRANSACTION_ID_LEN);
    // Store the actual allocation size
    pStunPacket->allocationSize = allocationSize;

    // Set the attribute array pointer
    pStunPacket->attributeList = (PStunAttributeHeader*) (pStunPacket + 1);

    // Set the count of the processed attributes only
    pStunPacket->attributesCount = attributeCount;

    // Set the attribute buffer start
    pDestAttribute = (PStunAttributeHeader) (pStunPacket->attributeList + attributeCount);

    // Start packaging the attributes
    while (i < attributeCount && (StunDeserializer_GetNextAttribute(&(stunContext), &(stunAttribute)) == STUN_RESULT_OK)) {
        // Set the array entry first
        pStunPacket->attributeList[i++] = pDestAttribute;

        // Copy the attribute header
        pDestAttribute->type = stunAttribute.attributeType;
        pDestAttribute->length = stunAttribute.attributeValueLength;

        // Zero out for before iteration
        attributeSize = 0;

        // Calculate the padded size
        paddedLength = (UINT16) ROUND_UP(pDestAttribute->length, 4);

        switch (pDestAttribute->type) {
            case STUN_ATTRIBUTE_TYPE_MAPPED_ADDRESS:
            case STUN_ATTRIBUTE_TYPE_RESPONSE_ADDRESS:
            case STUN_ATTRIBUTE_TYPE_SOURCE_ADDRESS:
            case STUN_ATTRIBUTE_TYPE_REFLECTED_FROM:
            case STUN_ATTRIBUTE_TYPE_CHANGED_ADDRESS:
            case STUN_ATTRIBUTE_TYPE_XOR_MAPPED_ADDRESS:
            case STUN_ATTRIBUTE_TYPE_XOR_RELAYED_ADDRESS:
            case STUN_ATTRIBUTE_TYPE_XOR_PEER_ADDRESS:
                pStunAttributeAddress = (PStunAttributeAddress) pDestAttribute;
                attributeSize = SIZEOF(StunAttributeAddress);

                stunResult = StunDeserializer_ParseAttributeAddress(&(stunContext), &stunAttribute, &stunMappedAddress);
                CHK(stunResult == STUN_RESULT_OK, convertStunErrorCode(stunResult));
                MEMCPY(&pStunAttributeAddress->address.address, &stunMappedAddress.address,
                       pStunAttributeAddress->attribute.length - STUN_ATTRIBUTE_ADDRESS_HEADER_LENGTH);
                pStunAttributeAddress->address.family = (UINT16) (stunMappedAddress.family);
                // getInt16 id done for backward compatability
                pStunAttributeAddress->address.port = (UINT16) getInt16(stunMappedAddress.port);

                break;

            case STUN_ATTRIBUTE_TYPE_USERNAME:
                pStunAttributeUsername = (PStunAttributeUsername) pDestAttribute;

                // Set the padded length
                pStunAttributeUsername->paddedLength = paddedLength;

                // Set the pointer following the structure
                pStunAttributeUsername->userName = (PCHAR) (pStunAttributeUsername + 1);

                // Copy the padded user name
                MEMCPY(pStunAttributeUsername->userName, stunAttribute.pAttributeValue, pStunAttributeUsername->paddedLength);
                attributeSize = SIZEOF(StunAttributeUsername) + pStunAttributeUsername->paddedLength;

                break;

            case STUN_ATTRIBUTE_TYPE_PRIORITY:
                pStunAttributePriority = (PStunAttributePriority) pDestAttribute;

                stunResult = StunDeserializer_ParseAttributePriority(&(stunContext), &(stunAttribute), &priority);
                CHK(stunResult == STUN_RESULT_OK, convertStunErrorCode(stunResult));

                pStunAttributePriority->priority = (UINT32) priority;

                attributeSize = SIZEOF(StunAttributePriority);

                break;

            case STUN_ATTRIBUTE_TYPE_USE_CANDIDATE:
            case STUN_ATTRIBUTE_TYPE_DONT_FRAGMENT:
                attributeSize = SIZEOF(StunAttributeFlag);

                break;

            case STUN_ATTRIBUTE_TYPE_LIFETIME:
                pStunAttributeLifetime = (PStunAttributeLifetime) pDestAttribute;

                stunResult = StunDeserializer_ParseAttributeLifetime(&(stunContext), &(stunAttribute), &lifetime);
                CHK(stunResult == STUN_RESULT_OK, convertStunErrorCode(stunResult));

                pStunAttributeLifetime->lifetime = (UINT32) getInt32(lifetime);

                attributeSize = SIZEOF(StunAttributeLifetime);

                break;

            case STUN_ATTRIBUTE_TYPE_CHANGE_REQUEST:
                pStunAttributeChangeRequest = (PStunAttributeChangeRequest) pDestAttribute;

                stunResult = StunDeserializer_ParseAttributeChangeRequest(&(stunContext), &(stunAttribute), &changeFlag);
                CHK(stunResult == STUN_RESULT_OK, convertStunErrorCode(stunResult));

                pStunAttributeChangeRequest->changeFlag = (UINT32) getInt32(changeFlag);

                attributeSize = SIZEOF(StunAttributeChangeRequest);

                break;

            case STUN_ATTRIBUTE_TYPE_REQUESTED_TRANSPORT:
                pStunAttributeRequestedTransport = (PStunAttributeRequestedTransport) pDestAttribute;

                MEMCPY(pStunAttributeRequestedTransport->protocol, (PBYTE) stunAttribute.pAttributeValue,
                       STUN_ATTRIBUTE_REQUESTED_TRANSPORT_PROTOCOL_LEN);

                attributeSize = SIZEOF(StunAttributeRequestedTransport);

                break;

            case STUN_ATTRIBUTE_TYPE_REALM:
                pStunAttributeRealm = (PStunAttributeRealm) pDestAttribute;

                // Set the padded length
                pStunAttributeRealm->paddedLength = paddedLength;

                // Set the pointer following the structure
                pStunAttributeRealm->realm = (PCHAR) (pStunAttributeRealm + 1);

                // Copy the padded realm
                MEMCPY(pStunAttributeRealm->realm, (PBYTE) stunAttribute.pAttributeValue, pStunAttributeRealm->paddedLength);
                attributeSize = SIZEOF(StunAttributeRealm) + pStunAttributeRealm->paddedLength;

                break;

            case STUN_ATTRIBUTE_TYPE_NONCE:
                pStunAttributeNonce = (PStunAttributeNonce) pDestAttribute;

                // Set the padded length
                pStunAttributeNonce->paddedLength = paddedLength;

                // Set the pointer following the structure
                pStunAttributeNonce->nonce = (PBYTE) (pStunAttributeNonce + 1);

                // Copy the padded nonce
                MEMCPY(pStunAttributeNonce->nonce, (PBYTE) stunAttribute.pAttributeValue, pStunAttributeNonce->paddedLength);
                attributeSize = SIZEOF(StunAttributeNonce) + pStunAttributeNonce->paddedLength;

                break;

            case STUN_ATTRIBUTE_TYPE_ERROR_CODE:
                pStunAttributeErrorCode = (PStunAttributeErrorCode) pDestAttribute;

                // Set the padded length
                pStunAttributeErrorCode->paddedLength = paddedLength;

                stunResult = StunDeserializer_ParseAttributeErrorCode(&stunAttribute, &errorCode, &pErrorPhrase, &errorPhraseLength);
                CHK(stunResult == STUN_RESULT_OK, convertStunErrorCode(stunResult));

                // swap the error code
                pStunAttributeErrorCode->errorCode = errorCode;

                // Set the pointer following the structure
                pStunAttributeErrorCode->errorPhrase = (PCHAR) (pStunAttributeErrorCode + 1);

                // Copy the padded error phrase
                MEMCPY(pStunAttributeErrorCode->errorPhrase, ((PBYTE) pErrorPhrase), errorPhraseLength);
                attributeSize = SIZEOF(StunAttributeErrorCode) + pStunAttributeErrorCode->paddedLength;

                break;

            case STUN_ATTRIBUTE_TYPE_ICE_CONTROLLED:
                pStunAttributeIceControl = (PStunAttributeIceControl) pDestAttribute;

                stunResult = StunDeserializer_ParseAttributeIceControlled(&(stunContext), &stunAttribute, &data64);
                CHK(stunResult == STUN_RESULT_OK, convertStunErrorCode(stunResult));

                // Swap the bits
                MEMCPY((PBYTE) pStunAttributeIceControl + SIZEOF(StunAttributeIceControl) - SIZEOF(UINT64), &data64, SIZEOF(INT64));

                attributeSize = SIZEOF(StunAttributeIceControl);

                break;

            case STUN_ATTRIBUTE_TYPE_ICE_CONTROLLING:
                pStunAttributeIceControl = (PStunAttributeIceControl) pDestAttribute;

                stunResult = StunDeserializer_ParseAttributeIceControlling(&(stunContext), &stunAttribute, &data64);
                CHK(stunResult == STUN_RESULT_OK, convertStunErrorCode(stunResult));

                // Swap the bits
                MEMCPY((PBYTE) pStunAttributeIceControl + SIZEOF(StunAttributeIceControl) - SIZEOF(UINT64), &data64, SIZEOF(INT64));

                attributeSize = SIZEOF(StunAttributeIceControl);

                break;

            case STUN_ATTRIBUTE_TYPE_DATA:
                pStunAttributeData = (PStunAttributeData) pDestAttribute;

                // Set the padded length
                pStunAttributeData->paddedLength = paddedLength;

                // Set the pointer following the structure
                pStunAttributeData->data = (PBYTE) (pStunAttributeData + 1);

                // Copy the padded nonce
                MEMCPY(pStunAttributeData->data, (PBYTE) stunAttribute.pAttributeValue, pStunAttributeData->paddedLength);
                attributeSize = SIZEOF(StunAttributeData) + pStunAttributeData->paddedLength;

                break;

            case STUN_ATTRIBUTE_TYPE_CHANNEL_NUMBER:
                pStunAttributeChannelNumber = (PStunAttributeChannelNumber) pDestAttribute;

                stunResult = StunDeserializer_ParseAttributeChannelNumber(&(stunContext), &(stunAttribute), &channelNumber);
                CHK(stunResult == STUN_RESULT_OK, convertStunErrorCode(stunResult));

                pStunAttributeChannelNumber->channelNumber = (UINT16) getInt16(channelNumber);

                pStunAttributeChannelNumber->reserve = 0;

                attributeSize = SIZEOF(StunAttributeChannelNumber);

                break;

            case STUN_ATTRIBUTE_TYPE_MESSAGE_INTEGRITY:
                CHK(password != NULL, STATUS_NULL_ARG);
                CHK(passwordLen != 0, STATUS_INVALID_ARG);

                pStunAttributeMessageIntegrity = (PStunAttributeMessageIntegrity) pDestAttribute;

                // Copy the message integrity
                attributeSize = SIZEOF(StunAttributeMessageIntegrity);

                stunResult = StunDeserializer_GetIntegrityBuffer(&(stunContext), &pBuffer, &buffereLength);
                CHK(stunResult == STUN_RESULT_OK, convertStunErrorCode(stunResult));

                KVS_SHA1_HMAC(password, (INT32) passwordLen, pBuffer, buffereLength, pStunAttributeMessageIntegrity->messageIntegrity, &hmacLen);

                // Validate the HMAC
                CHK(0 == MEMCMP(pStunAttributeMessageIntegrity->messageIntegrity, (PBYTE) stunAttribute.pAttributeValue, STUN_HMAC_VALUE_LEN),
                    STATUS_STUN_MESSAGE_INTEGRITY_MISMATCH);

                break;

            case STUN_ATTRIBUTE_TYPE_FINGERPRINT:

                pStunAttributeFingerprint = (PStunAttributeFingerprint) pDestAttribute;

                stunResult = StunDeserializer_ParseAttributeFingerprint(&(stunContext), &(stunAttribute), &crc32Fingerprint);
                CHK(stunResult == STUN_RESULT_OK, convertStunErrorCode(stunResult));

                stunResult = StunDeserializer_GetFingerprintBuffer(&(stunContext), &pBuffer, &buffereLength);
                CHK(stunResult == STUN_RESULT_OK, convertStunErrorCode(stunResult));

                crc32 = COMPUTE_CRC32(pBuffer, (UINT32) buffereLength) ^ STUN_FINGERPRINT_ATTRIBUTE_XOR_VALUE;

                // Validate the fingerprint
                CHK(crc32 == crc32Fingerprint, STATUS_STUN_FINGERPRINT_MISMATCH);

                break;

            default:
                // Skip over the unknown attributes
                break;
        }

        // Set the destination
        pDestAttribute = (PStunAttributeHeader) ((PBYTE) pDestAttribute + attributeSize);
    }

CleanUp:

    if (STATUS_FAILED(retStatus)) {
        freeStunPacket(&pStunPacket);
    }

    if (ppStunPacket != NULL) {
        *ppStunPacket = pStunPacket;
    }

    CHK_LOG_ERR(retStatus);

    LEAVES();
    return retStatus;
}

STATUS freeStunPacket(PStunPacket* ppStunPacket)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    CHK(ppStunPacket != NULL, STATUS_NULL_ARG);

    SAFE_MEMFREE(*ppStunPacket);

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS createStunPacket(STUN_PACKET_TYPE stunPacketType, PBYTE transactionId, PStunPacket* ppStunPacket)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 i;
    PStunPacket pStunPacket = NULL;

    CHK(ppStunPacket != NULL, STATUS_NULL_ARG);
    CHK(NULL != (pStunPacket = (PStunPacket) MEMCALLOC(1, STUN_PACKET_ALLOCATION_SIZE)), STATUS_NOT_ENOUGH_MEMORY);
    pStunPacket->attributesCount = 0;
    pStunPacket->header.messageLength = 0;
    pStunPacket->header.magicCookie = STUN_HEADER_MAGIC_COOKIE;
    pStunPacket->header.stunMessageType = stunPacketType;

    // Generate the transaction id if none is specified
    if (transactionId == NULL) {
        for (i = 0; i < STUN_TRANSACTION_ID_LEN; i++) {
            pStunPacket->header.transactionId[i] = (BYTE) (RAND() % 0xFF);
        }
    } else {
        MEMCPY(pStunPacket->header.transactionId, transactionId, STUN_TRANSACTION_ID_LEN);
    }

    // Set the address - calloc should have NULL-ified the actual pointers
    pStunPacket->attributeList = (PStunAttributeHeader*) (pStunPacket + 1);

    // Store the actual allocation size
    pStunPacket->allocationSize = STUN_PACKET_ALLOCATION_SIZE;

    *ppStunPacket = pStunPacket;

CleanUp:

    if (STATUS_FAILED(retStatus)) {
        freeStunPacket(&pStunPacket);
    }

    if (ppStunPacket != NULL) {
        *ppStunPacket = pStunPacket;
    }

    LEAVES();
    return retStatus;
}

STATUS appendStunAddressAttribute(PStunPacket pStunPacket, STUN_ATTRIBUTE_TYPE addressAttributeType, PKvsIpAddress pAddress)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PStunAttributeAddress pAttribute = NULL;
    PStunAttributeHeader pAttributeHeader = NULL;

    CHK(pAddress != NULL, STATUS_NULL_ARG);
    CHK_STATUS(getFirstAvailableStunAttribute(pStunPacket, &pAttributeHeader));
    pAttribute = (PStunAttributeAddress) pAttributeHeader;

    // Validate the overall size
    CHK((PBYTE) pStunPacket + pStunPacket->allocationSize >= (PBYTE) pAttribute + ROUND_UP(SIZEOF(StunAttributeAddress), 8),
        STATUS_NOT_ENOUGH_MEMORY);

    // Set up the new entry and copy data over
    pStunPacket->attributeList[pStunPacket->attributesCount++] = (PStunAttributeHeader) pAttribute;

    pAttribute->attribute.length = STUN_ATTRIBUTE_ADDRESS_HEADER_LEN + (IS_IPV4_ADDR(pAddress) ? IPV4_ADDRESS_LENGTH : IPV6_ADDRESS_LENGTH);
    pAttribute->attribute.type = addressAttributeType;

    // Copy the attribute entirely
    pAttribute->address = *pAddress;

    // Fix-up the STUN header message length
    pStunPacket->header.messageLength += pAttribute->attribute.length + STUN_ATTRIBUTE_HEADER_LEN;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS appendStunUsernameAttribute(PStunPacket pStunPacket, PCHAR userName)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PStunAttributeUsername pAttribute = NULL;
    PStunAttributeHeader pAttributeHeader = NULL;
    UINT16 length, paddedLength;

    CHK(userName != NULL, STATUS_NULL_ARG);

    CHK_STATUS(getFirstAvailableStunAttribute(pStunPacket, &pAttributeHeader));
    pAttribute = (PStunAttributeUsername) pAttributeHeader;

    length = (UINT16) STRNLEN(userName, STUN_MAX_USERNAME_LEN);
    paddedLength = (UINT16) ROUND_UP(length, 4);

    // Validate the overall size
    CHK((PBYTE) pStunPacket + pStunPacket->allocationSize >= (PBYTE) pAttribute + ROUND_UP(paddedLength + SIZEOF(StunAttributeUsername), 8),
        STATUS_NOT_ENOUGH_MEMORY);

    // Set up the new entry and copy data over
    pStunPacket->attributeList[pStunPacket->attributesCount++] = (PStunAttributeHeader) pAttribute;

    pAttribute->attribute.length = length;
    pAttribute->attribute.type = STUN_ATTRIBUTE_TYPE_USERNAME;

    // Set the padded length
    pAttribute->paddedLength = paddedLength;

    // Set the pointer following the structure
    pAttribute->userName = (PCHAR) (pAttribute + 1);

    MEMCPY(pAttribute->userName, userName, length * SIZEOF(CHAR));

    // Fix-up the STUN header message length
    pStunPacket->header.messageLength += paddedLength + STUN_ATTRIBUTE_HEADER_LEN;

CleanUp:

    CHK_LOG_ERR(retStatus);

    LEAVES();
    return retStatus;
}

STATUS getStunAttribute(PStunPacket pStunPacket, STUN_ATTRIBUTE_TYPE attributeType, PStunAttributeHeader* ppStunAttribute)
{
    STATUS retStatus = STATUS_SUCCESS;
    PStunAttributeHeader pTargetAttribute = NULL;
    UINT32 i;

    CHK(pStunPacket != NULL && ppStunAttribute != NULL, STATUS_NULL_ARG);

    for (i = 0; i < pStunPacket->attributesCount && pTargetAttribute == NULL; ++i) {
        if (pStunPacket->attributeList[i]->type == attributeType) {
            pTargetAttribute = pStunPacket->attributeList[i];
        }
    }

CleanUp:

    CHK_LOG_ERR(retStatus);

    if (ppStunAttribute != NULL) {
        *ppStunAttribute = pTargetAttribute;
    }

    return retStatus;
}

STATUS xorIpAddress(PKvsIpAddress pAddress, PBYTE pTransactionId)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 data;
    PBYTE pData;
    UINT32 i;

    CHK(pAddress != NULL, STATUS_NULL_ARG);
    CHK(IS_IPV4_ADDR(pAddress) || pTransactionId != NULL, STATUS_INVALID_ARG);

    // Perform the XOR-ing
    pAddress->port = (UINT16) (getInt16(STUN_HEADER_MAGIC_COOKIE >> 16)) ^ pAddress->port;

    data = (UINT32) getInt32(*(PINT32) pAddress->address);
    data ^= STUN_HEADER_MAGIC_COOKIE;
    putInt32((PINT32) pAddress->address, data);

    if (pAddress->family == KVS_IP_FAMILY_TYPE_IPV6) {
        // Process the rest of 12 bytes
        pData = &pAddress->address[SIZEOF(UINT32)];
        for (i = 0; i < STUN_TRANSACTION_ID_LEN; i++) {
            *pData++ ^= *pTransactionId++;
        }
    }

CleanUp:

    return retStatus;
}

STATUS appendStunPriorityAttribute(PStunPacket pStunPacket, UINT32 priority)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PStunAttributePriority pAttribute = NULL;
    PStunAttributeHeader pAttributeHeader = NULL;

    CHK_STATUS(getFirstAvailableStunAttribute(pStunPacket, &pAttributeHeader));
    pAttribute = (PStunAttributePriority) pAttributeHeader;

    // Validate the overall size
    CHK((PBYTE) pStunPacket + pStunPacket->allocationSize >= (PBYTE) pAttribute + ROUND_UP(SIZEOF(StunAttributePriority), 8),
        STATUS_NOT_ENOUGH_MEMORY);

    // Set up the new entry and copy data over
    pStunPacket->attributeList[pStunPacket->attributesCount++] = (PStunAttributeHeader) pAttribute;

    pAttribute->attribute.length = STUN_ATTRIBUTE_PRIORITY_LEN;
    pAttribute->attribute.type = STUN_ATTRIBUTE_TYPE_PRIORITY;

    // Set the priority
    pAttribute->priority = priority;

    // Fix-up the STUN header message length
    pStunPacket->header.messageLength += pAttribute->attribute.length + STUN_ATTRIBUTE_HEADER_LEN;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS appendStunFlagAttribute(PStunPacket pStunPacket, STUN_ATTRIBUTE_TYPE attrType)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PStunAttributeFlag pAttribute = NULL;
    PStunAttributeHeader pAttributeHeader = NULL;

    CHK_STATUS(getFirstAvailableStunAttribute(pStunPacket, &pAttributeHeader));
    pAttribute = (PStunAttributeFlag) pAttributeHeader;

    // Validate the overall size
    CHK((PBYTE) pStunPacket + pStunPacket->allocationSize >= (PBYTE) pAttribute + ROUND_UP(SIZEOF(StunAttributeFlag), 8), STATUS_NOT_ENOUGH_MEMORY);

    // Set up the new entry and copy data over
    pStunPacket->attributeList[pStunPacket->attributesCount++] = (PStunAttributeHeader) pAttribute;

    pAttribute->attribute.length = STUN_ATTRIBUTE_FLAG_LEN;
    pAttribute->attribute.type = attrType;

    // Fix-up the STUN header message length
    pStunPacket->header.messageLength += pAttribute->attribute.length + STUN_ATTRIBUTE_HEADER_LEN;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS appendStunLifetimeAttribute(PStunPacket pStunPacket, UINT32 lifetime)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PStunAttributeLifetime pAttribute = NULL;
    PStunAttributeHeader pAttributeHeader = NULL;

    CHK_STATUS(getFirstAvailableStunAttribute(pStunPacket, &pAttributeHeader));
    pAttribute = (PStunAttributeLifetime) pAttributeHeader;

    // Validate the overall size
    CHK((PBYTE) pStunPacket + pStunPacket->allocationSize >= (PBYTE) pAttribute + ROUND_UP(SIZEOF(StunAttributeLifetime), 8),
        STATUS_NOT_ENOUGH_MEMORY);

    // Set up the new entry and copy data over
    pStunPacket->attributeList[pStunPacket->attributesCount++] = (PStunAttributeHeader) pAttribute;

    pAttribute->attribute.length = STUN_ATTRIBUTE_LIFETIME_LEN;
    pAttribute->attribute.type = STUN_ATTRIBUTE_TYPE_LIFETIME;

    // Set the lifetime
    pAttribute->lifetime = lifetime;

    // Fix-up the STUN header message length
    pStunPacket->header.messageLength += pAttribute->attribute.length + STUN_ATTRIBUTE_HEADER_LEN;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS appendStunChangeRequestAttribute(PStunPacket pStunPacket, UINT32 changeFlag)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PStunAttributeChangeRequest pAttribute = NULL;
    PStunAttributeHeader pAttributeHeader = NULL;

    CHK_STATUS(getFirstAvailableStunAttribute(pStunPacket, &pAttributeHeader));
    pAttribute = (PStunAttributeChangeRequest) pAttributeHeader;

    // Validate the overall size
    CHK((PBYTE) pStunPacket + pStunPacket->allocationSize >= (PBYTE) pAttribute + ROUND_UP(SIZEOF(StunAttributeChangeRequest), 8),
        STATUS_NOT_ENOUGH_MEMORY);

    // Set up the new entry and copy data over
    pStunPacket->attributeList[pStunPacket->attributesCount++] = (PStunAttributeHeader) pAttribute;

    pAttribute->attribute.length = STUN_ATTRIBUTE_CHANGE_REQUEST_FLAG_LEN;
    pAttribute->attribute.type = STUN_ATTRIBUTE_TYPE_CHANGE_REQUEST;

    // Set the change flag
    pAttribute->changeFlag = changeFlag;

    // Fix-up the STUN header message length
    pStunPacket->header.messageLength += pAttribute->attribute.length + STUN_ATTRIBUTE_HEADER_LEN;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS appendStunRequestedTransportAttribute(PStunPacket pStunPacket, UINT8 protocol)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PStunAttributeRequestedTransport pAttribute = NULL;
    PStunAttributeHeader pAttributeHeader = NULL;

    CHK_STATUS(getFirstAvailableStunAttribute(pStunPacket, &pAttributeHeader));
    pAttribute = (PStunAttributeRequestedTransport) pAttributeHeader;

    // Validate the overall size
    CHK((PBYTE) pStunPacket + pStunPacket->allocationSize >= (PBYTE) pAttribute + ROUND_UP(SIZEOF(StunAttributeRequestedTransport), 8),
        STATUS_NOT_ENOUGH_MEMORY);

    // Set up the new entry and copy data over
    pStunPacket->attributeList[pStunPacket->attributesCount++] = (PStunAttributeHeader) pAttribute;

    pAttribute->attribute.length = STUN_ATTRIBUTE_REQUESTED_TRANSPORT_PROTOCOL_LEN;
    pAttribute->attribute.type = STUN_ATTRIBUTE_TYPE_REQUESTED_TRANSPORT;

    // Set the protocol
    MEMSET(pAttribute->protocol, 0x00, STUN_ATTRIBUTE_REQUESTED_TRANSPORT_PROTOCOL_LEN);
    *pAttribute->protocol = (BYTE) protocol;

    // Fix-up the STUN header message length
    pStunPacket->header.messageLength += pAttribute->attribute.length + STUN_ATTRIBUTE_HEADER_LEN;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS appendStunRealmAttribute(PStunPacket pStunPacket, PCHAR realm)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PStunAttributeRealm pAttribute = NULL;
    PStunAttributeHeader pAttributeHeader = NULL;
    UINT16 length, paddedLength;

    CHK(realm != NULL, STATUS_NULL_ARG);

    CHK_STATUS(getFirstAvailableStunAttribute(pStunPacket, &pAttributeHeader));
    pAttribute = (PStunAttributeRealm) pAttributeHeader;

    length = (UINT16) STRNLEN(realm, STUN_MAX_REALM_LEN);
    paddedLength = (UINT16) ROUND_UP(length, 4);

    // Validate the overall size
    CHK((PBYTE) pStunPacket + pStunPacket->allocationSize >= (PBYTE) pAttribute + ROUND_UP(paddedLength + SIZEOF(StunAttributeRealm), 8),
        STATUS_NOT_ENOUGH_MEMORY);

    // Set up the new entry and copy data over
    pStunPacket->attributeList[pStunPacket->attributesCount++] = (PStunAttributeHeader) pAttribute;

    pAttribute->attribute.length = length;
    pAttribute->attribute.type = STUN_ATTRIBUTE_TYPE_REALM;

    // Set the padded length
    pAttribute->paddedLength = paddedLength;

    // Set the pointer following the structure
    pAttribute->realm = (PCHAR) (pAttribute + 1);

    MEMCPY(pAttribute->realm, realm, length * SIZEOF(CHAR));

    // Fix-up the STUN header message length
    pStunPacket->header.messageLength += paddedLength + STUN_ATTRIBUTE_HEADER_LEN;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS appendStunNonceAttribute(PStunPacket pStunPacket, PBYTE nonce, UINT16 nonceLen)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PStunAttributeNonce pAttribute = NULL;
    PStunAttributeHeader pAttributeHeader = NULL;
    UINT16 paddedLength;

    CHK(nonce != NULL, STATUS_NULL_ARG);

    CHK_STATUS(getFirstAvailableStunAttribute(pStunPacket, &pAttributeHeader));
    pAttribute = (PStunAttributeNonce) pAttributeHeader;

    paddedLength = (UINT16) ROUND_UP(nonceLen, 4);

    // Validate the overall size
    CHK((PBYTE) pStunPacket + pStunPacket->allocationSize >= (PBYTE) pAttribute + ROUND_UP(paddedLength + SIZEOF(StunAttributeNonce), 8),
        STATUS_NOT_ENOUGH_MEMORY);

    // Set up the new entry and copy data over
    pStunPacket->attributeList[pStunPacket->attributesCount++] = (PStunAttributeHeader) pAttribute;

    pAttribute->attribute.length = nonceLen;
    pAttribute->attribute.type = STUN_ATTRIBUTE_TYPE_NONCE;

    // Set the padded length
    pAttribute->paddedLength = paddedLength;

    // Set the pointer following the structure
    pAttribute->nonce = (PBYTE) (pAttribute + 1);

    MEMCPY(pAttribute->nonce, nonce, nonceLen);

    // Fix-up the STUN header message length
    pStunPacket->header.messageLength += paddedLength + STUN_ATTRIBUTE_HEADER_LEN;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS updateStunNonceAttribute(PStunPacket pStunPacket, PBYTE nonce, UINT16 nonceLen)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PStunAttributeNonce pAttribute = NULL;
    PStunAttributeHeader pAttributeHeader = NULL;

    CHK(pStunPacket != NULL && nonce != NULL, STATUS_NULL_ARG);

    CHK_STATUS(getStunAttribute(pStunPacket, STUN_ATTRIBUTE_TYPE_NONCE, &pAttributeHeader));
    // do nothing if nonce attribute not found
    CHK(pAttributeHeader != NULL, retStatus);

    pAttribute = (PStunAttributeNonce) pAttributeHeader;

    // not expecting nonce length to change while streaming
    CHK_WARN(pAttributeHeader->length == nonceLen, STATUS_INVALID_ARG, "Nonce length should not change");

    MEMCPY(pAttribute->nonce, nonce, nonceLen);

CleanUp:

    CHK_LOG_ERR(retStatus);

    LEAVES();
    return retStatus;
}

STATUS appendStunErrorCodeAttribute(PStunPacket pStunPacket, PCHAR errorPhrase, UINT16 errorCode)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PStunAttributeErrorCode pAttribute = NULL;
    PStunAttributeHeader pAttributeHeader = NULL;
    UINT16 length, paddedLength;

    CHK(errorPhrase != NULL, STATUS_NULL_ARG);

    CHK_STATUS(getFirstAvailableStunAttribute(pStunPacket, &pAttributeHeader));
    pAttribute = (PStunAttributeErrorCode) pAttributeHeader;

    length = (UINT16) STRNLEN(errorPhrase, STUN_MAX_ERROR_PHRASE_LEN);
    paddedLength = (UINT16) ROUND_UP(length, 4);

    // Validate the overall size
    CHK((PBYTE) pStunPacket + pStunPacket->allocationSize >= (PBYTE) pAttribute + ROUND_UP(paddedLength + SIZEOF(StunAttributeErrorCode), 8),
        STATUS_NOT_ENOUGH_MEMORY);

    // Set up the new entry and copy data over
    pStunPacket->attributeList[pStunPacket->attributesCount++] = (PStunAttributeHeader) pAttribute;

    pAttribute->attribute.length = length;
    pAttribute->attribute.type = STUN_ATTRIBUTE_TYPE_ERROR_CODE;

    // Set the padded length
    pAttribute->paddedLength = paddedLength;

    pAttribute->errorCode = errorCode;

    // Set the pointer following the structure
    pAttribute->errorPhrase = (PCHAR) (pAttribute + 1);

    MEMCPY(pAttribute->errorPhrase, errorPhrase, length * SIZEOF(CHAR));

    // Fix-up the STUN header message length
    pStunPacket->header.messageLength += paddedLength + STUN_ATTRIBUTE_HEADER_LEN;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS appendStunIceControllAttribute(PStunPacket pStunPacket, STUN_ATTRIBUTE_TYPE attributeType, UINT64 tieBreaker)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PStunAttributeIceControl pAttribute = NULL;
    PStunAttributeHeader pAttributeHeader = NULL;

    CHK(attributeType == STUN_ATTRIBUTE_TYPE_ICE_CONTROLLING || attributeType == STUN_ATTRIBUTE_TYPE_ICE_CONTROLLED, STATUS_INVALID_ARG);

    CHK_STATUS(getFirstAvailableStunAttribute(pStunPacket, &pAttributeHeader));
    pAttribute = (PStunAttributeIceControl) pAttributeHeader;

    // Validate the overall size
    CHK((PBYTE) pStunPacket + pStunPacket->allocationSize >= (PBYTE) pAttribute + ROUND_UP(SIZEOF(StunAttributeIceControl), 8),
        STATUS_NOT_ENOUGH_MEMORY);

    // Set up the new entry and copy data over
    pStunPacket->attributeList[pStunPacket->attributesCount++] = (PStunAttributeHeader) pAttribute;

    pAttribute->attribute.length = STUN_ATTRIBUTE_ICE_CONTROL_LEN;
    pAttribute->attribute.type = attributeType;

    // Set the tiebreaker
    MEMCPY((PBYTE) pAttribute + STUN_ATTRIBUTE_HEADER_LEN, &tieBreaker, SIZEOF(UINT64));

    // Fix-up the STUN header message length
    pStunPacket->header.messageLength += pAttribute->attribute.length + STUN_ATTRIBUTE_HEADER_LEN;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS appendStunDataAttribute(PStunPacket pStunPacket, PBYTE data, UINT16 dataLen)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PStunAttributeData pAttribute = NULL;
    PStunAttributeHeader pAttributeHeader = NULL;
    UINT16 paddedLength;

    CHK(data != NULL, STATUS_NULL_ARG);

    CHK_STATUS(getFirstAvailableStunAttribute(pStunPacket, &pAttributeHeader));
    pAttribute = (PStunAttributeData) pAttributeHeader;

    paddedLength = (UINT16) ROUND_UP(dataLen, 4);

    // Validate the overall size
    CHK((PBYTE) pStunPacket + pStunPacket->allocationSize >= (PBYTE) pAttribute + ROUND_UP(paddedLength + SIZEOF(StunAttributeData), 8),
        STATUS_NOT_ENOUGH_MEMORY);

    // Set up the new entry and copy data over
    pStunPacket->attributeList[pStunPacket->attributesCount++] = (PStunAttributeHeader) pAttribute;

    pAttribute->attribute.length = dataLen;
    pAttribute->attribute.type = STUN_ATTRIBUTE_TYPE_DATA;

    // Set the padded length
    pAttribute->paddedLength = paddedLength;

    // Set the pointer following the structure
    pAttribute->data = (PBYTE) (pAttribute + 1);

    MEMCPY(pAttribute->data, data, dataLen);

    // Fix-up the STUN header message length
    pStunPacket->header.messageLength += paddedLength + STUN_ATTRIBUTE_HEADER_LEN;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS appendStunChannelNumberAttribute(PStunPacket pStunPacket, UINT16 channelNumber)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PStunAttributeChannelNumber pAttribute = NULL;
    PStunAttributeHeader pAttributeHeader = NULL;

    CHK_STATUS(getFirstAvailableStunAttribute(pStunPacket, &pAttributeHeader));
    pAttribute = (PStunAttributeChannelNumber) pAttributeHeader;

    // Validate the overall size
    CHK((PBYTE) pStunPacket + pStunPacket->allocationSize >= (PBYTE) pAttribute + ROUND_UP(SIZEOF(StunAttributeChannelNumber), 8),
        STATUS_NOT_ENOUGH_MEMORY);

    // Set up the new entry and copy data over
    pStunPacket->attributeList[pStunPacket->attributesCount++] = (PStunAttributeHeader) pAttribute;

    pAttribute->attribute.length = STUN_ATTRIBUTE_CHANNEL_NUMBER_LEN;
    pAttribute->attribute.type = STUN_ATTRIBUTE_TYPE_CHANNEL_NUMBER;

    // Set the channel number and reserve
    pAttribute->channelNumber = channelNumber;
    pAttribute->reserve = 0;

    // Fix-up the STUN header message length
    pStunPacket->header.messageLength += pAttribute->attribute.length + STUN_ATTRIBUTE_HEADER_LEN;

CleanUp:

    LEAVES();
    return retStatus;
}

UINT16 getPackagedStunAttributeSize(PStunAttributeHeader pStunAttributeHeader)
{
    UINT16 length;

    switch (pStunAttributeHeader->type) {
        case STUN_ATTRIBUTE_TYPE_MAPPED_ADDRESS:
        case STUN_ATTRIBUTE_TYPE_XOR_MAPPED_ADDRESS:
        case STUN_ATTRIBUTE_TYPE_RESPONSE_ADDRESS:
        case STUN_ATTRIBUTE_TYPE_SOURCE_ADDRESS:
        case STUN_ATTRIBUTE_TYPE_REFLECTED_FROM:
        case STUN_ATTRIBUTE_TYPE_XOR_PEER_ADDRESS:
        case STUN_ATTRIBUTE_TYPE_CHANGED_ADDRESS:
            length = SIZEOF(StunAttributeAddress);
            break;
        case STUN_ATTRIBUTE_TYPE_USE_CANDIDATE:
        case STUN_ATTRIBUTE_TYPE_DONT_FRAGMENT:
            length = SIZEOF(StunAttributeFlag);
            break;
        case STUN_ATTRIBUTE_TYPE_PRIORITY:
            length = SIZEOF(StunAttributePriority);
            break;
        case STUN_ATTRIBUTE_TYPE_LIFETIME:
            length = SIZEOF(StunAttributeLifetime);
            break;
        case STUN_ATTRIBUTE_TYPE_CHANGE_REQUEST:
            length = SIZEOF(StunAttributeChangeRequest);
            break;
        case STUN_ATTRIBUTE_TYPE_REQUESTED_TRANSPORT:
            length = SIZEOF(StunAttributeRequestedTransport);
            break;
        case STUN_ATTRIBUTE_TYPE_ICE_CONTROLLED:
        case STUN_ATTRIBUTE_TYPE_ICE_CONTROLLING:
            length = SIZEOF(StunAttributeIceControl);
            break;
        case STUN_ATTRIBUTE_TYPE_REALM:
            length = SIZEOF(StunAttributeRealm) + ((PStunAttributeRealm) pStunAttributeHeader)->paddedLength;
            break;
        case STUN_ATTRIBUTE_TYPE_NONCE:
            length = SIZEOF(StunAttributeNonce) + ((PStunAttributeNonce) pStunAttributeHeader)->paddedLength;
            break;
        case STUN_ATTRIBUTE_TYPE_DATA:
            length = SIZEOF(StunAttributeData) + ((PStunAttributeData) pStunAttributeHeader)->paddedLength;
            break;
        case STUN_ATTRIBUTE_TYPE_USERNAME:
            length = SIZEOF(StunAttributeUsername) + ((PStunAttributeUsername) pStunAttributeHeader)->paddedLength;
            break;
        case STUN_ATTRIBUTE_TYPE_ERROR_CODE:
            length = SIZEOF(StunAttributeErrorCode) + ((PStunAttributeErrorCode) pStunAttributeHeader)->paddedLength;
            break;
        case STUN_ATTRIBUTE_TYPE_CHANNEL_NUMBER:
            length = SIZEOF(StunAttributeChannelNumber);
            break;
        default:
            length = STUN_ATTRIBUTE_HEADER_LEN + pStunAttributeHeader->length;
    }

    return (UINT16) ROUND_UP(length, 8);
}

STATUS getFirstAvailableStunAttribute(PStunPacket pStunPacket, PStunAttributeHeader* ppStunAttribute)
{
    STATUS retStatus = STATUS_SUCCESS;
    PStunAttributeHeader pAttribute = NULL;

    CHK(pStunPacket != NULL && ppStunAttribute != NULL, STATUS_NULL_ARG);
    CHK(pStunPacket->attributesCount <= STUN_ATTRIBUTE_MAX_COUNT, STATUS_STUN_MAX_ATTRIBUTE_COUNT);

    if (pStunPacket->attributesCount != 0) {
        // Get the next address pointer
        pAttribute = pStunPacket->attributeList[pStunPacket->attributesCount - 1];

        // Validate if we have a terminal attribute
        CHK(pAttribute->type != STUN_ATTRIBUTE_TYPE_MESSAGE_INTEGRITY && pAttribute->type != STUN_ATTRIBUTE_TYPE_FINGERPRINT,
            STATUS_STUN_ATTRIBUTES_AFTER_FINGERPRINT_MESSAGE_INTEGRITY);

        // Calculate the first available address
        pAttribute = (PStunAttributeHeader) (((PBYTE) pAttribute) + getPackagedStunAttributeSize(pAttribute));

        // Validate we are still within the allocation
        CHK((PBYTE) pStunPacket + pStunPacket->allocationSize > (PBYTE) pAttribute, STATUS_NOT_ENOUGH_MEMORY);
    } else {
        // Set the attribute to the first one
        pAttribute = (PStunAttributeHeader) (pStunPacket->attributeList + STUN_ATTRIBUTE_MAX_COUNT);
    }

    *ppStunAttribute = pAttribute;

CleanUp:

    LEAVES();
    return retStatus;
}
