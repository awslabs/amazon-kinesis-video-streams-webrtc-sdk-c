#define LOG_CLASS "SDPDeserialize"
#include "../Include_i.h"
#include "sdp_deserializer.h"

STATUS parseMediaName(PSessionDescription pSessionDescription, PCHAR mediaValue, UINT32 mediaValueLength)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pSessionDescription->mediaCount < MAX_SDP_SESSION_MEDIA_COUNT, STATUS_BUFFER_TOO_SMALL);

    STRNCPY(pSessionDescription->mediaDescriptions[pSessionDescription->mediaCount].mediaName, mediaValue,
            MIN(MAX_SDP_MEDIA_NAME_LENGTH, mediaValueLength));
    pSessionDescription->mediaCount++;

CleanUp:
    LEAVES();
    return retStatus;
}

STATUS parseSessionAttributes(PSessionDescription pSessionDescription, PCHAR pValue, UINT32 valueLength)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    SdpResult_t sdpResult = SDP_RESULT_OK;
    SdpAttribute_t attribute;

    CHK(pSessionDescription->sessionAttributesCount < MAX_SDP_ATTRIBUTES_COUNT, STATUS_SDP_ATTRIBUTE_MAX_EXCEEDED);

    // Session attributes
    sdpResult = SdpDeserializer_ParseAttribute(pValue, valueLength, &attribute);
    CHK(sdpResult == SDP_RESULT_OK, sdpResult);

    STRNCPY(pSessionDescription->sdpAttributes[pSessionDescription->sessionAttributesCount].attributeName, attribute.pAttributeName,
            MIN(MAX_SDP_ATTRIBUTE_NAME_LENGTH, attribute.attributeNameLength));

    if (attribute.pAttributeValue != NULL) {
        STRNCPY(pSessionDescription->sdpAttributes[pSessionDescription->sessionAttributesCount].attributeValue, attribute.pAttributeValue,
                MIN(MAX_SDP_ATTRIBUTE_VALUE_LENGTH, attribute.attributeValueLength));
    }

    pSessionDescription->sessionAttributesCount++;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS parseMediaAttributes(PSessionDescription pSessionDescription, PCHAR pValue, UINT32 valueLength)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    SdpResult_t sdpResult = SDP_RESULT_OK;
    SdpAttribute_t attribute;
    UINT16 currentMediaAttributesCount;
    UINT32 mediaIdx = pSessionDescription->mediaCount - 1;

    currentMediaAttributesCount = pSessionDescription->mediaDescriptions[mediaIdx].mediaAttributesCount;

    CHK(currentMediaAttributesCount < MAX_SDP_ATTRIBUTES_COUNT, STATUS_SDP_ATTRIBUTE_MAX_EXCEEDED);

    // Media attributes
    sdpResult = SdpDeserializer_ParseAttribute(pValue, valueLength, &attribute);
    CHK(sdpResult == SDP_RESULT_OK, sdpResult);

    STRNCPY(pSessionDescription->mediaDescriptions[mediaIdx].sdpAttributes[currentMediaAttributesCount].attributeName, attribute.pAttributeName,
            MIN(MAX_SDP_ATTRIBUTE_NAME_LENGTH, attribute.attributeNameLength));

    if (attribute.pAttributeValue != NULL) {
        STRNCPY(pSessionDescription->mediaDescriptions[mediaIdx].sdpAttributes[currentMediaAttributesCount].attributeValue, attribute.pAttributeValue,
                MIN(MAX_SDP_ATTRIBUTE_VALUE_LENGTH, attribute.attributeValueLength));
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
    size_t valueLength;
    UINT8 type;

    CHK(sdpBytes != NULL, STATUS_SESSION_DESCRIPTION_INVALID_SESSION_DESCRIPTION);

    sdpResult = SdpDeserializer_Init(&ctx, sdpBytes, STRLEN(sdpBytes));
    CHK(sdpResult == SDP_RESULT_OK, sdpResult);

    for (; sdpResult == SDP_RESULT_OK;) {
        sdpResult = SdpDeserializer_GetNext(&ctx, &type, (const CHAR**) &pValue, &valueLength);

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
                STRNCPY(pSessionDescription->mediaDescriptions[pSessionDescription->mediaCount - 1].mediaTitle, pValue,
                        MIN(MAX_SDP_MEDIA_TITLE_LENGTH, valueLength));
            } else {
                /* Do nothing. */
            }
        } else {
            if (type == SDP_TYPE_SESSION_NAME) {
                // SDP Session Name
                STRNCPY(pSessionDescription->sessionName, pValue, MIN(MAX_SDP_SESSION_NAME_LENGTH, valueLength));
            } else if (type == SDP_TYPE_SESSION_INFO) {
                // SDP Session Information
                STRNCPY(pSessionDescription->sessionInformation, pValue, MIN(MAX_SDP_SESSION_INFORMATION_LENGTH, valueLength));
            } else if (type == SDP_TYPE_URI) {
                // SDP URI
                STRNCPY(pSessionDescription->uri, pValue, MIN(MAX_SDP_SESSION_URI_LENGTH, valueLength));
            } else if (type == SDP_TYPE_EMAIL) {
                // SDP Email Address
                STRNCPY(pSessionDescription->emailAddress, pValue, MIN(MAX_SDP_SESSION_EMAIL_ADDRESS_LENGTH, valueLength));
            } else if (type == SDP_TYPE_PHONE) {
                // SDP Phone number
                STRNCPY(pSessionDescription->phoneNumber, pValue, MIN(MAX_SDP_SESSION_PHONE_NUMBER_LENGTH, valueLength));
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
