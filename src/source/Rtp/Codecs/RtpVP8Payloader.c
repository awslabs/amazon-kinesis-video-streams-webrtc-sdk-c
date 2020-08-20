#define LOG_CLASS "RtpVP8Payloader"

#include "../../Include_i.h"

STATUS createPayloadForVP8(UINT32 mtu, PBYTE pData, UINT32 dataLen, PBYTE payloadBuffer, PUINT32 pPayloadLength, PUINT32 pPayloadSubLength,
                           PUINT32 pPayloadSubLenSize)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    BOOL sizeCalculationOnly = (payloadBuffer == NULL);
    PayloadArray payloadArray;
    UINT32 payloadRemaining = dataLen, payloadLenConsumed = 0;
    PBYTE currentData = pData;

    CHK(pData != NULL && pPayloadSubLenSize != NULL && pPayloadLength != NULL && (sizeCalculationOnly || pPayloadSubLength != NULL), STATUS_NULL_ARG);

    MEMSET(&payloadArray, 0, SIZEOF(payloadArray));
    payloadArray.payloadBuffer = payloadBuffer;

    while (payloadRemaining > 0) {
        payloadLenConsumed = MIN(mtu - VP8_PAYLOAD_DESCRIPTOR_SIZE, payloadRemaining);
        payloadArray.payloadLength += (payloadLenConsumed + VP8_PAYLOAD_DESCRIPTOR_SIZE);

        if (!sizeCalculationOnly) {
            *payloadArray.payloadBuffer = payloadArray.payloadSubLenSize == 0 ? VP8_PAYLOAD_DESCRIPTOR_START_OF_PARTITION_VALUE : 0;
            payloadArray.payloadBuffer++;

            MEMCPY(payloadArray.payloadBuffer, currentData, payloadLenConsumed);

            pPayloadSubLength[payloadArray.payloadSubLenSize] = (payloadLenConsumed + VP8_PAYLOAD_DESCRIPTOR_SIZE);
            payloadArray.payloadBuffer += payloadLenConsumed;
            currentData += payloadLenConsumed;
        }

        payloadArray.payloadSubLenSize++;
        payloadRemaining -= payloadLenConsumed;
    }

CleanUp:
    if (STATUS_FAILED(retStatus) && sizeCalculationOnly) {
        payloadArray.payloadLength = 0;
        payloadArray.payloadSubLenSize = 0;
    }

    if (pPayloadSubLenSize != NULL && pPayloadLength != NULL) {
        *pPayloadLength = payloadArray.payloadLength;
        *pPayloadSubLenSize = payloadArray.payloadSubLenSize;
    }

    LEAVES();
    return retStatus;
}

STATUS depayVP8FromRtpPayload(PBYTE pRawPacket, UINT32 packetLength, PBYTE pVp8Data, PUINT32 pVp8Length, PBOOL pIsStart)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 vp8Length = packetLength, payloadDescriptorLength = 0;
    BOOL sizeCalculationOnly = (pVp8Data == NULL);
    BOOL haveExtendedControlBits = FALSE;
    BOOL havePictureID = FALSE;
    BOOL haveTL0PICIDX = FALSE;
    BOOL haveTID = FALSE;
    BOOL haveKEYIDX = FALSE;

    CHK(pRawPacket != NULL && pVp8Length != NULL, STATUS_NULL_ARG);
    CHK(packetLength > 0, retStatus);

    haveExtendedControlBits = (pRawPacket[payloadDescriptorLength] & 0x80) >> 7;
    payloadDescriptorLength++;

    if (haveExtendedControlBits) {
        havePictureID = (pRawPacket[payloadDescriptorLength] & 0x80) >> 7;
        haveTL0PICIDX = (pRawPacket[payloadDescriptorLength] & 0x40) >> 6;
        haveTID = (pRawPacket[payloadDescriptorLength] & 0x20) >> 5;
        haveKEYIDX = (pRawPacket[payloadDescriptorLength] & 0x10) >> 4;
        payloadDescriptorLength++;
    }

    if (havePictureID) {
        if ((pRawPacket[payloadDescriptorLength] & 0x80) > 0) { // PID is 16bit
            payloadDescriptorLength += 2;
        } else {
            payloadDescriptorLength++;
        }
    }

    if (haveTL0PICIDX == 1) {
        payloadDescriptorLength++;
    }

    if (haveTID || haveKEYIDX == 1) {
        payloadDescriptorLength++;
    }

    vp8Length -= payloadDescriptorLength;
    CHK(!sizeCalculationOnly, retStatus);

    CHK(vp8Length <= *pVp8Length, STATUS_BUFFER_TOO_SMALL);
    MEMCPY(pVp8Data, pRawPacket + payloadDescriptorLength, vp8Length);

CleanUp:
    if (STATUS_FAILED(retStatus) && sizeCalculationOnly) {
        vp8Length = 0;
    }

    if (pVp8Length != NULL) {
        *pVp8Length = vp8Length;
    }

    if (pIsStart != NULL) {
        *pIsStart = TRUE;
    }

    LEAVES();
    return retStatus;
}
