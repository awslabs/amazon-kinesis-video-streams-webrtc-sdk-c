#define LOG_CLASS "SDP"
#include "../Include_i.h"

STATUS serializeVersion(UINT64 version, PCHAR* ppOutputData, PUINT32 pTotalWritten, PUINT32 pBufferSize)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 currentWriteSize = 0;

    currentWriteSize = SNPRINTF(*ppOutputData, (*ppOutputData) == NULL ? 0 : *pBufferSize - *pTotalWritten,
                                SDP_VERSION_MARKER "%" PRIu64 SDP_LINE_SEPARATOR, version);

    CHK(*ppOutputData == NULL || ((*pBufferSize - *pTotalWritten) >= currentWriteSize), STATUS_BUFFER_TOO_SMALL);
    *pTotalWritten += currentWriteSize;
    if (*ppOutputData != NULL) {
        *ppOutputData += currentWriteSize;
    }

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS serializeOrigin(PSdpOrigin pSDPOrigin, PCHAR* ppOutputData, PUINT32 pTotalWritten, PUINT32 pBufferSize)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 currentWriteSize = 0;

    CHK(pSDPOrigin != NULL, STATUS_NULL_ARG);

    if (pSDPOrigin->userName[0] != '\0' && pSDPOrigin->sdpConnectionInformation.networkType[0] != '\0' &&
        pSDPOrigin->sdpConnectionInformation.addressType[0] != '\0' && pSDPOrigin->sdpConnectionInformation.connectionAddress[0] != '\0') {
        currentWriteSize = SNPRINTF(*ppOutputData, (*ppOutputData) == NULL ? 0 : *pBufferSize - *pTotalWritten,
                                    SDP_ORIGIN_MARKER "%s %" PRIu64 " %" PRIu64 " %s %s %s" SDP_LINE_SEPARATOR, pSDPOrigin->userName,
                                    pSDPOrigin->sessionId, pSDPOrigin->sessionVersion, pSDPOrigin->sdpConnectionInformation.networkType,
                                    pSDPOrigin->sdpConnectionInformation.addressType, pSDPOrigin->sdpConnectionInformation.connectionAddress);

        CHK(*ppOutputData == NULL || ((*pBufferSize - *pTotalWritten) >= currentWriteSize), STATUS_BUFFER_TOO_SMALL);
        *pTotalWritten += currentWriteSize;
        if (*ppOutputData != NULL) {
            *ppOutputData += currentWriteSize;
        }
    }

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS serializeSessionName(PCHAR sessionName, PCHAR* ppOutputData, PUINT32 pTotalWritten, PUINT32 pBufferSize)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 currentWriteSize = 0;

    if (sessionName[0] != '\0') {
        currentWriteSize = SNPRINTF(*ppOutputData, (*ppOutputData) == NULL ? 0 : *pBufferSize - *pTotalWritten,
                                    SDP_SESSION_NAME_MARKER "%s" SDP_LINE_SEPARATOR, sessionName);

        CHK(*ppOutputData == NULL || ((*pBufferSize - *pTotalWritten) >= currentWriteSize), STATUS_BUFFER_TOO_SMALL);
        *pTotalWritten += currentWriteSize;
        if (*ppOutputData != NULL) {
            *ppOutputData += currentWriteSize;
        }
    }

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS serializeTimeDescription(PSdpTimeDescription pSDPTimeDescription, PCHAR* ppOutputData, PUINT32 pTotalWritten, PUINT32 pBufferSize)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 currentWriteSize = 0;

    currentWriteSize = SNPRINTF(*ppOutputData, (*ppOutputData) == NULL ? 0 : *pBufferSize - *pTotalWritten,
                                SDP_TIME_DESCRIPTION_MARKER "%" PRIu64 " %" PRIu64 SDP_LINE_SEPARATOR, pSDPTimeDescription->startTime,
                                pSDPTimeDescription->stopTime);

    *pTotalWritten += currentWriteSize;
    if (*ppOutputData != NULL) {
        *ppOutputData += currentWriteSize;
    }

    LEAVES();
    return retStatus;
}

STATUS serializeAttribute(PSdpAttributes pSDPAttributes, PCHAR* ppOutputData, PUINT32 pTotalWritten, PUINT32 pBufferSize)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 currentWriteSize = 0;

    if (pSDPAttributes->attributeValue[0] == '\0') {
        currentWriteSize = SNPRINTF(*ppOutputData, (*ppOutputData) == NULL ? 0 : *pBufferSize - *pTotalWritten,
                                    SDP_ATTRIBUTE_MARKER "%s" SDP_LINE_SEPARATOR, pSDPAttributes->attributeName);
    } else {
        currentWriteSize = snprintf(*ppOutputData, (*ppOutputData) == NULL ? 0 : *pBufferSize - *pTotalWritten,
                                    SDP_ATTRIBUTE_MARKER "%s:%s" SDP_LINE_SEPARATOR, pSDPAttributes->attributeName, pSDPAttributes->attributeValue);
    }

    *pTotalWritten += currentWriteSize;
    if (*ppOutputData != NULL) {
        *ppOutputData += currentWriteSize;
    }

    LEAVES();
    return retStatus;
}

STATUS serializeMediaName(PCHAR pMediaName, PCHAR* ppOutputData, PUINT32 pTotalWritten, PUINT32 pBufferSize)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 currentWriteSize = 0;

    if (pMediaName[0] != '\0') {
        currentWriteSize = snprintf(*ppOutputData, (*ppOutputData) == NULL ? 0 : *pBufferSize - *pTotalWritten,
                                    SDP_MEDIA_NAME_MARKER "%s" SDP_LINE_SEPARATOR, pMediaName);

        CHK(*ppOutputData == NULL || ((*pBufferSize - *pTotalWritten) >= currentWriteSize), STATUS_BUFFER_TOO_SMALL);
        *pTotalWritten += currentWriteSize;
        if (*ppOutputData != NULL) {
            *ppOutputData += currentWriteSize;
        }
    }

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS serializeMediaConnectionInformation(PSdpConnectionInformation pSdpConnectionInformation, PCHAR* ppOutputData, PUINT32 pTotalWritten,
                                           PUINT32 pBufferSize)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 currentWriteSize = 0;

    if (pSdpConnectionInformation->networkType[0] != '\0') {
        currentWriteSize = SNPRINTF(*ppOutputData, (*ppOutputData) == NULL ? 0 : *pBufferSize - *pTotalWritten,
                                    SDP_CONNECTION_INFORMATION_MARKER "%s %s %s" SDP_LINE_SEPARATOR, pSdpConnectionInformation->networkType,
                                    pSdpConnectionInformation->addressType, pSdpConnectionInformation->connectionAddress);

        CHK(*ppOutputData == NULL || ((*pBufferSize - *pTotalWritten) >= currentWriteSize), STATUS_BUFFER_TOO_SMALL);
        *pTotalWritten += currentWriteSize;
        if (*ppOutputData != NULL) {
            *ppOutputData += currentWriteSize;
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
    PCHAR curr = sdpBytes;
    UINT32 i, j, bufferSize = 0;

    CHK(pSessionDescription != NULL && sdpBytesLength != NULL, STATUS_NULL_ARG);

    bufferSize = *sdpBytesLength;
    *sdpBytesLength = 0;

    CHK_STATUS(serializeVersion(pSessionDescription->version, &curr, sdpBytesLength, &bufferSize));
    CHK_STATUS(serializeOrigin(&pSessionDescription->sdpOrigin, &curr, sdpBytesLength, &bufferSize));
    CHK_STATUS(serializeSessionName(pSessionDescription->sessionName, &curr, sdpBytesLength, &bufferSize));
    for (i = 0; i < pSessionDescription->timeDescriptionCount; i++) {
        CHK_STATUS(serializeTimeDescription(&pSessionDescription->sdpTimeDescription[i], &curr, sdpBytesLength, &bufferSize));
    }
    for (i = 0; i < pSessionDescription->sessionAttributesCount; i++) {
        CHK_STATUS(serializeAttribute(&pSessionDescription->sdpAttributes[i], &curr, sdpBytesLength, &bufferSize));
    }

    for (i = 0; i < pSessionDescription->mediaCount; i++) {
        CHK_STATUS(serializeMediaName(pSessionDescription->mediaDescriptions[i].mediaName, &curr, sdpBytesLength, &bufferSize));
        CHK_STATUS(serializeMediaConnectionInformation(&(pSessionDescription->mediaDescriptions[i].sdpConnectionInformation), &curr, sdpBytesLength,
                                                       &bufferSize));
        for (j = 0; j < pSessionDescription->mediaDescriptions[i].mediaAttributesCount; j++) {
            CHK_STATUS(serializeAttribute(&pSessionDescription->mediaDescriptions[i].sdpAttributes[j], &curr, sdpBytesLength, &bufferSize));
        }
    }

    *sdpBytesLength += 1; // NULL terminator

CleanUp:
    LEAVES();
    return retStatus;
}
