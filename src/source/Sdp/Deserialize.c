#define LOG_CLASS "SDPDeserialize"
#include "../Include_i.h"
#include "kvssdp/sdp_deserializer.h"

// Convert error code from SDP library to STATUS.


STATUS parseMediaName(PSessionDescription pSessionDescription, PCHAR mediaValue, SIZE_T mediaValueLength)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    SIZE_T minMediaValueLength;

    CHK(pSessionDescription->mediaCount < MAX_SDP_SESSION_MEDIA_COUNT, STATUS_SESSION_DESCRIPTION_MAX_MEDIA_COUNT);

    minMediaValueLength = MIN(MAX_SDP_MEDIA_NAME_LENGTH, mediaValueLength);
    STRNCPY(pSessionDescription->mediaDescriptions[pSessionDescription->mediaCount].mediaName, mediaValue, minMediaValueLength);
    pSessionDescription->mediaDescriptions[pSessionDescription->mediaCount].mediaName[minMediaValueLength] = '\0';
    pSessionDescription->mediaCount++;

CleanUp:
    LEAVES();
    return retStatus;
}

STATUS parseSessionAttributes(PSessionDescription pSessionDescription, PCHAR pValue, SIZE_T valueLength)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    SdpResult_t sdpResult = SDP_RESULT_OK;
    SdpAttribute_t attribute;
    SIZE_T minAttributeLength;

    CHK(pSessionDescription->sessionAttributesCount < MAX_SDP_ATTRIBUTES_COUNT, STATUS_SDP_ATTRIBUTE_MAX_EXCEEDED);

    // Session attributes
    sdpResult = SdpDeserializer_ParseAttribute(pValue, valueLength, &attribute);
    CHK(sdpResult == SDP_RESULT_OK, convertSdpErrorCode(sdpResult));

    minAttributeLength = MIN(MAX_SDP_ATTRIBUTE_NAME_LENGTH, attribute.attributeNameLength);
    STRNCPY(pSessionDescription->sdpAttributes[pSessionDescription->sessionAttributesCount].attributeName, attribute.pAttributeName,
            minAttributeLength);
    pSessionDescription->sdpAttributes[pSessionDescription->sessionAttributesCount].attributeName[minAttributeLength] = '\0';

    if (attribute.pAttributeValue != NULL) {
        minAttributeLength = MIN(MAX_SDP_ATTRIBUTE_VALUE_LENGTH, attribute.attributeValueLength);
        STRNCPY(pSessionDescription->sdpAttributes[pSessionDescription->sessionAttributesCount].attributeValue, attribute.pAttributeValue,
                minAttributeLength);
        pSessionDescription->sdpAttributes[pSessionDescription->sessionAttributesCount].attributeValue[minAttributeLength] = '\0';
    }

    pSessionDescription->sessionAttributesCount++;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS parseMediaAttributes(PSessionDescription pSessionDescription, PCHAR pValue, SIZE_T valueLength)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    SdpResult_t sdpResult = SDP_RESULT_OK;
    SdpAttribute_t attribute;
    UINT16 currentMediaAttributesCount;
    UINT32 mediaIdx = pSessionDescription->mediaCount - 1;
    SIZE_T minAttributeNameLength;

    currentMediaAttributesCount = pSessionDescription->mediaDescriptions[mediaIdx].mediaAttributesCount;

    CHK(currentMediaAttributesCount < MAX_SDP_ATTRIBUTES_COUNT, STATUS_SDP_ATTRIBUTE_MAX_EXCEEDED);

    // Media attributes
    sdpResult = SdpDeserializer_ParseAttribute(pValue, valueLength, &attribute);
    CHK(sdpResult == SDP_RESULT_OK, convertSdpErrorCode(sdpResult));

    minAttributeNameLength = MIN(MAX_SDP_ATTRIBUTE_NAME_LENGTH, attribute.attributeNameLength);
    STRNCPY(pSessionDescription->mediaDescriptions[mediaIdx].sdpAttributes[currentMediaAttributesCount].attributeName, attribute.pAttributeName,
            minAttributeNameLength);
    pSessionDescription->mediaDescriptions[mediaIdx].sdpAttributes[currentMediaAttributesCount].attributeName[minAttributeNameLength] = '\0';

    if (attribute.pAttributeValue != NULL) {
        minAttributeNameLength = MIN(MAX_SDP_ATTRIBUTE_VALUE_LENGTH, attribute.attributeValueLength);
        STRNCPY(pSessionDescription->mediaDescriptions[mediaIdx].sdpAttributes[currentMediaAttributesCount].attributeValue, attribute.pAttributeValue,
                minAttributeNameLength);
        pSessionDescription->mediaDescriptions[mediaIdx].sdpAttributes[currentMediaAttributesCount].attributeValue[minAttributeNameLength] = '\0';
    }

    pSessionDescription->mediaDescriptions[mediaIdx].mediaAttributesCount++;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS deserializeSessionDescription(PSessionDescription pSessionDescription, PCHAR sdpBytes)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    SdpResult_t sdpResult = SDP_RESULT_OK;
    SdpDeserializerContext_t ctx;
    CHAR* pValue;
    SIZE_T valueLength;
    UINT8 type;
    SIZE_T minLength;

    CHK(sdpBytes != NULL, STATUS_SESSION_DESCRIPTION_INVALID_SESSION_DESCRIPTION);

    sdpResult = SdpDeserializer_Init(&ctx, sdpBytes, STRLEN(sdpBytes));
    CHK(sdpResult == SDP_RESULT_OK, convertSdpErrorCode(sdpResult));

    while (sdpResult == SDP_RESULT_OK) {
        sdpResult = SdpDeserializer_GetNext(&ctx, &type, (const CHAR**) &pValue, (size_t*) &valueLength);

        if (sdpResult == SDP_RESULT_OK) {
            /* Do nothing. */
        } else {
            retStatus = convertSdpErrorCode(sdpResult);
            break;
        }

        if (type == SDP_TYPE_MEDIA) {
            CHK_STATUS(parseMediaName(pSessionDescription, pValue, valueLength));
        } else if (pSessionDescription->mediaCount != 0) {
            if (type == SDP_TYPE_ATTRIBUTE) {
                CHK_STATUS(parseMediaAttributes(pSessionDescription, pValue, valueLength));
            } else if (type == SDP_TYPE_SESSION_INFO) {
                // Media Title
                minLength = MIN(MAX_SDP_MEDIA_TITLE_LENGTH, valueLength);
                STRNCPY(pSessionDescription->mediaDescriptions[pSessionDescription->mediaCount - 1].mediaTitle, pValue, minLength);
                pSessionDescription->mediaDescriptions[pSessionDescription->mediaCount - 1].mediaTitle[minLength] = '\0';
            } else {
                /* Do nothing. */
            }
        } else {
            if (type == SDP_TYPE_SESSION_NAME) {
                // SDP Session Name
                minLength = MIN(MAX_SDP_SESSION_NAME_LENGTH, valueLength);
                STRNCPY(pSessionDescription->sessionName, pValue, minLength);
                pSessionDescription->sessionName[minLength] = '\0';
            } else if (type == SDP_TYPE_SESSION_INFO) {
                // SDP Session Information
                minLength = MIN(MAX_SDP_SESSION_INFORMATION_LENGTH, valueLength);
                STRNCPY(pSessionDescription->sessionInformation, pValue, minLength);
                pSessionDescription->sessionInformation[minLength] = '\0';
            } else if (type == SDP_TYPE_URI) {
                // SDP URI
                minLength = MIN(MAX_SDP_SESSION_URI_LENGTH, valueLength);
                STRNCPY(pSessionDescription->uri, pValue, minLength);
                pSessionDescription->uri[minLength] = '\0';
            } else if (type == SDP_TYPE_EMAIL) {
                // SDP Email Address
                minLength = MIN(MAX_SDP_SESSION_EMAIL_ADDRESS_LENGTH, valueLength);
                STRNCPY(pSessionDescription->emailAddress, pValue, minLength);
                pSessionDescription->emailAddress[minLength] = '\0';
            } else if (type == SDP_TYPE_PHONE) {
                // SDP Phone number
                minLength = MIN(MAX_SDP_SESSION_PHONE_NUMBER_LENGTH, valueLength);
                STRNCPY(pSessionDescription->phoneNumber, pValue, minLength);
                pSessionDescription->phoneNumber[minLength] = '\0';
            } else if (type == SDP_TYPE_VERSION) {
                // Version
                STRTOUI64(pValue, pValue + valueLength, 10, &pSessionDescription->version);
            } else if (type == SDP_TYPE_ATTRIBUTE) {
                CHK_STATUS(parseSessionAttributes(pSessionDescription, pValue, valueLength));
            } else {
                /* Do nothing. */
            }
        }
    }

CleanUp:

    LEAVES();
    return retStatus;
}
