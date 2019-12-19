#define LOG_CLASS "SessionDescription"
#include "../Include_i.h"

STATUS serializeSessionDescriptionInit(PRtcSessionDescriptionInit pSessionDescriptionInit, PCHAR sessionDescriptionJSON, PUINT32 sessionDescriptionJSONLen)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PCHAR curr, tail, next;
    UINT32 lineLen, inputSize = 0, amountWritten;

    CHK(pSessionDescriptionInit != NULL && sessionDescriptionJSONLen != NULL, STATUS_NULL_ARG);

    inputSize = *sessionDescriptionJSONLen;
    *sessionDescriptionJSONLen = 0;

    amountWritten = SNPRINTF(
            sessionDescriptionJSON,
            sessionDescriptionJSON == NULL ? 0 : inputSize - *sessionDescriptionJSONLen,
            SESSION_DESCRIPTION_INIT_TEMPLATE_HEAD,
            pSessionDescriptionInit->type == SDP_TYPE_OFFER ? SDP_OFFER_VALUE : SDP_ANSWER_VALUE);
    CHK(sessionDescriptionJSON == NULL || ((inputSize - *sessionDescriptionJSONLen) >= amountWritten), STATUS_BUFFER_TOO_SMALL);
    *sessionDescriptionJSONLen += amountWritten;

    curr = pSessionDescriptionInit->sdp;
    tail = pSessionDescriptionInit->sdp + STRLEN(pSessionDescriptionInit->sdp);
    while ((next = STRNCHR(curr, (UINT32) (tail - curr), '\n')) != NULL) {
        lineLen = (UINT32) (next - curr);

         amountWritten = SNPRINTF(
                sessionDescriptionJSON + *sessionDescriptionJSONLen,
                sessionDescriptionJSON == NULL ? 0 : inputSize - *sessionDescriptionJSONLen,
                "%*.*s%s",
                lineLen,
                lineLen,
                curr,
                SESSION_DESCRIPTION_INIT_LINE_ENDING);
        CHK(sessionDescriptionJSON == NULL || ((inputSize - *sessionDescriptionJSONLen) >= amountWritten), STATUS_BUFFER_TOO_SMALL);

        *sessionDescriptionJSONLen += amountWritten;
        curr = next + 1;
    }

    amountWritten = SNPRINTF(
           sessionDescriptionJSON + *sessionDescriptionJSONLen,
           sessionDescriptionJSON == NULL ? 0 : inputSize - *sessionDescriptionJSONLen,
           SESSION_DESCRIPTION_INIT_TEMPLATE_TAIL);
    CHK(sessionDescriptionJSON == NULL || ((inputSize - *sessionDescriptionJSONLen) >= amountWritten), STATUS_BUFFER_TOO_SMALL);
    *sessionDescriptionJSONLen += (amountWritten + 1); // NULL terminator

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS deserializeSessionDescriptionInit(PCHAR sessionDescriptionJSON, UINT32 sessionDescriptionJSONLen, PRtcSessionDescriptionInit pSessionDescriptionInit)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    jsmntok_t tokens[MAX_JSON_TOKEN_COUNT];
    jsmn_parser parser;
    INT8 i;
    INT32 j, tokenCount, lineLen;
    PCHAR curr, last, tail;

    CHK(pSessionDescriptionInit != NULL && sessionDescriptionJSON != NULL, STATUS_NULL_ARG);
    MEMSET(pSessionDescriptionInit, 0x00, SIZEOF(RtcSessionDescriptionInit));

    jsmn_init(&parser);

    tokenCount = jsmn_parse(&parser, sessionDescriptionJSON, sessionDescriptionJSONLen, tokens, ARRAY_SIZE(tokens));
    CHK(tokenCount > 1, STATUS_INVALID_API_CALL_RETURN_JSON);
    CHK(tokens[0].type == JSMN_OBJECT, STATUS_SESSION_DESCRIPTION_INIT_NOT_OBJECT);

    for (i = 1; i < tokenCount; i += 2) {
        if (STRNCMP(SDP_TYPE_KEY, sessionDescriptionJSON + tokens[i].start, ARRAY_SIZE(SDP_TYPE_KEY) - 1) == 0) {
            if (STRNCMP(SDP_OFFER_VALUE, sessionDescriptionJSON + tokens[i+1].start, ARRAY_SIZE(SDP_OFFER_VALUE) - 1) == 0) {
                pSessionDescriptionInit->type = SDP_TYPE_OFFER;
            } else if (STRNCMP(SDP_ANSWER_VALUE, sessionDescriptionJSON + tokens[i+1].start, ARRAY_SIZE(SDP_ANSWER_VALUE) - 1) == 0) {
                pSessionDescriptionInit->type = SDP_TYPE_ANSWER;
            } else {
                CHK(FALSE, STATUS_SESSION_DESCRIPTION_INIT_INVALID_TYPE);
            }
        } else if (STRNCMP(SDP_KEY, sessionDescriptionJSON + tokens[i].start, ARRAY_SIZE(SDP_KEY) - 1) == 0) {
            CHK((tokens[i + 1].end - tokens[i + 1].start) <= MAX_SESSION_DESCRIPTION_INIT_SDP_LEN,
                STATUS_SESSION_DESCRIPTION_INIT_MAX_SDP_LEN_EXCEEDED);
            last = curr = sessionDescriptionJSON + tokens[i + 1].start;
            last -= ARRAY_SIZE(SESSION_DESCRIPTION_INIT_LINE_ENDING) - 1;
            tail = sessionDescriptionJSON + tokens[i + 1].end;
            j = 0;

            for (;curr <= tail; curr++) {
                if (STRNCMP(curr, SESSION_DESCRIPTION_INIT_LINE_ENDING, ARRAY_SIZE(SESSION_DESCRIPTION_INIT_LINE_ENDING) - 1) == 0) {
                    last += ARRAY_SIZE(SESSION_DESCRIPTION_INIT_LINE_ENDING) - 1;
                    lineLen = (curr - last);

                    MEMCPY((pSessionDescriptionInit->sdp) + j, last, lineLen);
                    j += (lineLen + 1);
                    pSessionDescriptionInit->sdp[j - 1] = '\n';

                    last = curr;
                }
            }
        }
    }

    CHK(pSessionDescriptionInit->sdp[0] != '\0', STATUS_SESSION_DESCRIPTION_INIT_MISSING_SDP);
    CHK(pSessionDescriptionInit->type != 0, STATUS_SESSION_DESCRIPTION_INIT_MISSING_TYPE);

CleanUp:

    LEAVES();
    return retStatus;
}

/*
 * Populate map with PayloadTypes if we are offering
 */
STATUS setPayloadTypesForOffer(PHashTable codecTable)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    CHK_STATUS(hashTableUpsert(codecTable, RTC_CODEC_MULAW, DEFAULT_PAYLOAD_MULAW));
    CHK_STATUS(hashTableUpsert(codecTable, RTC_CODEC_ALAW, DEFAULT_PAYLOAD_ALAW));
    CHK_STATUS(hashTableUpsert(codecTable, RTC_CODEC_VP8, DEFAULT_PAYLOAD_VP8));
    CHK_STATUS(hashTableUpsert(codecTable, RTC_CODEC_OPUS, DEFAULT_PAYLOAD_OPUS));
    CHK_STATUS(hashTableUpsert(codecTable, RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE, DEFAULT_PAYLOAD_H264));

CleanUp:
    return retStatus;
}

/*
 * Populate map with PayloadTypes for codecs a KvsPeerConnection has enabled.
 */
STATUS setPayloadTypesFromOffer(PHashTable codecTable, PHashTable rtxTable, PSessionDescription pSessionDescription)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PSdpMediaDescription pMediaDescription = NULL;
    UINT8 currentMedia, currentAttribute;
    PCHAR attributeValue, end;
    UINT64 parsedPayloadType, rtxPayloadType, hashmapPayloadType;
    BOOL supportCodec;
    UINT32 tokenLen;

    for (currentMedia = 0; currentMedia < pSessionDescription->mediaCount; currentMedia++) {
        pMediaDescription = &(pSessionDescription->mediaDescriptions[currentMedia]);

        attributeValue = pMediaDescription->mediaName;
        do {
            if ((end = STRCHR(attributeValue, ' ')) != NULL) {
                tokenLen = (end - attributeValue);
            } else {
                tokenLen = STRLEN(attributeValue);
            }

            if (STRNCMP(DEFAULT_PAYLOAD_MULAW_STR, attributeValue, tokenLen) == 0) {
                CHK_STATUS(hashTableUpsert(codecTable, RTC_CODEC_MULAW, DEFAULT_PAYLOAD_MULAW));
            } else if (STRNCMP(DEFAULT_PAYLOAD_ALAW_STR, attributeValue, tokenLen) == 0) {
                CHK_STATUS(hashTableUpsert(codecTable, RTC_CODEC_ALAW, DEFAULT_PAYLOAD_ALAW));
            }

            attributeValue = end + 1;
        } while (end != NULL);

        for (currentAttribute = 0; currentAttribute < pMediaDescription->mediaAttributesCount; currentAttribute++) {
            attributeValue = pMediaDescription->sdpAttributes[currentAttribute].attributeValue;

            CHK_STATUS(hashTableContains(codecTable, RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE, &supportCodec));
            if (supportCodec && (end = STRSTR(attributeValue, H264_VALUE)) != NULL) {
                CHK_STATUS(STRTOUI64(attributeValue, end - 1, 10, &parsedPayloadType));
                CHK_STATUS(hashTableUpsert(codecTable, RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE, parsedPayloadType));
            }

            CHK_STATUS(hashTableContains(codecTable, RTC_CODEC_OPUS, &supportCodec));
            if (supportCodec && (end = STRSTR(attributeValue, OPUS_VALUE)) != NULL) {
                CHK_STATUS(STRTOUI64(attributeValue, end - 1, 10, &parsedPayloadType));
                CHK_STATUS(hashTableUpsert(codecTable, RTC_CODEC_OPUS, parsedPayloadType));
            }

            CHK_STATUS(hashTableContains(codecTable, RTC_CODEC_VP8, &supportCodec));
            if (supportCodec && (end = STRSTR(attributeValue, VP8_VALUE)) != NULL) {
                CHK_STATUS(STRTOUI64(attributeValue, end - 1, 10, &parsedPayloadType));
                CHK_STATUS(hashTableUpsert(codecTable, RTC_CODEC_VP8, parsedPayloadType));
            }

            CHK_STATUS(hashTableContains(codecTable, RTC_CODEC_MULAW, &supportCodec));
            if (supportCodec && (end = STRSTR(attributeValue, MULAW_VALUE)) != NULL) {
                CHK_STATUS(STRTOUI64(attributeValue, end - 1, 10, &parsedPayloadType));
                CHK_STATUS(hashTableUpsert(codecTable, RTC_CODEC_MULAW, parsedPayloadType));
            }

            CHK_STATUS(hashTableContains(codecTable, RTC_CODEC_ALAW, &supportCodec));
            if (supportCodec && (end = STRSTR(attributeValue, ALAW_VALUE)) != NULL) {
                CHK_STATUS(STRTOUI64(attributeValue, end - 1, 10, &parsedPayloadType));
                CHK_STATUS(hashTableUpsert(codecTable, RTC_CODEC_ALAW, parsedPayloadType));
            }

            if ((end = STRSTR(attributeValue, RTX_VALUE)) != NULL) {
                CHK_STATUS(STRTOUI64(attributeValue, end - 1, 10, &rtxPayloadType));
                attributeValue = pMediaDescription->sdpAttributes[++currentAttribute].attributeValue;
                if ((end = STRSTR(attributeValue, RTX_CODEC_VALUE)) != NULL) {
                    CHK_STATUS(STRTOUI64(end + STRLEN(RTX_CODEC_VALUE), NULL, 10, &parsedPayloadType));
                    CHK_STATUS(hashTableContains(codecTable, RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE, &supportCodec));
                    if (supportCodec) {
                        CHK_STATUS(hashTableGet(codecTable, RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE, &hashmapPayloadType));
                        if (parsedPayloadType == hashmapPayloadType) {
                            CHK_STATUS(hashTableUpsert(rtxTable, RTC_RTX_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE, rtxPayloadType));
                        }
                    }

                    CHK_STATUS(hashTableContains(codecTable, RTC_CODEC_VP8, &supportCodec));
                    if (supportCodec) {
                        CHK_STATUS(hashTableGet(codecTable, RTC_CODEC_VP8, &hashmapPayloadType));
                        if (parsedPayloadType == hashmapPayloadType) {
                            CHK_STATUS(hashTableUpsert(rtxTable, RTC_RTX_CODEC_VP8, rtxPayloadType));
                        }
                    }
                }
            }
        }
    }

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS setTransceiverPayloadTypes(PHashTable codecTable, PHashTable rtxTable, PDoubleList pTransceivers) {
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PDoubleListNode pCurNode = NULL;
    PKvsRtpTransceiver pKvsRtpTransceiver;
    BOOL containRtx = FALSE;
    UINT64 data;

    // Loop over Transceivers and set the payloadType (which what we got from the other side)
    // If a codec we want to send wasn't supported by the other return an error
    CHK_STATUS(doubleListGetHeadNode(pTransceivers, &pCurNode));
    while (pCurNode != NULL) {
        CHK_STATUS(doubleListGetNodeData(pCurNode, &data));
        pCurNode = pCurNode->pNext;
        pKvsRtpTransceiver = (PKvsRtpTransceiver) data;

        if (pKvsRtpTransceiver != NULL &&
            (pKvsRtpTransceiver->transceiver.direction == RTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV ||
             pKvsRtpTransceiver->transceiver.direction == RTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY)) {

            CHK_STATUS(hashTableGet(codecTable, pKvsRtpTransceiver->sender.track.codec, &data));
            pKvsRtpTransceiver->sender.payloadType = (UINT8) data;
            switch (pKvsRtpTransceiver->sender.track.codec) {
                case RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE:
                    retStatus = hashTableGet(rtxTable, RTC_RTX_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE, &data);
                    CHK(retStatus == STATUS_SUCCESS || retStatus == STATUS_HASH_KEY_NOT_PRESENT, retStatus);
                    break;
                case RTC_CODEC_VP8:
                    retStatus = hashTableGet(rtxTable, RTC_RTX_CODEC_VP8, &data);
                    CHK(retStatus == STATUS_SUCCESS || retStatus == STATUS_HASH_KEY_NOT_PRESENT, retStatus);
                    break;
                default:
                    retStatus = STATUS_HASH_KEY_NOT_PRESENT;
                    break;
            }
            if (retStatus == STATUS_SUCCESS) {
                containRtx = TRUE;
                pKvsRtpTransceiver->sender.rtxPayloadType = (UINT8) data;
            }
            retStatus = STATUS_SUCCESS;
        }
    }

    // Free rolling buffer and retransmitter if rtx is not needed
    if (containRtx) {
        CHK_STATUS(createRtpRollingBuffer(DEFAULT_ROLLING_BUFFER_DURATION_IN_SECONDS * HIGHEST_EXPECTED_BIT_RATE / 8 / DEFAULT_MTU_SIZE, &pKvsRtpTransceiver->sender.packetBuffer));
        CHK_STATUS(createRetransmitter(DEFAULT_SEQ_NUM_BUFFER_SIZE, DEFAULT_VALID_INDEX_BUFFER_SIZE, &pKvsRtpTransceiver->sender.retransmitter));
    }

CleanUp:

    LEAVES();
    return retStatus;
}

// Populate a single media section from a PKvsRtpTransceiver
STATUS populateSingleMediaSection(PKvsPeerConnection pKvsPeerConnection, PKvsRtpTransceiver pKvsRtpTransceiver, PSdpMediaDescription pSdpMediaDescription, PCHAR pCertificateFingerprint, UINT32 mediaSectionId, PCHAR pDtlsRole)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT64 payloadType, rtxPayloadType;
    BOOL containRtx = FALSE;
    UINT32 attributeCount = 0;
    PRtcMediaStreamTrack pRtcMediaStreamTrack = &(pKvsRtpTransceiver->sender.track);

    CHK_STATUS(hashTableGet(pKvsPeerConnection->pCodecTable, pRtcMediaStreamTrack->codec, &payloadType));

    if (pRtcMediaStreamTrack->codec == RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE || pRtcMediaStreamTrack->codec == RTC_CODEC_VP8) {
        if (pRtcMediaStreamTrack->codec == RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE) {
            retStatus = hashTableGet(pKvsPeerConnection->pRtxTable, RTC_RTX_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE, &rtxPayloadType);
        } else {
            retStatus = hashTableGet(pKvsPeerConnection->pRtxTable, RTC_RTX_CODEC_VP8, &rtxPayloadType);
        }
        CHK(retStatus == STATUS_SUCCESS || retStatus == STATUS_HASH_KEY_NOT_PRESENT, retStatus);
        containRtx = (retStatus == STATUS_SUCCESS);
        retStatus = STATUS_SUCCESS;
        if (containRtx) {
            SPRINTF(pSdpMediaDescription->mediaName, "video 9 UDP/TLS/RTP/SAVPF %"PRId64" %"PRId64, payloadType, rtxPayloadType);
        } else {
            SPRINTF(pSdpMediaDescription->mediaName, "video 9 UDP/TLS/RTP/SAVPF %"PRId64, payloadType);
        }
    } else if (pRtcMediaStreamTrack->codec == RTC_CODEC_OPUS || pRtcMediaStreamTrack->codec == RTC_CODEC_MULAW || pRtcMediaStreamTrack->codec == RTC_CODEC_ALAW) {
        SPRINTF(pSdpMediaDescription->mediaName, "audio 9 UDP/TLS/RTP/SAVPF %"PRId64, payloadType);
    }

    CHK_STATUS(iceAgentPopulateSdpMediaDescriptionCandidates(pKvsPeerConnection->pIceAgent, pSdpMediaDescription, MAX_SDP_ATTRIBUTE_VALUE_LENGTH, &attributeCount));

    if (containRtx) {
        STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "msid");
        SPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue, "%s %sRTX", pRtcMediaStreamTrack->streamId, pRtcMediaStreamTrack->trackId);
        attributeCount++;

        STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "ssrc-group");
        SPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue, "FID %u %u", pKvsRtpTransceiver->sender.ssrc, pKvsRtpTransceiver->sender.rtxSsrc);
        attributeCount++;
    } else {
        STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "msid");
        SPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue, "%s %s", pRtcMediaStreamTrack->streamId, pRtcMediaStreamTrack->trackId);
        attributeCount++;
    }

    STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "ssrc");
    SPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue, "%u cname:%s", pKvsRtpTransceiver->sender.ssrc, pKvsPeerConnection->localCNAME);
    attributeCount++;

    STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "ssrc");
    SPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue, "%u msid:%s %s", pKvsRtpTransceiver->sender.ssrc, pRtcMediaStreamTrack->streamId, pRtcMediaStreamTrack->trackId);
    attributeCount++;

    STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "ssrc");
    SPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue, "%u mslabel:%s", pKvsRtpTransceiver->sender.ssrc, pRtcMediaStreamTrack->streamId);
    attributeCount++;

    STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "ssrc");
    SPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue, "%u label:%s", pKvsRtpTransceiver->sender.ssrc, pRtcMediaStreamTrack->trackId);
    attributeCount++;

    if (containRtx) {
        STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "ssrc");
        SPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue, "%u cname:%s", pKvsRtpTransceiver->sender.rtxSsrc, pKvsPeerConnection->localCNAME);
        attributeCount++;

        STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "ssrc");
        SPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue, "%u msid:%s %sRTX", pKvsRtpTransceiver->sender.rtxSsrc, pRtcMediaStreamTrack->streamId, pRtcMediaStreamTrack->trackId);
        attributeCount++;

        STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "ssrc");
        SPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue, "%u mslabel:%sRTX", pKvsRtpTransceiver->sender.rtxSsrc, pRtcMediaStreamTrack->streamId);
        attributeCount++;

        STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "ssrc");
        SPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue, "%u label:%sRTX", pKvsRtpTransceiver->sender.rtxSsrc, pRtcMediaStreamTrack->trackId);
        attributeCount++;
    }

    STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "rtcp");
    STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue, "9 IN IP4 0.0.0.0");
    attributeCount++;

    STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "ice-ufrag");
    STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue, pKvsPeerConnection->localIceUfrag);
    attributeCount++;

    STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "ice-pwd");
    STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue, pKvsPeerConnection->localIcePwd);
    attributeCount++;

    STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "fingerprint");
    STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue, "sha-256 ");
    STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue + 8, pCertificateFingerprint);
    attributeCount++;

    STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "setup");
    STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue, pDtlsRole);
    attributeCount++;

    STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "mid");
    SPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue, "%d", mediaSectionId);
    attributeCount++;

    STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "sendrecv");
    attributeCount++;

    STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "rtcp-mux");
    attributeCount++;

    STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "rtcp-rsize");
    attributeCount++;

    if (pRtcMediaStreamTrack->codec == RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE) {
        STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "rtpmap");
        SPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue, "%"PRId64" H264/90000", payloadType);
        attributeCount++;

        STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "fmtp");
        SPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue, "%"PRId64" level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42e01f", payloadType);
        attributeCount++;

        if (containRtx) {
            STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "rtpmap");
            SPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue, "%"PRId64" "RTX_VALUE, rtxPayloadType);
            attributeCount++;

            STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "fmtp");
            SPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue, "%"PRId64" apt=%"PRId64"", rtxPayloadType, payloadType);
            attributeCount++;
        }
    } else if (pRtcMediaStreamTrack->codec == RTC_CODEC_OPUS) {
        STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "rtpmap");
        SPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue, "%"PRId64" opus/48000/2", payloadType);
        attributeCount++;

        STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "fmtp");
        SPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue, "%"PRId64" minptime=10;useinbandfec=1", payloadType);
        attributeCount++;
    } else if (pRtcMediaStreamTrack->codec == RTC_CODEC_VP8) {
        STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "rtpmap");
        SPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue, "%"PRId64" "VP8_VALUE, payloadType);
        attributeCount++;

        if (containRtx) {
            CHK_STATUS(hashTableGet(pKvsPeerConnection->pRtxTable, RTC_RTX_CODEC_VP8, &rtxPayloadType));
            STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "rtpmap");
            SPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue, "%"PRId64" "RTX_VALUE, rtxPayloadType);
            attributeCount++;

            STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "fmtp");
            SPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue, "%"PRId64" apt=%"PRId64"", rtxPayloadType, payloadType);
            attributeCount++;
        }
    } else if (pRtcMediaStreamTrack->codec == RTC_CODEC_MULAW) {
        STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "rtpmap");
        SPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue, "%"PRId64" "MULAW_VALUE, payloadType);
        attributeCount++;
    } else if (pRtcMediaStreamTrack->codec == RTC_CODEC_ALAW) {
        STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "rtpmap");
        SPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue, "%"PRId64" "ALAW_VALUE, payloadType);
        attributeCount++;
    }

    STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "rtcp-fb");
    SPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue, "%"PRId64" nack", payloadType);
    attributeCount++;

    pSdpMediaDescription->mediaAttributesCount = attributeCount;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS populateSessionDescriptionDataChannel(PKvsPeerConnection pKvsPeerConnection, PSdpMediaDescription pSdpMediaDescription, PCHAR pCertificateFingerprint, UINT32 mediaSectionId, PCHAR pDtlsRole)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 attributeCount = 0;

    SPRINTF(pSdpMediaDescription->mediaName, "application 9 UDP/DTLS/SCTP webrtc-datachannel");

    CHK_STATUS(iceAgentPopulateSdpMediaDescriptionCandidates(pKvsPeerConnection->pIceAgent, pSdpMediaDescription, MAX_SDP_ATTRIBUTE_VALUE_LENGTH, &attributeCount));

    STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "rtcp");
    STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue, "9 IN IP4 0.0.0.0");
    attributeCount++;

    STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "ice-ufrag");
    STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue, pKvsPeerConnection->localIceUfrag);
    attributeCount++;

    STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "ice-pwd");
    STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue, pKvsPeerConnection->localIcePwd);
    attributeCount++;

    STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "fingerprint");
    STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue, "sha-256 ");
    STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue + 8, pCertificateFingerprint);
    attributeCount++;

    STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "setup");
    STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue, pDtlsRole);
    attributeCount++;

    STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "mid");
    SPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue, "%d", mediaSectionId);
    attributeCount++;

    STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "sctp-port");
    SPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue, "5000");
    attributeCount++;

    pSdpMediaDescription->mediaAttributesCount = attributeCount;

CleanUp:

    LEAVES();
    return retStatus;
}


// Populate the media sections of a SessionDescription with the current state of the KvsPeerConnection
STATUS populateSessionDescriptionMedia(PKvsPeerConnection pKvsPeerConnection, PSessionDescription pRemoteSessionDescription, PSessionDescription pLocalSessionDescription)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PDoubleListNode pCurNode = NULL;
    CHAR certificateFingerprint[CERTIFICATE_FINGERPRINT_LENGTH];
    UINT64 data;
    PKvsRtpTransceiver pKvsRtpTransceiver;
    PCHAR pDtlsRole = NULL;

    CHK_STATUS(dtlsSessionGenerateLocalCertificateFingerprint(pKvsPeerConnection->pDtlsSession, certificateFingerprint, CERTIFICATE_FINGERPRINT_LENGTH));

    if (pKvsPeerConnection->isOffer) {
        pDtlsRole = DTLS_ROLE_ACTPASS;
    } else {
        pDtlsRole = DTLS_ROLE_ACTIVE;
        CHK_STATUS(reorderTransceiverByRemoteDescription(pKvsPeerConnection, pRemoteSessionDescription));
    }

    CHK_STATUS(doubleListGetHeadNode(pKvsPeerConnection->pTransceievers, &pCurNode));
    while (pCurNode != NULL) {
        CHK_STATUS(doubleListGetNodeData(pCurNode, &data));
        pCurNode = pCurNode->pNext;
        pKvsRtpTransceiver = (PKvsRtpTransceiver) data;
        if (pKvsRtpTransceiver != NULL) {
            CHK_STATUS(populateSingleMediaSection(
                        pKvsPeerConnection,
                        pKvsRtpTransceiver,
                        &(pLocalSessionDescription->mediaDescriptions[pLocalSessionDescription->mediaCount]),
                        certificateFingerprint,
                        pLocalSessionDescription->mediaCount,
                        pDtlsRole
            ));
            pLocalSessionDescription->mediaCount++;
        }
    }

    if (pKvsPeerConnection->sctpIsEnabled) {
        CHK_STATUS(populateSessionDescriptionDataChannel(
                        pKvsPeerConnection,
                        &(pLocalSessionDescription->mediaDescriptions[pLocalSessionDescription->mediaCount]),
                        certificateFingerprint,
                        pLocalSessionDescription->mediaCount,
                        pDtlsRole
        ));
        pLocalSessionDescription->mediaCount++;
    }

CleanUp:

    LEAVES();
    return retStatus;
}


// Populate a SessionDescription with the current state of the KvsPeerConnection
STATUS populateSessionDescription(PKvsPeerConnection pKvsPeerConnection, PSessionDescription pRemoteSessionDescription, PSessionDescription pLocalSessionDescription)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    CHAR bundleValue[MAX_SDP_ATTRIBUTE_VALUE_LENGTH], wmsValue[MAX_SDP_ATTRIBUTE_VALUE_LENGTH];
    PCHAR curr = NULL;
    UINT32 i, sizeRemaining;

    CHK(pKvsPeerConnection != NULL && pLocalSessionDescription != NULL && pRemoteSessionDescription != NULL, STATUS_NULL_ARG);

    CHK_STATUS(populateSessionDescriptionMedia(pKvsPeerConnection, pRemoteSessionDescription, pLocalSessionDescription));

    MEMSET(bundleValue, 0, MAX_SDP_ATTRIBUTE_VALUE_LENGTH);
    MEMSET(wmsValue, 0, MAX_SDP_ATTRIBUTE_VALUE_LENGTH);

    STRCPY(pLocalSessionDescription->sdpOrigin.userName, "-");
    pLocalSessionDescription->sdpOrigin.sessionId = RAND();
    pLocalSessionDescription->sdpOrigin.sessionVersion = 2;
    STRCPY(pLocalSessionDescription->sdpOrigin.sdpConnectionInformation.networkType, "IN");
    STRCPY(pLocalSessionDescription->sdpOrigin.sdpConnectionInformation.addressType, "IP4");
    STRCPY(pLocalSessionDescription->sdpOrigin.sdpConnectionInformation.connectionAddress, "127.0.0.1");

    STRCPY(pLocalSessionDescription->sessionName, "-");

    pLocalSessionDescription->timeDescriptionCount = 1;
    pLocalSessionDescription->sdpTimeDescription[0].startTime = 0;
    pLocalSessionDescription->sdpTimeDescription[0].stopTime = 0;

    STRCPY(pLocalSessionDescription->sdpAttributes[0].attributeName, "group");
    STRCPY(pLocalSessionDescription->sdpAttributes[0].attributeValue, BUNDLE_KEY);
    for (curr = (pLocalSessionDescription->sdpAttributes[0].attributeValue + ARRAY_SIZE(BUNDLE_KEY) - 1), i = 0; i < pLocalSessionDescription->mediaCount; i++) {
        STRCPY(pLocalSessionDescription->mediaDescriptions[i].sdpConnectionInformation.networkType, "IN");
        STRCPY(pLocalSessionDescription->mediaDescriptions[i].sdpConnectionInformation.addressType, "IP4");
        STRCPY(pLocalSessionDescription->mediaDescriptions[i].sdpConnectionInformation.connectionAddress, "127.0.0.1");

        sizeRemaining = MAX_SDP_ATTRIBUTE_VALUE_LENGTH - (curr - pLocalSessionDescription->sdpAttributes[0].attributeValue);
        curr += SNPRINTF(curr, sizeRemaining, " %d", i);
    }
    pLocalSessionDescription->sessionAttributesCount++;

    STRCPY(pLocalSessionDescription->sdpAttributes[pLocalSessionDescription->sessionAttributesCount].attributeName, "msid-semantic");
    STRCPY(pLocalSessionDescription->sdpAttributes[pLocalSessionDescription->sessionAttributesCount].attributeValue, " WMS myKvsVideoStream");
    pLocalSessionDescription->sessionAttributesCount++;

CleanUp:

    LEAVES();
    return retStatus;
}

// primarily meant to be used by reorderTransceiverByRemoteDescription
// Find a Transceiver with n codec, and then copy it to the end of the transceievers
// this allows us to re-order by the order the remote dictates
STATUS copyTransceiverWithCodec(PKvsPeerConnection pKvsPeerConnection, RTC_CODEC rtcCodec, PBOOL pDidFindCodec)
{
    STATUS retStatus = STATUS_SUCCESS;
    PDoubleListNode pCurNode = NULL;
    PKvsRtpTransceiver pTargetKvsRtpTransceiver = NULL, pKvsRtpTransceiver;
    UINT64 data;

    CHK(pKvsPeerConnection != NULL && pDidFindCodec != NULL, STATUS_NULL_ARG);

    *pDidFindCodec = FALSE;

    CHK_STATUS(doubleListGetHeadNode(pKvsPeerConnection->pTransceievers, &pCurNode));
    while (pCurNode != NULL && pTargetKvsRtpTransceiver == NULL) {
        CHK_STATUS(doubleListGetNodeData(pCurNode, &data));
        pCurNode = pCurNode->pNext;
        pKvsRtpTransceiver = (PKvsRtpTransceiver) data;
        if (pKvsRtpTransceiver != NULL && pKvsRtpTransceiver->sender.track.codec == rtcCodec) {
            pTargetKvsRtpTransceiver = pKvsRtpTransceiver;
        }
    }
    if (pTargetKvsRtpTransceiver != NULL) {
        CHK_STATUS(doubleListInsertItemTail(pKvsPeerConnection->pTransceievers, (UINT64) pTargetKvsRtpTransceiver));
        *pDidFindCodec = TRUE;
    }

CleanUp:

    return retStatus;
}

STATUS reorderTransceiverByRemoteDescription(PKvsPeerConnection pKvsPeerConnection, PSessionDescription pRemoteSessionDescription)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 currentMedia, currentAttribute, transceieverCount = 0, i, tokenLen;
    PSdpMediaDescription pMediaDescription = NULL;
    PCHAR attributeValue, end;
    BOOL supportCodec, foundMediaSectionWithCodec;
    RTC_CODEC rtcCodec;

    // change the order of pKvsPeerConnection->pTransceievers to have the same codec order in pRemoteSessionDescription
    CHK_STATUS(doubleListGetNodeCount(pKvsPeerConnection->pTransceievers, &transceieverCount));

    for (currentMedia = 0; currentMedia < pRemoteSessionDescription->mediaCount; currentMedia++) {
        pMediaDescription = &(pRemoteSessionDescription->mediaDescriptions[currentMedia]);
        foundMediaSectionWithCodec = FALSE;

        // Scan the media section name for any codecs we support
        attributeValue = pMediaDescription->mediaName;
        do {
            if ((end = STRCHR(attributeValue, ' ')) != NULL) {
                tokenLen = (end - attributeValue);
            } else {
                tokenLen = STRLEN(attributeValue);
            }

            if (STRNCMP(DEFAULT_PAYLOAD_MULAW_STR, attributeValue, tokenLen) == 0) {
                supportCodec = TRUE;
                rtcCodec = RTC_CODEC_MULAW;
            } else if (STRNCMP(DEFAULT_PAYLOAD_ALAW_STR, attributeValue, tokenLen) == 0) {
                supportCodec = TRUE;
                rtcCodec = RTC_CODEC_ALAW;
            } else {
                supportCodec = FALSE;
            }

            // find transceiever with rtcCodec and duplicate it at tail
            if (supportCodec) {
                CHK_STATUS(copyTransceiverWithCodec(pKvsPeerConnection, rtcCodec, &foundMediaSectionWithCodec));
            }
            attributeValue = end + 1;
        } while (end != NULL && !foundMediaSectionWithCodec);

        // Scan the media section attributes for codecs we support
        for (currentAttribute = 0; currentAttribute < pMediaDescription->mediaAttributesCount && !foundMediaSectionWithCodec; currentAttribute++) {
            attributeValue = pMediaDescription->sdpAttributes[currentAttribute].attributeValue;

            if (STRSTR(attributeValue, H264_VALUE) != NULL){
                supportCodec = TRUE;
                rtcCodec = RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE;
            } else if (STRSTR(attributeValue, OPUS_VALUE) != NULL) {
                supportCodec = TRUE;
                rtcCodec = RTC_CODEC_OPUS;
            } else if (STRSTR(attributeValue, MULAW_VALUE) != NULL) {
                supportCodec = TRUE;
                rtcCodec = RTC_CODEC_MULAW;
            } else if (STRSTR(attributeValue, ALAW_VALUE) != NULL) {
                supportCodec = TRUE;
                rtcCodec = RTC_CODEC_ALAW;
            } else if (STRSTR(attributeValue, VP8_VALUE) != NULL) {
                supportCodec = TRUE;
                rtcCodec = RTC_CODEC_VP8;
            } else {
                supportCodec = FALSE;
            }

            // find transceiever with rtcCodec and duplicate it at tail
            if (supportCodec) {
                CHK_STATUS(copyTransceiverWithCodec(pKvsPeerConnection, rtcCodec, &foundMediaSectionWithCodec));
            }
        }
    }

    // delete the unordered part in pKvsPeerConnection->pTransceievers
    for (i = 0; i < transceieverCount; ++i) {
        CHK_STATUS(doubleListDeleteHead(pKvsPeerConnection->pTransceievers));
    }

CleanUp:

    CHK_LOG_ERR_NV(retStatus);

    LEAVES();
    return retStatus;
}

STATUS deserializeRtcIceCandidateInit(PCHAR pJson, UINT32 jsonLen, PRtcIceCandidateInit pRtcIceCandidateInit)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    jsmntok_t tokens[MAX_JSON_TOKEN_COUNT];
    jsmn_parser parser;
    INT8 i;
    INT32 tokenCount;

    CHK(pRtcIceCandidateInit != NULL && pJson != NULL, STATUS_NULL_ARG);
    MEMSET(pRtcIceCandidateInit->candidate, 0x00, MAX_ICE_CANDIDATE_INIT_CANDIDATE_LEN + 1);

    jsmn_init(&parser);

    tokenCount = jsmn_parse(&parser, pJson, jsonLen, tokens, ARRAY_SIZE(tokens));
    CHK(tokenCount > 1, STATUS_INVALID_API_CALL_RETURN_JSON);
    CHK(tokens[0].type == JSMN_OBJECT, STATUS_ICE_CANDIDATE_INIT_MALFORMED);

    for (i = 1; i < (tokenCount - 1); i++) {
        if (STRNCMP(CANDIDATE_KEY, pJson + tokens[i].start, ARRAY_SIZE(CANDIDATE_KEY) - 1) == 0) {
            STRNCPY(pRtcIceCandidateInit->candidate, pJson + tokens[i + 1].start, (tokens[i + 1].end - tokens[i + 1].start));
        }
    }

    CHK(pRtcIceCandidateInit->candidate[0] != '\0', STATUS_ICE_CANDIDATE_MISSING_CANDIDATE);

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS setReceiversSsrc(PSessionDescription pRemoteSessionDescription, PDoubleList pTransceievers)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSdpMediaDescription pMediaDescription = NULL;
    BOOL foundSsrc, isVideoMediaSection, isAudioMediaSection, isAudioCodec, isVideoCodec;
    UINT32 currentAttribute, currentMedia, ssrc;
    UINT64 data;
    PDoubleListNode pCurNode = NULL;
    PKvsRtpTransceiver pKvsRtpTransceiver;
    RTC_CODEC codec;
    PCHAR end = NULL;

    for (currentMedia = 0; currentMedia < pRemoteSessionDescription->mediaCount; currentMedia++) {
        pMediaDescription = &(pRemoteSessionDescription->mediaDescriptions[currentMedia]);
        isVideoMediaSection = (STRNCMP(pMediaDescription->mediaName, MEDIA_SECTION_VIDEO_VALUE, ARRAY_SIZE(MEDIA_SECTION_VIDEO_VALUE) - 1) == 0);
        isAudioMediaSection = (STRNCMP(pMediaDescription->mediaName, MEDIA_SECTION_AUDIO_VALUE, ARRAY_SIZE(MEDIA_SECTION_AUDIO_VALUE) - 1) == 0);
        foundSsrc = FALSE;
        ssrc = 0;

        if (isVideoMediaSection || isAudioMediaSection) {
            for (currentAttribute = 0; currentAttribute < pMediaDescription->mediaAttributesCount && !foundSsrc; currentAttribute++) {
                if (STRNCMP(pMediaDescription->sdpAttributes[currentAttribute].attributeName, SSRC_KEY, STRLEN(pMediaDescription->sdpAttributes[currentAttribute].attributeName)) == 0) {
                    if ((end = STRCHR(pMediaDescription->sdpAttributes[currentAttribute].attributeValue, ' ')) != NULL) {
                        CHK_STATUS(STRTOUI32(pMediaDescription->sdpAttributes[currentAttribute].attributeValue, end, 10, &ssrc));
                        foundSsrc = TRUE;
                    }
                }
            }

            if (foundSsrc) {
                CHK_STATUS(doubleListGetHeadNode(pTransceievers, &pCurNode));
                while (pCurNode != NULL) {
                    CHK_STATUS(doubleListGetNodeData(pCurNode, &data));
                    pKvsRtpTransceiver = (PKvsRtpTransceiver) data;
                    codec = pKvsRtpTransceiver->sender.track.codec;
                    isVideoCodec = (codec == RTC_CODEC_VP8 || codec == RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE);
                    isAudioCodec = (codec == RTC_CODEC_MULAW || codec == RTC_CODEC_ALAW || codec == RTC_CODEC_OPUS);


                    if (pKvsRtpTransceiver->jitterBufferSsrc == 0 && ((isVideoCodec && isVideoMediaSection) || (isAudioCodec && isAudioMediaSection))) {
                        // Finish iteration, we assigned the ssrc move on to next media section
                        pKvsRtpTransceiver->jitterBufferSsrc = ssrc;
                        pCurNode = NULL;
                    } else {
                        pCurNode = pCurNode->pNext;
                    }

                }
            }
        }
    }

CleanUp:

    return retStatus;
}
