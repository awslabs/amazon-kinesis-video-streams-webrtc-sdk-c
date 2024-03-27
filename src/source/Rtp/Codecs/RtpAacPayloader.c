#define LOG_CLASS "RtpAacPayloader"

#include "../../Include_i.h"

STATUS createPayloadForAac(UINT32 mtu, PBYTE aacFrame, UINT32 aacFrameLength, PBYTE payloadBuffer, PUINT32 pPayloadLength, PUINT32 pPayloadSubLength,
                           PUINT32 pPayloadSubLenSize)
{
    UNUSED_PARAM(mtu);
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 payloadLength = 0;
    UINT32 payloadSubLenSize = 0;
    BOOL sizeCalculationOnly = (payloadBuffer == NULL);

    CHK(aacFrame != NULL && pPayloadSubLenSize != NULL && pPayloadLength != NULL && (sizeCalculationOnly || pPayloadSubLength != NULL),
        STATUS_NULL_ARG);

    payloadLength = aacFrameLength;
    payloadSubLenSize = 1;

    // Only return size if given buffer is NULL
    CHK(!sizeCalculationOnly, retStatus);
    CHK(payloadLength <= *pPayloadLength && payloadSubLenSize <= *pPayloadSubLenSize, STATUS_BUFFER_TOO_SMALL);

    MEMCPY(payloadBuffer, aacFrame, aacFrameLength);
    pPayloadSubLength[0] = aacFrameLength;

CleanUp:
    if (STATUS_FAILED(retStatus) && sizeCalculationOnly) {
        payloadLength = 0;
        payloadSubLenSize = 0;
    }

    if (pPayloadSubLenSize != NULL && pPayloadLength != NULL) {
        *pPayloadLength = payloadLength;
        *pPayloadSubLenSize = payloadSubLenSize;
    }

    LEAVES();
    return retStatus;
}

STATUS depayAacFromRtpPayload(PBYTE pRawPacket, UINT32 packetLength, PBYTE pAacData, PUINT32 pAacLength, PBOOL pIsStart)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 aacLength = 0;
    BOOL sizeCalculationOnly = (pAacData == NULL);

    CHK(pRawPacket != NULL && pAacLength != NULL, STATUS_NULL_ARG);
    CHK(packetLength > 0, retStatus);

    aacLength = packetLength;

    CHK(!sizeCalculationOnly, retStatus);
    CHK(aacLength <= *pAacLength, STATUS_BUFFER_TOO_SMALL);

    MEMCPY(pAacData, pRawPacket, aacLength);

CleanUp:
    if (STATUS_FAILED(retStatus) && sizeCalculationOnly) {
        aacLength = 0;
    }

    if (pAacLength != NULL) {
        *pAacLength = aacLength;
    }

    if (pIsStart != NULL) {
        *pIsStart = TRUE;
    }

    LEAVES();
    return retStatus;
}
