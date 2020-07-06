#define LOG_CLASS "SDP"
#include "../Include_i.h"

STATUS parseMediaName(PSessionDescription pSessionDescription, PCHAR pch, UINT32 lineLen)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pSessionDescription->mediaCount < MAX_SDP_SESSION_MEDIA_COUNT, STATUS_BUFFER_TOO_SMALL);

    STRNCPY(pSessionDescription->mediaDescriptions[pSessionDescription->mediaCount].mediaName,
            (pch + SDP_ATTRIBUTE_LENGTH),
            MIN(MAX_SDP_MEDIA_NAME_LENGTH, lineLen - SDP_ATTRIBUTE_LENGTH));
    pSessionDescription->mediaCount++;

CleanUp:
    LEAVES();
    return retStatus;
}

STATUS parseSessionAttributes(PSessionDescription pSessionDescription, PCHAR pch, UINT32 lineLen)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PCHAR search;

    CHK(pSessionDescription->sessionAttributesCount < MAX_SDP_ATTRIBUTES_COUNT, STATUS_SDP_ATTRIBUTE_MAX_EXCEEDED);

    if ((search = STRNCHR(pch, lineLen, ':')) == NULL) {
        STRNCPY(pSessionDescription->sdpAttributes[pSessionDescription->sessionAttributesCount].attributeName,
                pch + SDP_ATTRIBUTE_LENGTH,
                MIN(MAX_SDP_ATTRIBUTE_NAME_LENGTH, lineLen - SDP_ATTRIBUTE_LENGTH));
    } else {
        STRNCPY(pSessionDescription->sdpAttributes[pSessionDescription->sessionAttributesCount].attributeName,
                pch + SDP_ATTRIBUTE_LENGTH,
                (search - (pch + SDP_ATTRIBUTE_LENGTH)));
        STRNCPY(pSessionDescription->sdpAttributes[pSessionDescription->sessionAttributesCount].attributeValue,
                search + 1,
                MIN(MAX_SDP_ATTRIBUTE_VALUE_LENGTH, lineLen - (search - pch + 1)));
    }

    pSessionDescription->sessionAttributesCount++;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS parseMediaAttributes(PSessionDescription pSessionDescription, PCHAR pch, UINT32 lineLen)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PCHAR search;
    UINT8 currentMediaAttributesCount;

    currentMediaAttributesCount =
        pSessionDescription->mediaDescriptions[pSessionDescription->mediaCount - 1].mediaAttributesCount;

    CHK(currentMediaAttributesCount < MAX_SDP_ATTRIBUTES_COUNT, STATUS_SDP_ATTRIBUTE_MAX_EXCEEDED);

    if ((search = STRNCHR(pch, lineLen, ':')) == NULL) {
        STRNCPY(pSessionDescription->mediaDescriptions[pSessionDescription->mediaCount - 1].
                sdpAttributes[currentMediaAttributesCount].attributeName, pch + SDP_ATTRIBUTE_LENGTH,
                MIN(MAX_SDP_ATTRIBUTE_NAME_LENGTH, lineLen - SDP_ATTRIBUTE_LENGTH));
    } else {
        STRNCPY(pSessionDescription->mediaDescriptions[pSessionDescription->mediaCount - 1].sdpAttributes[currentMediaAttributesCount].attributeName,
                pch + SDP_ATTRIBUTE_LENGTH,
                (search - (pch + SDP_ATTRIBUTE_LENGTH)));
        STRNCPY(pSessionDescription->mediaDescriptions[pSessionDescription->mediaCount - 1].sdpAttributes[currentMediaAttributesCount].attributeValue,
                search + 1,
                MIN(MAX_SDP_ATTRIBUTE_VALUE_LENGTH, lineLen - (search - pch + 1)));
    }
    pSessionDescription->mediaDescriptions[pSessionDescription->mediaCount - 1].mediaAttributesCount++;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS serializeSessionDescription(PSessionDescription pSessionDescription, PCHAR sdpBytes)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PCHAR curr, tail, next;
    UINT32 lineLen;
    CHK(sdpBytes != NULL, STATUS_SESSION_DESCRIPTION_INVALID_SESSION_DESCRIPTION);

    curr = sdpBytes;
    tail = sdpBytes + STRLEN(sdpBytes);

    while ((next = STRNCHR(curr, tail - curr, '\n')) != NULL) {
        lineLen = (UINT32) (next - curr);

        if (0 == STRNCMP(curr, SDP_MEDIA_NAME_MARKER, (ARRAY_SIZE(SDP_MEDIA_NAME_MARKER) - 1))) {
            CHK_STATUS(parseMediaName(pSessionDescription, curr, lineLen));
        }

        if (pSessionDescription->mediaCount != 0) {
            if (0 == STRNCMP(curr, SDP_ATTRIBUTE_MARKER, (ARRAY_SIZE(SDP_ATTRIBUTE_MARKER) - 1))) {
                CHK_STATUS(parseMediaAttributes(pSessionDescription, curr, lineLen));
            }

            //Media Title
            if (0 == STRNCMP(curr, SDP_INFORMATION_MARKER, (ARRAY_SIZE(SDP_INFORMATION_MARKER) - 1))) {
                STRNCPY(pSessionDescription->mediaDescriptions[pSessionDescription->mediaCount - 1].mediaTitle,
                        (curr + SDP_ATTRIBUTE_LENGTH),
                        MIN(MAX_SDP_MEDIA_NAME_LENGTH, lineLen - SDP_ATTRIBUTE_LENGTH));
            }
        } else {

            // SDP Session Name
            if (0 == STRNCMP(curr, SDP_SESSION_NAME_MARKER, (ARRAY_SIZE(SDP_SESSION_NAME_MARKER) - 1))) {
                STRNCPY(pSessionDescription->sessionName,
                        (curr + SDP_ATTRIBUTE_LENGTH),
                        MIN(MAX_SDP_MEDIA_NAME_LENGTH, lineLen - SDP_ATTRIBUTE_LENGTH));
            }

            // SDP Session Name
            if (0 == STRNCMP(curr, SDP_INFORMATION_MARKER, (ARRAY_SIZE(SDP_INFORMATION_MARKER) - 1))) {
                STRNCPY(pSessionDescription->sessionInformation,
                        (curr + SDP_ATTRIBUTE_LENGTH),
                        MIN(MAX_SDP_MEDIA_NAME_LENGTH, lineLen - SDP_ATTRIBUTE_LENGTH));
            }

            // SDP URI
            if (0 == STRNCMP(curr, SDP_URI_MARKER, (ARRAY_SIZE(SDP_URI_MARKER) - 1))) {
                STRNCPY(pSessionDescription->uri,
                        (curr + SDP_ATTRIBUTE_LENGTH),
                        MIN(MAX_SDP_MEDIA_NAME_LENGTH, lineLen - SDP_ATTRIBUTE_LENGTH));
            }

            // SDP Email Address
            if (0 == STRNCMP(curr, SDP_EMAIL_ADDRESS_MARKER, (ARRAY_SIZE(SDP_EMAIL_ADDRESS_MARKER) - 1))) {
                STRNCPY(pSessionDescription->emailAddress,
                        (curr + SDP_ATTRIBUTE_LENGTH),
                        MIN(MAX_SDP_MEDIA_NAME_LENGTH, lineLen - SDP_ATTRIBUTE_LENGTH));
            }

            // SDP Phone number
            if (0 == STRNCMP(curr, SDP_PHONE_NUMBER_MARKER, (ARRAY_SIZE(SDP_PHONE_NUMBER_MARKER) - 1))) {
                STRNCPY(pSessionDescription->phoneNumber,
                        (curr + SDP_ATTRIBUTE_LENGTH),
                        MIN(MAX_SDP_MEDIA_NAME_LENGTH, lineLen - SDP_ATTRIBUTE_LENGTH));
            }

            if (0 == STRNCMP(curr, SDP_VERSION_MARKER, (ARRAY_SIZE(SDP_VERSION_MARKER) - 1))) {
                STRTOUI64(curr, curr + MAX_SDP_TOKEN_LENGTH, 10, &pSessionDescription->version);
            }

            if (0 == STRNCMP(curr, SDP_ATTRIBUTE_MARKER, (ARRAY_SIZE(SDP_ATTRIBUTE_MARKER) - 1))) {
                CHK_STATUS(parseSessionAttributes(pSessionDescription, curr, lineLen));
            }
        }

        curr = next + 1;
    }

CleanUp:

    LEAVES();
    return retStatus;
}
