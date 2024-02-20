#define LOG_CLASS "SDPDeserialize"
#include "../Include_i.h"
#include "kvssdp/sdp_deserializer.h"

STATUS parseMediaName(PSessionDescription pSessionDescription, PCHAR mediaValue, SIZE_T mediaValueLength)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    SIZE_T copyLength;

    CHK(pSessionDescription->mediaCount < MAX_SDP_SESSION_MEDIA_COUNT, STATUS_SESSION_DESCRIPTION_MAX_MEDIA_COUNT);

    copyLength = MIN(MAX_SDP_MEDIA_NAME_LENGTH, mediaValueLength);
    STRNCPY(pSessionDescription->mediaDescriptions[pSessionDescription->mediaCount].mediaName, mediaValue, copyLength);
    pSessionDescription->mediaDescriptions[pSessionDescription->mediaCount].mediaName[copyLength] = '\0';
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
    SIZE_T copyLength;

    CHK(pSessionDescription->sessionAttributesCount < MAX_SDP_ATTRIBUTES_COUNT, STATUS_SDP_ATTRIBUTE_MAX_EXCEEDED);

    // Session attributes
    sdpResult = SdpDeserializer_ParseAttribute(pValue, valueLength, &attribute);
    CHK(sdpResult == SDP_RESULT_OK, sdpResult);

    copyLength = MIN(MAX_SDP_ATTRIBUTE_NAME_LENGTH, attribute.attributeNameLength);
    STRNCPY(pSessionDescription->sdpAttributes[pSessionDescription->sessionAttributesCount].attributeName, attribute.pAttributeName, copyLength);
    pSessionDescription->sdpAttributes[pSessionDescription->sessionAttributesCount].attributeName[copyLength] = '\0';

    if (attribute.pAttributeValue != NULL) {
        copyLength = MIN(MAX_SDP_ATTRIBUTE_VALUE_LENGTH, attribute.attributeValueLength);
        STRNCPY(pSessionDescription->sdpAttributes[pSessionDescription->sessionAttributesCount].attributeValue, attribute.pAttributeValue,
                copyLength);
        pSessionDescription->sdpAttributes[pSessionDescription->sessionAttributesCount].attributeValue[copyLength] = '\0';
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
    SIZE_T copyLength;

    currentMediaAttributesCount = pSessionDescription->mediaDescriptions[mediaIdx].mediaAttributesCount;

    CHK(currentMediaAttributesCount < MAX_SDP_ATTRIBUTES_COUNT, STATUS_SDP_ATTRIBUTE_MAX_EXCEEDED);

    // Media attributes
    sdpResult = SdpDeserializer_ParseAttribute(pValue, valueLength, &attribute);
    CHK(sdpResult == SDP_RESULT_OK, sdpResult);

    copyLength = MIN(MAX_SDP_ATTRIBUTE_NAME_LENGTH, attribute.attributeNameLength);
    STRNCPY(pSessionDescription->mediaDescriptions[mediaIdx].sdpAttributes[currentMediaAttributesCount].attributeName, attribute.pAttributeName,
            copyLength);
    pSessionDescription->mediaDescriptions[mediaIdx].sdpAttributes[currentMediaAttributesCount].attributeName[copyLength] = '\0';

    if (attribute.pAttributeValue != NULL) {
        copyLength = MIN(MAX_SDP_ATTRIBUTE_VALUE_LENGTH, attribute.attributeValueLength);
        STRNCPY(pSessionDescription->mediaDescriptions[mediaIdx].sdpAttributes[currentMediaAttributesCount].attributeValue, attribute.pAttributeValue,
                copyLength);
        pSessionDescription->mediaDescriptions[mediaIdx].sdpAttributes[currentMediaAttributesCount].attributeValue[copyLength] = '\0';
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
    SIZE_T copyLength;

    CHK(sdpBytes != NULL, STATUS_SESSION_DESCRIPTION_INVALID_SESSION_DESCRIPTION);

    sdpResult = SdpDeserializer_Init(&ctx, sdpBytes, STRLEN(sdpBytes));
    CHK(sdpResult == SDP_RESULT_OK, sdpResult);

    for (; sdpResult == SDP_RESULT_OK;) {
        sdpResult = SdpDeserializer_GetNext(&ctx, &type, (const CHAR**) &pValue, (size_t*) &valueLength);

        if (sdpResult == SDP_RESULT_OK) {
            /* Do nothing. */
        } else if (sdpResult == SDP_RESULT_MESSAGE_END) {
            /* Reset return value when done. */
            retStatus = STATUS_SUCCESS;
            break;
        } else {
            retStatus = sdpResult;
            break;
        }

        if (type == SDP_TYPE_MEDIA) {
            CHK_STATUS(parseMediaName(pSessionDescription, pValue, valueLength));
        } else if (pSessionDescription->mediaCount != 0) {
            if (type == SDP_TYPE_ATTRIBUTE) {
                CHK_STATUS(parseMediaAttributes(pSessionDescription, pValue, valueLength));
            } else if (type == SDP_TYPE_SESSION_INFO) {
                // Media Title
                copyLength = MIN(MAX_SDP_MEDIA_TITLE_LENGTH, valueLength);
                STRNCPY(pSessionDescription->mediaDescriptions[pSessionDescription->mediaCount - 1].mediaTitle, pValue, copyLength);
                pSessionDescription->mediaDescriptions[pSessionDescription->mediaCount - 1].mediaTitle[copyLength] = '\0';
            } else {
                /* Do nothing. */
            }
        } else {
            if (type == SDP_TYPE_SESSION_NAME) {
                // SDP Session Name
                copyLength = MIN(MAX_SDP_SESSION_NAME_LENGTH, valueLength);
                STRNCPY(pSessionDescription->sessionName, pValue, copyLength);
                pSessionDescription->sessionName[copyLength] = '\0';
            } else if (type == SDP_TYPE_SESSION_INFO) {
                // SDP Session Information
                copyLength = MIN(MAX_SDP_SESSION_INFORMATION_LENGTH, valueLength);
                STRNCPY(pSessionDescription->sessionInformation, pValue, copyLength);
                pSessionDescription->sessionInformation[copyLength] = '\0';
            } else if (type == SDP_TYPE_URI) {
                // SDP URI
                copyLength = MIN(MAX_SDP_SESSION_URI_LENGTH, valueLength);
                STRNCPY(pSessionDescription->uri, pValue, copyLength);
                pSessionDescription->uri[copyLength] = '\0';
            } else if (type == SDP_TYPE_EMAIL) {
                // SDP Email Address
                copyLength = MIN(MAX_SDP_SESSION_EMAIL_ADDRESS_LENGTH, valueLength);
                STRNCPY(pSessionDescription->emailAddress, pValue, copyLength);
                pSessionDescription->emailAddress[copyLength] = '\0';
            } else if (type == SDP_TYPE_PHONE) {
                // SDP Phone number
                copyLength = MIN(MAX_SDP_SESSION_PHONE_NUMBER_LENGTH, valueLength);
                STRNCPY(pSessionDescription->phoneNumber, pValue, copyLength);
                pSessionDescription->phoneNumber[copyLength] = '\0';
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
