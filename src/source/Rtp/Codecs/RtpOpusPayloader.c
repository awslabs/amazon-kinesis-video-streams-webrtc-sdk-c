#define LOG_CLASS "RtpOpusPayloader"

#include "../../Include_i.h"

STATUS createPayloadForOpus(UINT32 mtu, PBYTE opusFrame, UINT32 opusFrameLength, PBYTE payloadBuffer, PUINT32 pPayloadLength,
                            PUINT32 pPayloadSubLength, PUINT32 pPayloadSubLenSize)
{
    UNUSED_PARAM(mtu);
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 payloadLength = 0;
    UINT32 payloadSubLenSize = 0;
    BOOL sizeCalculationOnly = (payloadBuffer == NULL);

    CHK(opusFrame != NULL && pPayloadSubLenSize != NULL && pPayloadLength != NULL && (sizeCalculationOnly || pPayloadSubLength != NULL),
        STATUS_NULL_ARG);

    payloadLength = opusFrameLength;
    payloadSubLenSize = 1;

    // Only return size if given buffer is NULL
    CHK(!sizeCalculationOnly, retStatus);
    CHK(payloadLength <= *pPayloadLength && payloadSubLenSize <= *pPayloadSubLenSize, STATUS_BUFFER_TOO_SMALL);

    MEMCPY(payloadBuffer, opusFrame, opusFrameLength);
    pPayloadSubLength[0] = opusFrameLength;

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

STATUS depayOpusFromRtpPayload(PBYTE pRawPacket, UINT32 packetLength, PBYTE pOpusData, PUINT32 pOpusLength, PBOOL pIsStart)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 opusLength = 0;
    BOOL sizeCalculationOnly = (pOpusData == NULL);

    CHK(pRawPacket != NULL && pOpusLength != NULL, STATUS_NULL_ARG);
    CHK(packetLength > 0, retStatus);

    opusLength = packetLength;

    CHK(!sizeCalculationOnly, retStatus);
    CHK(opusLength <= *pOpusLength, STATUS_BUFFER_TOO_SMALL);

    MEMCPY(pOpusData, pRawPacket, opusLength);

CleanUp:
    if (STATUS_FAILED(retStatus) && sizeCalculationOnly) {
        opusLength = 0;
    }

    if (pOpusLength != NULL) {
        *pOpusLength = opusLength;
    }

    if (pIsStart != NULL) {
        *pIsStart = TRUE;
    }

    LEAVES();
    return retStatus;
}
