#define LOG_CLASS "RtcRtp"

#include "../Include_i.h"

typedef STATUS (*RtpPayloadFunc)(UINT32, PBYTE, UINT32, PBYTE, PUINT32, PUINT32, PUINT32);

STATUS createKvsRtpTransceiver(RTC_RTP_TRANSCEIVER_DIRECTION direction, PKvsPeerConnection pKvsPeerConnection, UINT32 ssrc,
                               UINT32 rtxSsrc, PRtcMediaStreamTrack pRtcMediaStreamTrack, PJitterBuffer pJitterBuffer,
                               RTC_CODEC rtcCodec, PKvsRtpTransceiver* ppKvsRtpTransceiver)
{
    STATUS retStatus = STATUS_SUCCESS;
    PKvsRtpTransceiver pKvsRtpTransceiver = NULL;

    CHK(ppKvsRtpTransceiver != NULL && pKvsPeerConnection != NULL && pRtcMediaStreamTrack != NULL,
        STATUS_NULL_ARG);

    pKvsRtpTransceiver = (PKvsRtpTransceiver) MEMCALLOC(1, SIZEOF(KvsRtpTransceiver));
    CHK(pKvsRtpTransceiver != NULL, STATUS_NOT_ENOUGH_MEMORY);

    pKvsRtpTransceiver->peerFrameBufferSize = DEFAULT_PEER_FRAME_BUFFER_SIZE;
    pKvsRtpTransceiver->peerFrameBuffer = (PBYTE) MEMALLOC(pKvsRtpTransceiver->peerFrameBufferSize);
    CHK(pKvsRtpTransceiver->peerFrameBuffer != NULL, STATUS_NOT_ENOUGH_MEMORY);
    pKvsRtpTransceiver->pKvsPeerConnection = pKvsPeerConnection;
    pKvsRtpTransceiver->sender.ssrc = ssrc;
    pKvsRtpTransceiver->sender.rtxSsrc = rtxSsrc;
    pKvsRtpTransceiver->sender.track = *pRtcMediaStreamTrack;
    pKvsRtpTransceiver->sender.packetBuffer = NULL;
    pKvsRtpTransceiver->sender.retransmitter = NULL;
    pKvsRtpTransceiver->pJitterBuffer = pJitterBuffer;
    pKvsRtpTransceiver->transceiver.receiver.track.codec = rtcCodec;
    pKvsRtpTransceiver->transceiver.direction = direction;

CleanUp:

    if (STATUS_FAILED(retStatus)) {
        freeKvsRtpTransceiver(&pKvsRtpTransceiver);
    }

    if (ppKvsRtpTransceiver != NULL) {
        *ppKvsRtpTransceiver = pKvsRtpTransceiver;
    }

    return retStatus;
}

STATUS freeKvsRtpTransceiver(PKvsRtpTransceiver* ppKvsRtpTransceiver)
{
    STATUS retStatus = STATUS_SUCCESS;
    PKvsRtpTransceiver pKvsRtpTransceiver = NULL;

    CHK(ppKvsRtpTransceiver != NULL, STATUS_NULL_ARG);
    pKvsRtpTransceiver = *ppKvsRtpTransceiver;
    // free is idempotent
    CHK(pKvsRtpTransceiver != NULL, retStatus);

    if (pKvsRtpTransceiver->pJitterBuffer != NULL) {
        freeJitterBuffer(&pKvsRtpTransceiver->pJitterBuffer);
    }

    if (pKvsRtpTransceiver->sender.packetBuffer != NULL) {
        freeRtpRollingBuffer(&pKvsRtpTransceiver->sender.packetBuffer);
    }

    if (pKvsRtpTransceiver->sender.retransmitter != NULL) {
        freeRetransmitter(&pKvsRtpTransceiver->sender.retransmitter);
    }

    SAFE_MEMFREE(pKvsRtpTransceiver->peerFrameBuffer);
    SAFE_MEMFREE(pKvsRtpTransceiver->sender.payloadArray.payloadBuffer);
    SAFE_MEMFREE(pKvsRtpTransceiver->sender.payloadArray.payloadSubLength);

    SAFE_MEMFREE(pKvsRtpTransceiver);

    *ppKvsRtpTransceiver = NULL;

CleanUp:

    return retStatus;
}

STATUS kvsRtpTransceiverSetJitterBuffer(PKvsRtpTransceiver pKvsRtpTransceiver, PJitterBuffer pJitterBuffer)
{
    STATUS retStatus = STATUS_SUCCESS;
    CHK(pKvsRtpTransceiver != NULL && pJitterBuffer != NULL, STATUS_NULL_ARG);

    pKvsRtpTransceiver->pJitterBuffer = pJitterBuffer;

CleanUp:

    return retStatus;
}

STATUS transceiverOnFrame(PRtcRtpTransceiver pRtcRtpTransceiver, UINT64 customData, RtcOnFrame rtcOnFrame) {
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PKvsRtpTransceiver pKvsRtpTransceiver = (PKvsRtpTransceiver) pRtcRtpTransceiver;

    CHK(pKvsRtpTransceiver != NULL && rtcOnFrame != NULL, STATUS_NULL_ARG);

    pKvsRtpTransceiver->onFrame = rtcOnFrame;
    pKvsRtpTransceiver->onFrameCustomData = customData;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS transceiverOnBandwidthEstimation(PRtcRtpTransceiver pRtcRtpTransceiver, UINT64 customData, RtcOnBandwidthEstimation rtcOnBandwidthEstimation) {
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PKvsRtpTransceiver pKvsRtpTransceiver = (PKvsRtpTransceiver) pRtcRtpTransceiver;

    CHK(pKvsRtpTransceiver != NULL && rtcOnBandwidthEstimation != NULL, STATUS_NULL_ARG);

    pKvsRtpTransceiver->onBandwidthEstimation = rtcOnBandwidthEstimation;
    pKvsRtpTransceiver->onBandwidthEstimationCustomData = customData;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS updateEncoderStats(PRtcRtpTransceiver pRtcRtpTransceiver, PRtcEncoderStats encoderStats)
{
    STATUS retStatus = STATUS_SUCCESS;
    PKvsRtpTransceiver pKvsRtpTransceiver = (PKvsRtpTransceiver) pRtcRtpTransceiver;
    CHK(pKvsRtpTransceiver != NULL && encoderStats != NULL, STATUS_NULL_ARG);
    ATOMIC_ADD(&pKvsRtpTransceiver->sender.outboundRtpStreamStats.totalEncodeTime, encoderStats->encodeTimeMsec);
    ATOMIC_STORE(&pKvsRtpTransceiver->sender.outboundRtpStreamStats.targetBitrate, encoderStats->targetBitrate);
    ATOMIC_STORE(&pKvsRtpTransceiver->sender.outboundRtpStreamStats.frameWidth, encoderStats->width);
    ATOMIC_STORE(&pKvsRtpTransceiver->sender.outboundRtpStreamStats.frameHeight, encoderStats->height);

CleanUp:
    CHK_LOG_ERR(retStatus);

    return retStatus;
}

STATUS writeFrame(PRtcRtpTransceiver pRtcRtpTransceiver, PFrame pFrame)
{
    STATUS retStatus = STATUS_SUCCESS;
    PKvsPeerConnection pKvsPeerConnection = NULL;
    PKvsRtpTransceiver pKvsRtpTransceiver = (PKvsRtpTransceiver) pRtcRtpTransceiver;
    BOOL locked = FALSE, bufferAfterEncrypt = FALSE;
    PRtpPacket pPacketList = NULL, pRtpPacket = NULL;
    UINT32 i = 0, packetLen = 0, headerLen = 0, allocSize;
    PBYTE rawPacket = NULL;
    PPayloadArray pPayloadArray = NULL;
    RtpPayloadFunc rtpPayloadFunc = NULL;
    UINT64 randomRtpTimeoffset = 0; // TODO: spec requires random rtp time offset
    UINT64 rtpTimestamp = 0;
    UINT64 now = GETTIME();

    CHK(pKvsRtpTransceiver != NULL, STATUS_NULL_ARG);
    pKvsPeerConnection = pKvsRtpTransceiver->pKvsPeerConnection;
    pPayloadArray = &(pKvsRtpTransceiver->sender.payloadArray);
    ATOMIC_ADD(&pKvsRtpTransceiver->sender.outboundRtpStreamStats.totalEncodedBytesTarget, pFrame->size);
    if (MEDIA_STREAM_TRACK_KIND_VIDEO == pKvsRtpTransceiver->sender.track.kind) {
        ATOMIC_INCREMENT(&pKvsRtpTransceiver->sender.outboundRtpStreamStats.framesEncoded);
        if (0 != (pFrame->flags & FRAME_FLAG_KEY_FRAME)) {
            ATOMIC_INCREMENT(&pKvsRtpTransceiver->sender.outboundRtpStreamStats.keyFramesEncoded);
        }
        if (pKvsRtpTransceiver->sender.lastKnownFrameCountTime == 0) {
            pKvsRtpTransceiver->sender.lastKnownFrameCountTime = now;
            pKvsRtpTransceiver->sender.lastKnownFrameCount     = pKvsRtpTransceiver->sender.outboundRtpStreamStats.framesEncoded;
        } else if (now - pKvsRtpTransceiver->sender.lastKnownFrameCountTime > HUNDREDS_OF_NANOS_IN_A_SECOND) {
            UINT64 frames = pKvsRtpTransceiver->sender.outboundRtpStreamStats.framesEncoded - pKvsRtpTransceiver->sender.lastKnownFrameCount;
            UINT64 time   = now - pKvsRtpTransceiver->sender.lastKnownFrameCountTime;
            DOUBLE fps    = (DOUBLE)(frames * HUNDREDS_OF_NANOS_IN_A_SECOND) / (DOUBLE) time;
            ATOMIC_STORE(&pKvsRtpTransceiver->sender.outboundRtpStreamStats.framesPerSecond, fps);
            pKvsRtpTransceiver->sender.lastKnownFrameCountTime = now;
            pKvsRtpTransceiver->sender.lastKnownFrameCount     = pKvsRtpTransceiver->sender.outboundRtpStreamStats.framesEncoded;
        }
    }

    MUTEX_LOCK(pKvsPeerConnection->pSrtpSessionLock);
    locked = TRUE;
    CHK(pKvsPeerConnection->pSrtpSession != NULL, STATUS_SUCCESS); // Discard packets till SRTP is ready

    switch (pKvsRtpTransceiver->sender.track.codec) {
        case RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE:
            rtpPayloadFunc = createPayloadForH264;
            rtpTimestamp = CONVERT_TIMESTAMP_TO_RTP(VIDEO_CLOCKRATE, pFrame->presentationTs);
            break;

        case RTC_CODEC_OPUS:
            rtpPayloadFunc = createPayloadForOpus;
            rtpTimestamp = CONVERT_TIMESTAMP_TO_RTP(OPUS_CLOCKRATE, pFrame->presentationTs);
            break;

        case RTC_CODEC_MULAW:
        case RTC_CODEC_ALAW:
            rtpPayloadFunc = createPayloadForG711;
            rtpTimestamp = CONVERT_TIMESTAMP_TO_RTP(PCM_CLOCKRATE, pFrame->presentationTs);
            break;

        case RTC_CODEC_VP8:
            rtpPayloadFunc = createPayloadForVP8;
            rtpTimestamp = CONVERT_TIMESTAMP_TO_RTP(VIDEO_CLOCKRATE, pFrame->presentationTs);
            break;

        default:
            CHK(FALSE, STATUS_NOT_IMPLEMENTED);
    }

    rtpTimestamp += randomRtpTimeoffset;

    CHK_STATUS(rtpPayloadFunc(pKvsPeerConnection->MTU, (PBYTE) pFrame->frameData, pFrame->size, NULL, &(pPayloadArray->payloadLength), NULL, &(pPayloadArray->payloadSubLenSize)));
    if (pPayloadArray->payloadLength > pPayloadArray->maxPayloadLength) {
        SAFE_MEMFREE(pPayloadArray->payloadBuffer);
        pPayloadArray->payloadBuffer = (PBYTE) MEMALLOC(pPayloadArray->payloadLength);
        pPayloadArray->maxPayloadLength = pPayloadArray->payloadLength;
    }
    if (pPayloadArray->payloadSubLenSize > pPayloadArray->maxPayloadSubLenSize) {
        SAFE_MEMFREE(pPayloadArray->payloadSubLength);
        pPayloadArray->payloadSubLength = (PUINT32) MEMALLOC(pPayloadArray->payloadSubLenSize * SIZEOF(UINT32));
        pPayloadArray->maxPayloadSubLenSize = pPayloadArray->payloadSubLenSize;
    }
    CHK_STATUS(rtpPayloadFunc(pKvsPeerConnection->MTU, (PBYTE) pFrame->frameData, pFrame->size, pPayloadArray->payloadBuffer, &(pPayloadArray->payloadLength), pPayloadArray->payloadSubLength, &(pPayloadArray->payloadSubLenSize)));
    pPacketList = (PRtpPacket) MEMALLOC(pPayloadArray->payloadSubLenSize * SIZEOF(RtpPacket));

    CHK_STATUS(constructRtpPackets(pPayloadArray, pKvsRtpTransceiver->sender.payloadType, pKvsRtpTransceiver->sender.sequenceNumber, rtpTimestamp, pKvsRtpTransceiver->sender.ssrc, pPacketList, pPayloadArray->payloadSubLenSize));
    pKvsRtpTransceiver->sender.sequenceNumber = GET_UINT16_SEQ_NUM(pKvsRtpTransceiver->sender.sequenceNumber + pPayloadArray->payloadSubLenSize);

    bufferAfterEncrypt = (pKvsRtpTransceiver->sender.payloadType == pKvsRtpTransceiver->sender.rtxPayloadType);
    for (i = 0; i < pPayloadArray->payloadSubLenSize; i++) {
        pRtpPacket = pPacketList + i;

        // Get the required size first
        CHK_STATUS(createBytesFromRtpPacket(pRtpPacket, NULL, &packetLen));

        // Account for SRTP authentication tag
        allocSize = packetLen + SRTP_AUTH_TAG_OVERHEAD;
        CHK(NULL != (rawPacket = (PBYTE) MEMALLOC(allocSize)), STATUS_NOT_ENOUGH_MEMORY);
        CHK_STATUS(createBytesFromRtpPacket(pRtpPacket, rawPacket, &packetLen));

        if (!bufferAfterEncrypt) {
            pRtpPacket->pRawPacket = rawPacket;
            pRtpPacket->rawPacketLength = packetLen;
            CHK_STATUS(rtpRollingBufferAddRtpPacket(pKvsRtpTransceiver->sender.packetBuffer, pRtpPacket));
        }

        CHK_STATUS(encryptRtpPacket(pKvsPeerConnection->pSrtpSession, rawPacket, (PINT32) &packetLen));
        CHK_STATUS(iceAgentSendPacket(pKvsPeerConnection->pIceAgent, rawPacket, packetLen));

        if (bufferAfterEncrypt) {
            pRtpPacket->pRawPacket = rawPacket;
            pRtpPacket->rawPacketLength = packetLen;
            CHK_STATUS(rtpRollingBufferAddRtpPacket(pKvsRtpTransceiver->sender.packetBuffer, pRtpPacket));
        }

        // https://tools.ietf.org/html/rfc3550#section-6.4.1
        // The total number of payload octets (i.e., not including header or padding) transmitted in RTP data packets by the sender
        headerLen = RTP_HEADER_LEN(pRtpPacket);
        ATOMIC_ADD(&pKvsRtpTransceiver->sender.outboundRtpStreamStats.sentRtpStreamStats.bytesSent, packetLen - headerLen);
        ATOMIC_INCREMENT(&pKvsRtpTransceiver->sender.outboundRtpStreamStats.sentRtpStreamStats.packetsSent);
        ATOMIC_STORE(&pKvsRtpTransceiver->sender.outboundRtpStreamStats.lastPacketSentTimestamp,
                     KVS_CONVERT_TIMESCALE(GETTIME(), HUNDREDS_OF_NANOS_IN_A_SECOND, 1000));
        ATOMIC_STORE(&pKvsRtpTransceiver->sender.outboundRtpStreamStats.headerBytesSent, headerLen);
        // TODO packetsDiscardedOnSend, bytesDiscardedOnSend, framesDiscardedOnSend - socket errors, i.e. a socket error occured when handing the
        // packets to the socket. This might happen due to various reasons, including full buffer or no available memory. iceAgentSendPacket return
        // value?

        SAFE_MEMFREE(rawPacket);
    }

    if (MEDIA_STREAM_TRACK_KIND_VIDEO == pKvsRtpTransceiver->sender.track.kind) {
        ATOMIC_INCREMENT(&pKvsRtpTransceiver->sender.outboundRtpStreamStats.framesSent);
        if (pKvsRtpTransceiver->sender.outboundRtpStreamStats.framesPerSecond > 0.0) {
            DOUBLE avgFrameSize =
                pKvsRtpTransceiver->sender.outboundRtpStreamStats.targetBitrate / pKvsRtpTransceiver->sender.outboundRtpStreamStats.framesPerSecond;
            if (pFrame->size >= avgFrameSize * HUGE_FRAME_MULTIPLIER) {
                ATOMIC_INCREMENT(&pKvsRtpTransceiver->sender.outboundRtpStreamStats.hugeFramesSent);
            }
        }
    }

    if (pKvsRtpTransceiver->sender.firstFrameWallClockTime == 0) {
        pKvsRtpTransceiver->sender.rtpTimeOffset = randomRtpTimeoffset;
        pKvsRtpTransceiver->sender.firstFrameWallClockTime = now;
    }

CleanUp:
    if (locked) {
        MUTEX_UNLOCK(pKvsPeerConnection->pSrtpSessionLock);
    }

    SAFE_MEMFREE(rawPacket);
    SAFE_MEMFREE(pPacketList);

    CHK_LOG_ERR(retStatus);

    return retStatus;
}

STATUS writeRtpPacket(PKvsPeerConnection pKvsPeerConnection, PRtpPacket pRtpPacket) {
    STATUS retStatus = STATUS_SUCCESS;
    BOOL locked = FALSE;
    PBYTE pRawPacket = NULL;
    INT32 rawLen = 0;

    CHK(pKvsPeerConnection != NULL && pRtpPacket != NULL && pRtpPacket->pRawPacket != NULL, STATUS_NULL_ARG);

    MUTEX_LOCK(pKvsPeerConnection->pSrtpSessionLock);
    locked = TRUE;
    CHK(pKvsPeerConnection->pSrtpSession != NULL, STATUS_SUCCESS); // Discard packets till SRTP is ready
    pRawPacket = MEMALLOC(pRtpPacket->rawPacketLength + SRTP_AUTH_TAG_OVERHEAD); // For SRTP authentication tag
    rawLen = pRtpPacket->rawPacketLength;
    MEMCPY(pRawPacket, pRtpPacket->pRawPacket, pRtpPacket->rawPacketLength);
    CHK_STATUS(encryptRtpPacket(pKvsPeerConnection->pSrtpSession, pRawPacket, &rawLen));
    CHK_STATUS(iceAgentSendPacket(pKvsPeerConnection->pIceAgent, pRawPacket, rawLen));

CleanUp:
    if (locked) {
        MUTEX_UNLOCK(pKvsPeerConnection->pSrtpSessionLock);
    }
    SAFE_MEMFREE(pRawPacket);

    return retStatus;
}
