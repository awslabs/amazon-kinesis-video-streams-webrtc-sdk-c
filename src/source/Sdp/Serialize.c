#define LOG_CLASS "SDPSerialize"
#include "../Include_i.h"
#include "kvssdp/sdp_serializer.h"

static STATUS serializeVersion(SdpSerializerContext_t* pCtx, UINT64 version)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    SdpResult_t sdpResult = SDP_RESULT_OK;

    sdpResult = SdpSerializer_AddU64(pCtx, SDP_TYPE_VERSION, version);

    if (sdpResult == SDP_RESULT_OK) {
        retStatus = STATUS_SUCCESS;
    } else if (sdpResult == SDP_RESULT_OUT_OF_MEMORY) {
        retStatus = STATUS_BUFFER_TOO_SMALL;
    } else {
        retStatus = sdpResult;
    }

    LEAVES();
    return retStatus;
}

static STATUS serializeOrigin(SdpSerializerContext_t* pCtx, PSdpOrigin pSDPOrigin)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    SdpResult_t sdpResult = SDP_RESULT_OK;
    SdpOriginator_t origin;

    CHK(pSDPOrigin != NULL, STATUS_NULL_ARG);

    if (pSDPOrigin->userName[0] != '\0' && pSDPOrigin->sdpConnectionInformation.networkType[0] != '\0' &&
        pSDPOrigin->sdpConnectionInformation.addressType[0] != '\0' && pSDPOrigin->sdpConnectionInformation.connectionAddress[0] != '\0') {
        origin.pUserName = pSDPOrigin->userName;
        origin.userNameLength = STRNLEN(pSDPOrigin->userName, MAX_SDP_SESSION_USERNAME_LENGTH);
        origin.sessionId = pSDPOrigin->sessionId;
        origin.sessionVersion = pSDPOrigin->sessionVersion;

        CHK(STRCMP(pSDPOrigin->sdpConnectionInformation.networkType, "IN") == 0, STATUS_INVALID_ARG);

        origin.connectionInfo.networkType = SDP_NETWORK_IN;
    }

    CHK(STRCMP(pSDPOrigin->sdpConnectionInformation.addressType, "IP4") == 0 || STRCMP(pSDPOrigin->sdpConnectionInformation.addressType, "IP6") == 0,
        STATUS_INVALID_ARG);

    if (STRCMP(pSDPOrigin->sdpConnectionInformation.addressType, "IP4") == 0) {
        origin.connectionInfo.addressType = SDP_ADDRESS_IPV4;
    } else {
        origin.connectionInfo.addressType = SDP_ADDRESS_IPV6;
    }

    origin.connectionInfo.pAddress = pSDPOrigin->sdpConnectionInformation.connectionAddress;
    origin.connectionInfo.addressLength = STRNLEN(pSDPOrigin->sdpConnectionInformation.connectionAddress, MAX_SDP_CONNECTION_ADDRESS_LENGTH);

    sdpResult = SdpSerializer_AddOriginator(pCtx, SDP_TYPE_ORIGINATOR, &origin);

    if (sdpResult == SDP_RESULT_OK) {
        retStatus = STATUS_SUCCESS;
    } else if (sdpResult == SDP_RESULT_OUT_OF_MEMORY) {
        retStatus = STATUS_BUFFER_TOO_SMALL;
    } else {
        retStatus = sdpResult;
    }

CleanUp:

    LEAVES();
    return retStatus;
}

static STATUS serializeSessionName(SdpSerializerContext_t* pCtx, PCHAR sessionName, UINT32 sessionNameSize)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    SdpResult_t sdpResult = SDP_RESULT_OK;

    /* Check if session name available. */
    if ((sessionName != NULL) && (sessionName[0] != '\0')) {
        sdpResult = SdpSerializer_AddBuffer(pCtx, SDP_TYPE_SESSION_NAME, sessionName, STRNLEN(sessionName, sessionNameSize));

        if (sdpResult == SDP_RESULT_OK) {
            retStatus = STATUS_SUCCESS;
        } else if (sdpResult == SDP_RESULT_OUT_OF_MEMORY) {
            retStatus = STATUS_BUFFER_TOO_SMALL;
        } else {
            retStatus = sdpResult;
        }
    }

    LEAVES();
    return retStatus;
}

static STATUS serializeTimeDescription(SdpSerializerContext_t* pCtx, PSdpTimeDescription pSDPTimeDescription)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    SdpResult_t sdpResult = SDP_RESULT_OK;
    SdpTimeDescription_t time;

    CHK(pSDPTimeDescription != NULL, STATUS_NULL_ARG);

    time.startTime = pSDPTimeDescription->startTime;
    time.stopTime = pSDPTimeDescription->stopTime;
    sdpResult = SdpSerializer_AddTimeActive(pCtx, SDP_TYPE_TIME_ACTIVE, &time);

    if (sdpResult == SDP_RESULT_OK) {
        retStatus = STATUS_SUCCESS;
    } else if (sdpResult == SDP_RESULT_OUT_OF_MEMORY) {
        retStatus = STATUS_BUFFER_TOO_SMALL;
    } else {
        retStatus = sdpResult;
    }

CleanUp:

    LEAVES();
    return retStatus;
}

static STATUS serializeAttribute(SdpSerializerContext_t* pCtx, PSdpAttributes pSDPAttributes)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    SdpResult_t sdpResult = SDP_RESULT_OK;
    SdpAttribute_t attribute;

    CHK(pSDPAttributes != NULL, STATUS_NULL_ARG);

    attribute.pAttributeName = pSDPAttributes->attributeName;
    attribute.attributeNameLength = STRNLEN(pSDPAttributes->attributeName, MAX_SDP_ATTRIBUTE_NAME_LENGTH);

    if (pSDPAttributes->attributeValue[0] == '\0') {
        attribute.pAttributeValue = NULL;
        attribute.attributeValueLength = 0;
    } else {
        attribute.pAttributeValue = pSDPAttributes->attributeValue;
        attribute.attributeValueLength = STRNLEN(pSDPAttributes->attributeValue, MAX_SDP_ATTRIBUTE_VALUE_LENGTH);
    }

    sdpResult = SdpSerializer_AddAttribute(pCtx, SDP_TYPE_ATTRIBUTE, &attribute);

    if (sdpResult == SDP_RESULT_OK) {
        retStatus = STATUS_SUCCESS;
    } else if (sdpResult == SDP_RESULT_OUT_OF_MEMORY) {
        retStatus = STATUS_BUFFER_TOO_SMALL;
    } else {
        retStatus = STATUS_INTERNAL_ERROR;
    }

CleanUp:

    LEAVES();
    return retStatus;
}

static STATUS serializeMediaName(SdpSerializerContext_t* pCtx, PCHAR pMediaName, UINT32 mediaNameSize)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    SdpResult_t sdpResult = SDP_RESULT_OK;

    CHK(pMediaName != NULL, STATUS_NULL_ARG);

    sdpResult = SdpSerializer_AddBuffer(pCtx, SDP_TYPE_MEDIA, pMediaName, STRNLEN(pMediaName, mediaNameSize));

    if (sdpResult == SDP_RESULT_OK) {
        retStatus = STATUS_SUCCESS;
    } else if (sdpResult == SDP_RESULT_OUT_OF_MEMORY) {
        retStatus = STATUS_BUFFER_TOO_SMALL;
    } else {
        retStatus = STATUS_INTERNAL_ERROR;
    }

CleanUp:

    LEAVES();
    return retStatus;
}

static STATUS serializeMediaConnectionInformation(SdpSerializerContext_t* pCtx, PSdpConnectionInformation pSdpConnectionInformation)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    SdpResult_t sdpResult = SDP_RESULT_OK;
    SdpConnectionInfo_t connInfo;

    CHK(pSdpConnectionInformation != NULL, STATUS_NULL_ARG);

    if (pSdpConnectionInformation->networkType[0] != '\0') {
        /* Append connection info */
        CHK(STRCMP(pSdpConnectionInformation->networkType, "IN") == 0, STATUS_INVALID_ARG);
        connInfo.networkType = SDP_NETWORK_IN;

        CHK(STRCMP(pSdpConnectionInformation->addressType, "IP4") == 0 || STRCMP(pSdpConnectionInformation->addressType, "IP6") == 0,
            STATUS_INVALID_ARG);

        if (STRCMP(pSdpConnectionInformation->addressType, "IP4") == 0) {
            connInfo.addressType = SDP_ADDRESS_IPV4;
        } else {
            connInfo.addressType = SDP_ADDRESS_IPV6;
        }

        connInfo.pAddress = pSdpConnectionInformation->connectionAddress;
        connInfo.addressLength = STRNLEN(pSdpConnectionInformation->connectionAddress, MAX_SDP_CONNECTION_ADDRESS_LENGTH);

        sdpResult = SdpSerializer_AddConnectionInfo(pCtx, SDP_TYPE_CONNINFO, &connInfo);

        if (sdpResult == SDP_RESULT_OK) {
            retStatus = STATUS_SUCCESS;
        } else if (sdpResult == SDP_RESULT_OUT_OF_MEMORY) {
            retStatus = STATUS_BUFFER_TOO_SMALL;
        } else {
            retStatus = sdpResult;
        }
    }

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS serializeSessionDescription(PSessionDescription pSessionDescription, PCHAR sdpBytes, PUINT32 sdpBytesLength)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    SdpResult_t sdpResult = SDP_RESULT_OK;
    SdpSerializerContext_t ctx;
    char* pSdpMessage;
    size_t sdpMessageLength;
    UINT32 i, j;

    sdpResult = SdpSerializer_Init(&ctx, sdpBytes, *sdpBytesLength);
    CHK(sdpResult == SDP_RESULT_OK, sdpResult);

    /* Append version. */
    CHK_STATUS(serializeVersion(&ctx, pSessionDescription->version));

    /* Append originator. */
    CHK_STATUS(serializeOrigin(&ctx, &(pSessionDescription->sdpOrigin)));

    /* Append session name. */
    CHK_STATUS(serializeSessionName(&ctx, pSessionDescription->sessionName, MAX_SDP_SESSION_NAME_LENGTH));

    /* Append time description. */
    for (i = 0; i < pSessionDescription->timeDescriptionCount; i++) {
        CHK_STATUS(serializeTimeDescription(&ctx, &(pSessionDescription->sdpTimeDescription[i])));
    }

    /* Append session attributes. */
    for (i = 0; i < pSessionDescription->sessionAttributesCount; i++) {
        CHK_STATUS(serializeAttribute(&ctx, &(pSessionDescription->sdpAttributes[i])));
    }

    /* Append media. */
    for (i = 0; i < pSessionDescription->mediaCount; i++) {
        CHK_STATUS(serializeMediaName(&ctx, pSessionDescription->mediaDescriptions[i].mediaName, MAX_SDP_MEDIA_NAME_LENGTH));
        CHK_STATUS(serializeMediaConnectionInformation(&ctx, &(pSessionDescription->mediaDescriptions[i].sdpConnectionInformation)));

        /* Append media attributes. */
        for (j = 0; j < pSessionDescription->mediaDescriptions[i].mediaAttributesCount; j++) {
            CHK_STATUS(serializeAttribute(&ctx, &(pSessionDescription->mediaDescriptions[i].sdpAttributes[j])));
        }
    }

    sdpResult = SdpSerializer_Finalize(&ctx, (const char**) &pSdpMessage, &sdpMessageLength);
    CHK(sdpResult == SDP_RESULT_OK, sdpResult);

    /* Append NULL terminator. */
    if (*sdpBytesLength >= sdpMessageLength + 1U) {
        *(pSdpMessage + sdpMessageLength) = '\0';
        *sdpBytesLength = sdpMessageLength + 1U;
    } else if (sdpBytes == NULL) {
        /* If buffer is NULL, we don't really need to append null terminator.
         * Reserve space for null terminator. */
        *sdpBytesLength = sdpMessageLength + 1U;
    } else {
        retStatus = STATUS_BUFFER_TOO_SMALL;
    }

CleanUp:
    LEAVES();
    return retStatus;
}
