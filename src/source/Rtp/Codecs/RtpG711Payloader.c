#define LOG_CLASS "RtpG711Payloader"

#include "../../Include_i.h"

STATUS createPayloadForG711(UINT32 mtu, PBYTE g711Frame, UINT32 g711FrameLength, PBYTE payloadBuffer, PUINT32 pPayloadLength,
                            PUINT32 pPayloadSubLength, PUINT32 pPayloadSubLenSize)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 payloadLength = 0;
    UINT32 payloadSubLenSize = 0;
    UINT32 remainingLength = g711FrameLength;
    PUINT32 pCurSubLen = pPayloadSubLength;
    BOOL sizeCalculationOnly = (payloadBuffer == NULL);

    CHK(g711Frame != NULL && pPayloadSubLenSize != NULL && pPayloadLength != NULL && (sizeCalculationOnly || pPayloadSubLength != NULL),
        STATUS_NULL_ARG);

    payloadLength = g711FrameLength;
    payloadSubLenSize = g711FrameLength / mtu + (g711FrameLength % mtu == 0 ? 0 : 1);

    // Only return size if given buffer is NULL
    CHK(!sizeCalculationOnly, retStatus);
    CHK(payloadLength <= *pPayloadLength && payloadSubLenSize <= *pPayloadSubLenSize, STATUS_BUFFER_TOO_SMALL);

    MEMCPY(payloadBuffer, g711Frame, g711FrameLength);
    for (remainingLength = g711FrameLength; remainingLength > mtu; remainingLength -= mtu, pCurSubLen++) {
        *pCurSubLen = mtu;
    }
    *pCurSubLen = remainingLength;

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

STATUS depayG711FromRtpPayload(PBYTE pRawPacket, UINT32 packetLength, PBYTE pG711Data, PUINT32 pG711Length, PBOOL pIsStart)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 g711Length = 0;
    BOOL sizeCalculationOnly = (pG711Data == NULL);

    CHK(pRawPacket != NULL && pG711Length != NULL, STATUS_NULL_ARG);
    CHK(packetLength > 0, retStatus);

    g711Length = packetLength;

    CHK(!sizeCalculationOnly, retStatus);
    CHK(g711Length <= *pG711Length, STATUS_BUFFER_TOO_SMALL);

    MEMCPY(pG711Data, pRawPacket, g711Length);

CleanUp:
    if (STATUS_FAILED(retStatus) && sizeCalculationOnly) {
        g711Length = 0;
    }

    if (pG711Length != NULL) {
        *pG711Length = g711Length;
    }

    if (pIsStart != NULL) {
        *pIsStart = TRUE;
    }

    LEAVES();
    return retStatus;
}
