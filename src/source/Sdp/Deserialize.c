#define LOG_CLASS "SDP"
#include "../Include_i.h"

STATUS deserializeVersion(UINT64 version, PCHAR *ppOutputData, PUINT32 pTotalWritten, PUINT32 pBufferSize)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 currentWriteSize = 0;

    currentWriteSize = SNPRINTF(*ppOutputData,
                           (*ppOutputData) == NULL ? 0 : *pBufferSize - *pTotalWritten,
                           SDP_VERSION_MARKER"%llu\n",
                           version);

    CHK(*ppOutputData == NULL || ((*pBufferSize - *pTotalWritten) >= currentWriteSize), STATUS_BUFFER_TOO_SMALL);
    *pTotalWritten += currentWriteSize;
    if (*ppOutputData != NULL) {
        *ppOutputData += currentWriteSize;
    }

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS deserializeOrigin(PSdpOrigin pSDPOrigin, PCHAR *ppOutputData, PUINT32 pTotalWritten, PUINT32 pBufferSize)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 currentWriteSize = 0;

    CHK(pSDPOrigin != NULL, STATUS_NULL_ARG);

     if (pSDPOrigin->userName[0] != '\0' &&
         pSDPOrigin->sdpConnectionInformation.connectionAddress[0] != '\0' &&
         pSDPOrigin->sdpConnectionInformation.connectionAddress[0] != '\0' &&
         pSDPOrigin->sdpConnectionInformation.connectionAddress[0] != '\0')
     {

        currentWriteSize = SNPRINTF(*ppOutputData,
                               (*ppOutputData) == NULL ? 0 : *pBufferSize - *pTotalWritten,
                               SDP_ORIGIN_MARKER"%s %llu %llu %s %s %s\n",
                               pSDPOrigin->userName,
                               pSDPOrigin->sessionId,
                               pSDPOrigin->sessionVersion,
                               pSDPOrigin->sdpConnectionInformation.networkType,
                               pSDPOrigin->sdpConnectionInformation.addressType,
                               pSDPOrigin->sdpConnectionInformation.connectionAddress);

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

STATUS deserializeSessionName(PCHAR sessionName, PCHAR *ppOutputData, PUINT32 pTotalWritten, PUINT32 pBufferSize)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 currentWriteSize = 0;

    if (sessionName[0] != '\0') {
        currentWriteSize = SNPRINTF(*ppOutputData,
                               (*ppOutputData) == NULL ? 0 : *pBufferSize - *pTotalWritten,
                               SDP_SESSION_NAME_MARKER"%s\n",
                               sessionName);

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

STATUS deserializeTimeDescription(PSdpTimeDescription pSDPTimeDescription, PCHAR *ppOutputData, PUINT32 pTotalWritten, PUINT32 pBufferSize)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 currentWriteSize = 0;

    currentWriteSize = SNPRINTF(*ppOutputData,
                           (*ppOutputData) == NULL ? 0 : *pBufferSize - *pTotalWritten,
                           SDP_TIME_DESCRIPTION_MARKER"%llu %llu\n",
                           pSDPTimeDescription->startTime,
                           pSDPTimeDescription->stopTime);

    *pTotalWritten += currentWriteSize;
    if (*ppOutputData != NULL) {
        *ppOutputData += currentWriteSize;
    }

    LEAVES();
    return retStatus;
}

STATUS deserializeAttribute(PSdpAttributes pSDPAttributes, PCHAR *ppOutputData, PUINT32 pTotalWritten, PUINT32 pBufferSize)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 currentWriteSize = 0;

    if (pSDPAttributes->attributeValue[0] == '\0') {
        currentWriteSize = SNPRINTF(*ppOutputData,
                               (*ppOutputData) == NULL ? 0 : *pBufferSize - *pTotalWritten,
                               SDP_ATTRIBUTE_MARKER"%s\n",
                               pSDPAttributes->attributeName);
    } else {
        currentWriteSize = snprintf(*ppOutputData,
                               (*ppOutputData) == NULL ? 0 : *pBufferSize - *pTotalWritten,
                               SDP_ATTRIBUTE_MARKER"%s:%s\n",
                               pSDPAttributes->attributeName,
                               pSDPAttributes->attributeValue);
    }

    *pTotalWritten += currentWriteSize;
    if (*ppOutputData != NULL) {
        *ppOutputData += currentWriteSize;
    }

    LEAVES();
    return retStatus;
}

STATUS deserializeMediaName(PCHAR pMediaName, PCHAR *ppOutputData, PUINT32 pTotalWritten, PUINT32 pBufferSize)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 currentWriteSize = 0;

    if (pMediaName[0] != '\0') {
        currentWriteSize = snprintf(*ppOutputData,
                               (*ppOutputData) == NULL ? 0 : *pBufferSize - *pTotalWritten,
                               SDP_MEDIA_NAME_MARKER"%s\n",
                               pMediaName);

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

STATUS deserializeMediaConnectionInformation(PSdpConnectionInformation pSdpConnectionInformation, PCHAR *ppOutputData, PUINT32 pTotalWritten, PUINT32 pBufferSize)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 currentWriteSize = 0;

    if (pSdpConnectionInformation->networkType[0] != '\0') {
        currentWriteSize = SNPRINTF(*ppOutputData,
                               (*ppOutputData) == NULL ? 0 : *pBufferSize - *pTotalWritten,
                               SDP_CONNECTION_INFORMATION_MARKER"%s %s %s\n",
                               pSdpConnectionInformation->networkType,
                               pSdpConnectionInformation->addressType,
                               pSdpConnectionInformation->connectionAddress);

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


STATUS deserializeSessionDescription(PSessionDescription pSessionDescription, PCHAR sdpBytes, PUINT32 sdpBytesLength)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PCHAR curr = sdpBytes;
    UINT32 i, j, bufferSize = 0;

    CHK(pSessionDescription != NULL && sdpBytesLength != NULL, STATUS_NULL_ARG);

    bufferSize = *sdpBytesLength;
    *sdpBytesLength = 0;

    CHK_STATUS(deserializeVersion(pSessionDescription->version, &curr, sdpBytesLength, &bufferSize));
    CHK_STATUS(deserializeOrigin(&pSessionDescription->sdpOrigin, &curr, sdpBytesLength, &bufferSize));
    CHK_STATUS(deserializeSessionName(pSessionDescription->sessionName, &curr, sdpBytesLength, &bufferSize));
    for (i = 0; i < pSessionDescription->timeDescriptionCount; i++) {
        CHK_STATUS(deserializeTimeDescription(&pSessionDescription->sdpTimeDescription[i], &curr, sdpBytesLength, &bufferSize));
    }
    for (i = 0; i < pSessionDescription->sessionAttributesCount; i++) {
        CHK_STATUS(deserializeAttribute(&pSessionDescription->sdpAttributes[i], &curr, sdpBytesLength, &bufferSize));
    }

    for (i = 0; i < pSessionDescription->mediaCount; i++) {
        CHK_STATUS(deserializeMediaName(pSessionDescription->mediaDescriptions[i].mediaName, &curr, sdpBytesLength, &bufferSize));
        CHK_STATUS(deserializeMediaConnectionInformation(&(pSessionDescription->mediaDescriptions[i].sdpConnectionInformation), &curr, sdpBytesLength, &bufferSize));
        for (j = 0; j < pSessionDescription->mediaDescriptions[i].mediaAttributesCount; j++) {
            CHK_STATUS(deserializeAttribute(&pSessionDescription->mediaDescriptions[i].sdpAttributes[j], &curr, sdpBytesLength, &bufferSize));
        }
    }

    *sdpBytesLength += 1; // NULL terminator

CleanUp:
    LEAVES();
    return retStatus;
}
