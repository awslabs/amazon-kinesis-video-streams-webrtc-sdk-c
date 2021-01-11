#define LOG_CLASS "SessionDescription"
#include "../Include_i.h"

STATUS serializeSessionDescriptionInit(PRtcSessionDescriptionInit pSessionDescriptionInit, PCHAR sessionDescriptionJSON,
                                       PUINT32 sessionDescriptionJSONLen)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PCHAR curr, tail, next;
    UINT32 lineLen, inputSize = 0, amountWritten;

    CHK(pSessionDescriptionInit != NULL && sessionDescriptionJSONLen != NULL, STATUS_NULL_ARG);

    inputSize = *sessionDescriptionJSONLen;
    *sessionDescriptionJSONLen = 0;

    amountWritten =
        SNPRINTF(sessionDescriptionJSON, sessionDescriptionJSON == NULL ? 0 : inputSize - *sessionDescriptionJSONLen,
                 SESSION_DESCRIPTION_INIT_TEMPLATE_HEAD, pSessionDescriptionInit->type == SDP_TYPE_OFFER ? SDP_OFFER_VALUE : SDP_ANSWER_VALUE);
    CHK(sessionDescriptionJSON == NULL || ((inputSize - *sessionDescriptionJSONLen) >= amountWritten), STATUS_BUFFER_TOO_SMALL);
    *sessionDescriptionJSONLen += amountWritten;

    curr = pSessionDescriptionInit->sdp;
    tail = pSessionDescriptionInit->sdp + STRLEN(pSessionDescriptionInit->sdp);

    while ((next = STRNCHR(curr, (UINT32)(tail - curr), '\n')) != NULL) {
        lineLen = (UINT32)(next - curr);

        if (lineLen > 0 && curr[lineLen - 1] == '\r') {
            lineLen--;
        }

        amountWritten =
            SNPRINTF(sessionDescriptionJSON + *sessionDescriptionJSONLen, sessionDescriptionJSON == NULL ? 0 : inputSize - *sessionDescriptionJSONLen,
                     "%*.*s%s", lineLen, lineLen, curr, SESSION_DESCRIPTION_INIT_LINE_ENDING);
        CHK(sessionDescriptionJSON == NULL || ((inputSize - *sessionDescriptionJSONLen) >= amountWritten), STATUS_BUFFER_TOO_SMALL);

        *sessionDescriptionJSONLen += amountWritten;
        curr = next + 1;
    }

    amountWritten = SNPRINTF(sessionDescriptionJSON + *sessionDescriptionJSONLen,
                             sessionDescriptionJSON == NULL ? 0 : inputSize - *sessionDescriptionJSONLen, SESSION_DESCRIPTION_INIT_TEMPLATE_TAIL);
    CHK(sessionDescriptionJSON == NULL || ((inputSize - *sessionDescriptionJSONLen) >= amountWritten), STATUS_BUFFER_TOO_SMALL);
    *sessionDescriptionJSONLen += (amountWritten + 1); // NULL terminator

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS deserializeSessionDescriptionInit(PCHAR sessionDescriptionJSON, UINT32 sessionDescriptionJSONLen,
                                         PRtcSessionDescriptionInit pSessionDescriptionInit)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    jsmntok_t tokens[MAX_JSON_TOKEN_COUNT];
    jsmn_parser parser;
    INT32 i, j, tokenCount, lineLen;
    PCHAR curr, next, tail;

    CHK(pSessionDescriptionInit != NULL && sessionDescriptionJSON != NULL, STATUS_NULL_ARG);
    MEMSET(pSessionDescriptionInit, 0x00, SIZEOF(RtcSessionDescriptionInit));

    jsmn_init(&parser);

    tokenCount = jsmn_parse(&parser, sessionDescriptionJSON, sessionDescriptionJSONLen, tokens, ARRAY_SIZE(tokens));
    CHK(tokenCount > 1, STATUS_INVALID_API_CALL_RETURN_JSON);
    CHK(tokens[0].type == JSMN_OBJECT, STATUS_SESSION_DESCRIPTION_INIT_NOT_OBJECT);

    for (i = 1; i < tokenCount; i += 2) {
        if (STRNCMP(SDP_TYPE_KEY, sessionDescriptionJSON + tokens[i].start, ARRAY_SIZE(SDP_TYPE_KEY) - 1) == 0) {
            if (STRNCMP(SDP_OFFER_VALUE, sessionDescriptionJSON + tokens[i + 1].start, ARRAY_SIZE(SDP_OFFER_VALUE) - 1) == 0) {
                pSessionDescriptionInit->type = SDP_TYPE_OFFER;
            } else if (STRNCMP(SDP_ANSWER_VALUE, sessionDescriptionJSON + tokens[i + 1].start, ARRAY_SIZE(SDP_ANSWER_VALUE) - 1) == 0) {
                pSessionDescriptionInit->type = SDP_TYPE_ANSWER;
            } else {
                CHK(FALSE, STATUS_SESSION_DESCRIPTION_INIT_INVALID_TYPE);
            }
        } else if (STRNCMP(SDP_KEY, sessionDescriptionJSON + tokens[i].start, ARRAY_SIZE(SDP_KEY) - 1) == 0) {
            CHK((tokens[i + 1].end - tokens[i + 1].start) <= MAX_SESSION_DESCRIPTION_INIT_SDP_LEN,
                STATUS_SESSION_DESCRIPTION_INIT_MAX_SDP_LEN_EXCEEDED);
            curr = sessionDescriptionJSON + tokens[i + 1].start;
            tail = sessionDescriptionJSON + tokens[i + 1].end;
            j = 0;

            // Unescape carriage return and line feed characters. The SDP that we receive at this point is in
            // JSON format, meaning that carriage return and line feed characters are escaped. So, to represent
            // these characters, a single escape character is prepended to each of them.
            //
            // When we store the sdp in memory, we want to recover the original format, without the escape characters.
            //
            // For example:
            //     \r becomes '\' and 'r'
            //     \n becomes '\' and 'n'
            while ((next = STRNSTR(curr, SESSION_DESCRIPTION_INIT_LINE_ENDING_WITHOUT_CR, tail - curr)) != NULL) {
                lineLen = (INT32)(next - curr);

                // Check if the SDP format is using \r\n or \n separator.
                // There are escape characters before \n and \r, so we need to move back 1 more character
                if (lineLen > 1 && curr[lineLen - 2] == '\\' && curr[lineLen - 1] == 'r') {
                    lineLen -= 2;
                }

                MEMCPY((pSessionDescriptionInit->sdp) + j, curr, lineLen * SIZEOF(CHAR));
                // Since we're adding 2 characters to the line, \r and \n (SDP record is separated by crlf),
                // we need to add 2 to the serialized line so that the next iteration will not overwrite
                // these 2 characters.
                j += (lineLen + 2);
                pSessionDescriptionInit->sdp[j - 2] = '\r';
                pSessionDescriptionInit->sdp[j - 1] = '\n';

                curr = next + 2;
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
    UINT64 parsedPayloadType, hashmapPayloadType, fmtpVal, aptVal;
    UINT16 aptFmtpVals[MAX_SDP_FMTP_VALUES];
    UINT16 aptFmtVal;
    BOOL supportCodec;
    UINT32 tokenLen, i, aptFmtpValCount;
    PCHAR fmtp;
    UINT64 fmtpScore, bestFmtpScore;

    for (currentMedia = 0; currentMedia < pSessionDescription->mediaCount; currentMedia++) {
        pMediaDescription = &(pSessionDescription->mediaDescriptions[currentMedia]);
        aptFmtpValCount = 0;
        bestFmtpScore = 0;
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

            if (end != NULL) {
                attributeValue = end + 1;
            }
        } while (end != NULL);

        for (currentAttribute = 0; currentAttribute < pMediaDescription->mediaAttributesCount; currentAttribute++) {
            attributeValue = pMediaDescription->sdpAttributes[currentAttribute].attributeValue;

            CHK_STATUS(hashTableContains(codecTable, RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE, &supportCodec));
            if (supportCodec && (end = STRSTR(attributeValue, H264_VALUE)) != NULL) {
                CHK_STATUS(STRTOUI64(attributeValue, end - 1, 10, &parsedPayloadType));
                fmtp = fmtpForPayloadType(parsedPayloadType, pSessionDescription);
                fmtpScore = getH264FmtpScore(fmtp);
                // When there's no match, the last fmtp will be chosen. This will allow us to not break existing customers who might be using
                // flexible decoders which can infer the video profile from the SPS header.
                if (fmtpScore >= bestFmtpScore) {
                    DLOGV("Found H264 payload type %" PRId64 " with score %lu: %s", parsedPayloadType, fmtpScore, fmtp);
                    CHK_STATUS(
                        hashTableUpsert(codecTable, RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE, parsedPayloadType));
                    bestFmtpScore = fmtpScore;
                }
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

            if ((end = STRSTR(attributeValue, RTX_CODEC_VALUE)) != NULL) {
                CHK_STATUS(STRTOUI64(end + STRLEN(RTX_CODEC_VALUE), NULL, 10, &parsedPayloadType));
                if ((end = STRSTR(attributeValue, FMTP_VALUE)) != NULL) {
                    CHK_STATUS(STRTOUI64(end + STRLEN(FMTP_VALUE), NULL, 10, &fmtpVal));
                    aptFmtpVals[aptFmtpValCount++] = (UINT32)((fmtpVal << 8u) & parsedPayloadType);
                }
            }
        }

        for (i = 0; i < aptFmtpValCount; i++) {
            aptFmtVal = aptFmtpVals[i];
            fmtpVal = aptFmtVal >> 8u;
            aptVal = aptFmtVal & 0xFFu;

            CHK_STATUS(hashTableContains(codecTable, RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE, &supportCodec));
            if (supportCodec) {
                CHK_STATUS(hashTableGet(codecTable, RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE, &hashmapPayloadType));
                if (aptVal == hashmapPayloadType) {
                    CHK_STATUS(hashTableUpsert(rtxTable, RTC_RTX_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE, fmtpVal));
                    DLOGV("found apt type %" PRId64 " for fmtp %" PRId64, aptVal, fmtpVal);
                }
            }

            CHK_STATUS(hashTableContains(codecTable, RTC_CODEC_VP8, &supportCodec));
            if (supportCodec) {
                CHK_STATUS(hashTableGet(codecTable, RTC_CODEC_VP8, &hashmapPayloadType));
                if (aptVal == hashmapPayloadType) {
                    CHK_STATUS(hashTableUpsert(rtxTable, RTC_RTX_CODEC_VP8, fmtpVal));
                }
            }
        }
    }

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS setTransceiverPayloadTypes(PHashTable codecTable, PHashTable rtxTable, PDoubleList pTransceivers)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PDoubleListNode pCurNode = NULL;
    PKvsRtpTransceiver pKvsRtpTransceiver;
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
            pKvsRtpTransceiver->sender.rtxPayloadType = (UINT8) data;

            // NACKs may have distinct PayloadTypes, look in the rtxTable and check. Otherwise NACKs will just be re-sending the same seqnum
            if (hashTableGet(rtxTable, pKvsRtpTransceiver->sender.track.codec, &data) == STATUS_SUCCESS) {
                pKvsRtpTransceiver->sender.rtxPayloadType = (UINT8) data;
            }
        }

        CHK_STATUS(createRtpRollingBuffer(DEFAULT_ROLLING_BUFFER_DURATION_IN_SECONDS * HIGHEST_EXPECTED_BIT_RATE / 8 / DEFAULT_MTU_SIZE,
                                          &pKvsRtpTransceiver->sender.packetBuffer));
        CHK_STATUS(createRetransmitter(DEFAULT_SEQ_NUM_BUFFER_SIZE, DEFAULT_VALID_INDEX_BUFFER_SIZE, &pKvsRtpTransceiver->sender.retransmitter));
    }

CleanUp:

    LEAVES();
    return retStatus;
}

PCHAR fmtpForPayloadType(UINT64 payloadType, PSessionDescription pSessionDescription)
{
    UINT32 currentMedia, currentAttribute;
    PSdpMediaDescription pMediaDescription = NULL;
    CHAR payloadStr[MAX_SDP_ATTRIBUTE_VALUE_LENGTH];

    MEMSET(payloadStr, 0x00, MAX_SDP_ATTRIBUTE_VALUE_LENGTH);
    SPRINTF(payloadStr, "%" PRId64, payloadType);

    for (currentMedia = 0; currentMedia < pSessionDescription->mediaCount; currentMedia++) {
        pMediaDescription = &(pSessionDescription->mediaDescriptions[currentMedia]);
        for (currentAttribute = 0; currentAttribute < pMediaDescription->mediaAttributesCount; currentAttribute++) {
            if (STRCMP(pMediaDescription->sdpAttributes[currentAttribute].attributeName, "fmtp") == 0 &&
                STRNCMP(pMediaDescription->sdpAttributes[currentAttribute].attributeValue, payloadStr, STRLEN(payloadStr)) == 0) {
                return pMediaDescription->sdpAttributes[currentAttribute].attributeValue + STRLEN(payloadStr) + 1;
            }
        }
    }

    return NULL;
}

/*
 * Extracts a (hex) value after the provided prefix string. Returns true if
 * successful.
 */
BOOL readHexValue(PCHAR input, PCHAR prefix, PUINT32 value)
{
    PCHAR substr = STRSTR(input, prefix);
    if (substr != NULL && SSCANF(substr + STRLEN(prefix), "%x", value) == 1) {
        return TRUE;
    }
    return FALSE;
}

/*
 * Scores the provided fmtp string based on this library's ability to
 * process various types of H264 streams. A score of 0 indicates an
 * incompatible fmtp line. Beyond this, a higher score indicates more
 * compatibility with the desired characteristics, packetization-mode=1,
 * level-asymmetry-allowed=1, and inbound match with our preferred
 * profile-level-id.
 *
 * At some future time, it may be worth expressing this as a true distance
 * function as defined here, although dealing with infinite floating point
 * values can get tricky:
 * https://www.w3.org/TR/mediacapture-streams/#dfn-fitness-distance
 */
UINT64 getH264FmtpScore(PCHAR fmtp)
{
    UINT32 profileId = 0, packetizationMode = 0, levelAsymmetry = 0;
    UINT64 score = 0;

    // No ftmp match found.
    if (fmtp == NULL) {
        return 0;
    }

    // Currently, the packetization mode must be 1, as the packetization logic
    // is currently not configurable, and sends both NALU and FU-A packets.
    // https://tools.ietf.org/html/rfc7742#section-6.2
    if (readHexValue(fmtp, "packetization-mode=", &packetizationMode) && packetizationMode == 1) {
        score++;
    }

    if (readHexValue(fmtp, "profile-level-id=", &profileId) &&
        (profileId & H264_FMTP_SUBPROFILE_MASK) == (H264_PROFILE_42E01F & H264_FMTP_SUBPROFILE_MASK) &&
        (profileId & H264_FMTP_PROFILE_LEVEL_MASK) <= (H264_PROFILE_42E01F & H264_FMTP_PROFILE_LEVEL_MASK)) {
        score++;
    }

    if (readHexValue(fmtp, "level-asymmetry-allowed=", &levelAsymmetry) && levelAsymmetry == 1) {
        score++;
    }

    return score;
}

// Populate a single media section from a PKvsRtpTransceiver
STATUS populateSingleMediaSection(PKvsPeerConnection pKvsPeerConnection, PKvsRtpTransceiver pKvsRtpTransceiver,
                                  PSdpMediaDescription pSdpMediaDescription, PSessionDescription pRemoteSessionDescription,
                                  PCHAR pCertificateFingerprint, UINT32 mediaSectionId, PCHAR pDtlsRole)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT64 payloadType, rtxPayloadType;
    BOOL containRtx = FALSE;
    BOOL directionFound = FALSE;
    UINT32 i, remoteAttributeCount, attributeCount = 0;
    PRtcMediaStreamTrack pRtcMediaStreamTrack = &(pKvsRtpTransceiver->sender.track);
    PSdpMediaDescription pSdpMediaDescriptionRemote;
    PCHAR currentFmtp = NULL;

    CHK_STATUS(hashTableGet(pKvsPeerConnection->pCodecTable, pRtcMediaStreamTrack->codec, &payloadType));
    currentFmtp = fmtpForPayloadType(payloadType, &(pKvsPeerConnection->remoteSessionDescription));

    if (pRtcMediaStreamTrack->codec == RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE ||
        pRtcMediaStreamTrack->codec == RTC_CODEC_VP8) {
        if (pRtcMediaStreamTrack->codec == RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE) {
            retStatus = hashTableGet(pKvsPeerConnection->pRtxTable, RTC_RTX_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE,
                                     &rtxPayloadType);
        } else {
            retStatus = hashTableGet(pKvsPeerConnection->pRtxTable, RTC_RTX_CODEC_VP8, &rtxPayloadType);
        }
        CHK(retStatus == STATUS_SUCCESS || retStatus == STATUS_HASH_KEY_NOT_PRESENT, retStatus);
        containRtx = (retStatus == STATUS_SUCCESS);
        retStatus = STATUS_SUCCESS;
        if (containRtx) {
            SPRINTF(pSdpMediaDescription->mediaName, "video 9 UDP/TLS/RTP/SAVPF %" PRId64 " %" PRId64, payloadType, rtxPayloadType);
        } else {
            SPRINTF(pSdpMediaDescription->mediaName, "video 9 UDP/TLS/RTP/SAVPF %" PRId64, payloadType);
        }
    } else if (pRtcMediaStreamTrack->codec == RTC_CODEC_OPUS || pRtcMediaStreamTrack->codec == RTC_CODEC_MULAW ||
               pRtcMediaStreamTrack->codec == RTC_CODEC_ALAW) {
        SPRINTF(pSdpMediaDescription->mediaName, "audio 9 UDP/TLS/RTP/SAVPF %" PRId64, payloadType);
    }

    CHK_STATUS(iceAgentPopulateSdpMediaDescriptionCandidates(pKvsPeerConnection->pIceAgent, pSdpMediaDescription, MAX_SDP_ATTRIBUTE_VALUE_LENGTH,
                                                             &attributeCount));

    if (containRtx) {
        STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "msid");
        SPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue, "%s %sRTX", pRtcMediaStreamTrack->streamId,
                pRtcMediaStreamTrack->trackId);
        attributeCount++;

        STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "ssrc-group");
        SPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue, "FID %u %u", pKvsRtpTransceiver->sender.ssrc,
                pKvsRtpTransceiver->sender.rtxSsrc);
        attributeCount++;
    } else {
        STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "msid");
        SPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue, "%s %s", pRtcMediaStreamTrack->streamId,
                pRtcMediaStreamTrack->trackId);
        attributeCount++;
    }

    STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "ssrc");
    SPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue, "%u cname:%s", pKvsRtpTransceiver->sender.ssrc,
            pKvsPeerConnection->localCNAME);
    attributeCount++;

    STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "ssrc");
    SPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue, "%u msid:%s %s", pKvsRtpTransceiver->sender.ssrc,
            pRtcMediaStreamTrack->streamId, pRtcMediaStreamTrack->trackId);
    attributeCount++;

    STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "ssrc");
    SPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue, "%u mslabel:%s", pKvsRtpTransceiver->sender.ssrc,
            pRtcMediaStreamTrack->streamId);
    attributeCount++;

    STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "ssrc");
    SPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue, "%u label:%s", pKvsRtpTransceiver->sender.ssrc,
            pRtcMediaStreamTrack->trackId);
    attributeCount++;

    if (containRtx) {
        STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "ssrc");
        SPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue, "%u cname:%s", pKvsRtpTransceiver->sender.rtxSsrc,
                pKvsPeerConnection->localCNAME);
        attributeCount++;

        STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "ssrc");
        SPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue, "%u msid:%s %sRTX", pKvsRtpTransceiver->sender.rtxSsrc,
                pRtcMediaStreamTrack->streamId, pRtcMediaStreamTrack->trackId);
        attributeCount++;

        STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "ssrc");
        SPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue, "%u mslabel:%sRTX", pKvsRtpTransceiver->sender.rtxSsrc,
                pRtcMediaStreamTrack->streamId);
        attributeCount++;

        STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "ssrc");
        SPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue, "%u label:%sRTX", pKvsRtpTransceiver->sender.rtxSsrc,
                pRtcMediaStreamTrack->trackId);
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

    STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "ice-options");
    STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue, "trickle");
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

    if (pKvsPeerConnection->isOffer) {
        switch (pKvsRtpTransceiver->transceiver.direction) {
            case RTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV:
                STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "sendrecv");
                break;
            case RTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY:
                STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "sendonly");
                break;
            case RTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY:
                STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "recvonly");
                break;
            default:
                // https://www.w3.org/TR/webrtc/#dom-rtcrtptransceiverdirection
                DLOGW("Incorrect/no transceiver direction set...this attribute will be set to inactive");
                STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "inactive");
        }
    } else {
        pSdpMediaDescriptionRemote = &pRemoteSessionDescription->mediaDescriptions[mediaSectionId];
        remoteAttributeCount = pSdpMediaDescriptionRemote->mediaAttributesCount;

        for (i = 0; i < remoteAttributeCount && directionFound == FALSE; i++) {
            if (STRCMP(pSdpMediaDescriptionRemote->sdpAttributes[i].attributeName, "sendrecv") == 0) {
                STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "sendrecv");
                directionFound = TRUE;
            } else if (STRCMP(pSdpMediaDescriptionRemote->sdpAttributes[i].attributeName, "recvonly") == 0) {
                STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "sendonly");
                directionFound = TRUE;
            } else if (STRCMP(pSdpMediaDescriptionRemote->sdpAttributes[i].attributeName, "sendonly") == 0) {
                STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "recvonly");
                directionFound = TRUE;
            }
        }
    }

    attributeCount++;

    STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "rtcp-mux");
    attributeCount++;

    STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "rtcp-rsize");
    attributeCount++;

    if (pRtcMediaStreamTrack->codec == RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE) {
        if (pKvsPeerConnection->isOffer) {
            currentFmtp = DEFAULT_H264_FMTP;
        }
        STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "rtpmap");
        SPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue, "%" PRId64 " H264/90000", payloadType);
        attributeCount++;

        // TODO: If level asymmetry is allowed, consider sending back DEFAULT_H264_FMTP instead of the received fmtp value.
        if (currentFmtp != NULL) {
            STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "fmtp");
            SPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue, "%" PRId64 " %s", payloadType, currentFmtp);
            attributeCount++;
        }

        if (containRtx) {
            STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "rtpmap");
            SPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue, "%" PRId64 " " RTX_VALUE, rtxPayloadType);
            attributeCount++;

            STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "fmtp");
            SPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue, "%" PRId64 " apt=%" PRId64 "", rtxPayloadType, payloadType);
            attributeCount++;
        }
    } else if (pRtcMediaStreamTrack->codec == RTC_CODEC_OPUS) {
        if (pKvsPeerConnection->isOffer) {
            currentFmtp = DEFAULT_OPUS_FMTP;
        }
        STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "rtpmap");
        SPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue, "%" PRId64 " opus/48000/2", payloadType);
        attributeCount++;

        if (currentFmtp != NULL) {
            STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "fmtp");
            SPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue, "%" PRId64 " %s", payloadType, currentFmtp);
            attributeCount++;
        }
    } else if (pRtcMediaStreamTrack->codec == RTC_CODEC_VP8) {
        STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "rtpmap");
        SPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue, "%" PRId64 " " VP8_VALUE, payloadType);
        attributeCount++;

        if (containRtx) {
            CHK_STATUS(hashTableGet(pKvsPeerConnection->pRtxTable, RTC_RTX_CODEC_VP8, &rtxPayloadType));
            STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "rtpmap");
            SPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue, "%" PRId64 " " RTX_VALUE, rtxPayloadType);
            attributeCount++;

            STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "fmtp");
            SPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue, "%" PRId64 " apt=%" PRId64 "", rtxPayloadType, payloadType);
            attributeCount++;
        }
    } else if (pRtcMediaStreamTrack->codec == RTC_CODEC_MULAW) {
        STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "rtpmap");
        SPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue, "%" PRId64 " " MULAW_VALUE, payloadType);
        attributeCount++;
    } else if (pRtcMediaStreamTrack->codec == RTC_CODEC_ALAW) {
        STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "rtpmap");
        SPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue, "%" PRId64 " " ALAW_VALUE, payloadType);
        attributeCount++;
    }

    STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "rtcp-fb");
    SPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue, "%" PRId64 " nack", payloadType);
    attributeCount++;

    if (pKvsPeerConnection->twccExtId != 0) {
        STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "rtcp-fb");
        SPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue, "%" PRId64 " " TWCC_SDP_ATTR, payloadType);
        attributeCount++;
    }

    pSdpMediaDescription->mediaAttributesCount = attributeCount;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS populateSessionDescriptionDataChannel(PKvsPeerConnection pKvsPeerConnection, PSdpMediaDescription pSdpMediaDescription,
                                             PCHAR pCertificateFingerprint, UINT32 mediaSectionId, PCHAR pDtlsRole)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 attributeCount = 0;

    SPRINTF(pSdpMediaDescription->mediaName, "application 9 UDP/DTLS/SCTP webrtc-datachannel");

    CHK_STATUS(iceAgentPopulateSdpMediaDescriptionCandidates(pKvsPeerConnection->pIceAgent, pSdpMediaDescription, MAX_SDP_ATTRIBUTE_VALUE_LENGTH,
                                                             &attributeCount));

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

BOOL isPresentInRemote(PKvsRtpTransceiver pKvsRtpTransceiver, PSessionDescription pRemoteSessionDescription)
{
    PCHAR remoteAttributeValue, end;
    UINT32 remoteTokenLen, i;
    PSdpMediaDescription pRemoteMediaDescription;
    MEDIA_STREAM_TRACK_KIND localTrackKind = pKvsRtpTransceiver->sender.track.kind;
    BOOL wasFound = FALSE;

    for (i = 0; i < pRemoteSessionDescription->mediaCount && wasFound == FALSE; i++) {
        pRemoteMediaDescription = &pRemoteSessionDescription->mediaDescriptions[i];
        remoteAttributeValue = pRemoteMediaDescription->mediaName;

        if ((end = STRCHR(remoteAttributeValue, ' ')) != NULL) {
            remoteTokenLen = (end - remoteAttributeValue);
        } else {
            remoteTokenLen = STRLEN(remoteAttributeValue);
        }

        switch (localTrackKind) {
            case MEDIA_STREAM_TRACK_KIND_AUDIO:
                if (remoteTokenLen == (ARRAY_SIZE(MEDIA_SECTION_AUDIO_VALUE) - 1) &&
                    STRNCMP(MEDIA_SECTION_AUDIO_VALUE, remoteAttributeValue, remoteTokenLen) == 0) {
                    wasFound = TRUE;
                }
                break;
            case MEDIA_STREAM_TRACK_KIND_VIDEO:
                if (remoteTokenLen == (ARRAY_SIZE(MEDIA_SECTION_VIDEO_VALUE) - 1) &&
                    STRNCMP(MEDIA_SECTION_VIDEO_VALUE, remoteAttributeValue, remoteTokenLen) == 0) {
                    wasFound = TRUE;
                }
                break;
            default:
                DLOGW("Unknown track kind:  %d", localTrackKind);
        }
    }

    return wasFound;
}

// Populate the media sections of a SessionDescription with the current state of the KvsPeerConnection
STATUS populateSessionDescriptionMedia(PKvsPeerConnection pKvsPeerConnection, PSessionDescription pRemoteSessionDescription,
                                       PSessionDescription pLocalSessionDescription)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PDoubleListNode pCurNode = NULL;
    CHAR certificateFingerprint[CERTIFICATE_FINGERPRINT_LENGTH];
    UINT64 data;
    PKvsRtpTransceiver pKvsRtpTransceiver;
    PCHAR pDtlsRole = NULL;

    CHK_STATUS(dtlsSessionGetLocalCertificateFingerprint(pKvsPeerConnection->pDtlsSession, certificateFingerprint, CERTIFICATE_FINGERPRINT_LENGTH));

    if (pKvsPeerConnection->isOffer) {
        pDtlsRole = DTLS_ROLE_ACTPASS;
    } else {
        pDtlsRole = DTLS_ROLE_ACTIVE;
        CHK_STATUS(reorderTransceiverByRemoteDescription(pKvsPeerConnection, pRemoteSessionDescription));
    }

    CHK_STATUS(doubleListGetHeadNode(pKvsPeerConnection->pTransceivers, &pCurNode));
    while (pCurNode != NULL) {
        CHK_STATUS(doubleListGetNodeData(pCurNode, &data));
        pCurNode = pCurNode->pNext;
        pKvsRtpTransceiver = (PKvsRtpTransceiver) data;
        if (pKvsRtpTransceiver != NULL) {
            CHK(pLocalSessionDescription->mediaCount < MAX_SDP_SESSION_MEDIA_COUNT, STATUS_SESSION_DESCRIPTION_MAX_MEDIA_COUNT);

            // If generating answer, need to check if Local Description is present in remote -- if not, we don't need to create a local description
            // for it or else our Answer will have an extra m-line, for offer the local is the offer itself, don't care about remote
            if (pKvsPeerConnection->isOffer || isPresentInRemote(pKvsRtpTransceiver, pRemoteSessionDescription)) {
                CHK_STATUS(populateSingleMediaSection(
                    pKvsPeerConnection, pKvsRtpTransceiver, &(pLocalSessionDescription->mediaDescriptions[pLocalSessionDescription->mediaCount]),
                    pRemoteSessionDescription, certificateFingerprint, pLocalSessionDescription->mediaCount, pDtlsRole));
                pLocalSessionDescription->mediaCount++;
            }
        }
    }

    if (pKvsPeerConnection->sctpIsEnabled) {
        CHK(pLocalSessionDescription->mediaCount < MAX_SDP_SESSION_MEDIA_COUNT, STATUS_SESSION_DESCRIPTION_MAX_MEDIA_COUNT);
        CHK_STATUS(populateSessionDescriptionDataChannel(pKvsPeerConnection,
                                                         &(pLocalSessionDescription->mediaDescriptions[pLocalSessionDescription->mediaCount]),
                                                         certificateFingerprint, pLocalSessionDescription->mediaCount, pDtlsRole));
        pLocalSessionDescription->mediaCount++;
    }

CleanUp:

    LEAVES();
    return retStatus;
}

// Populate a SessionDescription with the current state of the KvsPeerConnection
STATUS populateSessionDescription(PKvsPeerConnection pKvsPeerConnection, PSessionDescription pRemoteSessionDescription,
                                  PSessionDescription pLocalSessionDescription)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    CHAR bundleValue[MAX_SDP_ATTRIBUTE_VALUE_LENGTH], wmsValue[MAX_SDP_ATTRIBUTE_VALUE_LENGTH];
    PCHAR curr = NULL;
    UINT32 i, sizeRemaining;
    INT32 charsCopied;

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
    for (curr = (pLocalSessionDescription->sdpAttributes[0].attributeValue + ARRAY_SIZE(BUNDLE_KEY) - 1), i = 0;
         i < pLocalSessionDescription->mediaCount; i++) {
        STRCPY(pLocalSessionDescription->mediaDescriptions[i].sdpConnectionInformation.networkType, "IN");
        STRCPY(pLocalSessionDescription->mediaDescriptions[i].sdpConnectionInformation.addressType, "IP4");
        STRCPY(pLocalSessionDescription->mediaDescriptions[i].sdpConnectionInformation.connectionAddress, "127.0.0.1");

        sizeRemaining = MAX_SDP_ATTRIBUTE_VALUE_LENGTH - (curr - pLocalSessionDescription->sdpAttributes[0].attributeValue);
        charsCopied = SNPRINTF(curr, sizeRemaining, " %d", i);

        CHK(charsCopied > 0 && (UINT32) charsCopied < sizeRemaining, STATUS_BUFFER_TOO_SMALL);

        curr += charsCopied;
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
// Find a Transceiver with n codec, and then copy it to the end of the transceivers
// this allows us to re-order by the order the remote dictates
STATUS copyTransceiverWithCodec(PKvsPeerConnection pKvsPeerConnection, RTC_CODEC rtcCodec, PBOOL pDidFindCodec)
{
    STATUS retStatus = STATUS_SUCCESS;
    PDoubleListNode pCurNode = NULL;
    PKvsRtpTransceiver pTargetKvsRtpTransceiver = NULL, pKvsRtpTransceiver;
    UINT64 data;

    CHK(pKvsPeerConnection != NULL && pDidFindCodec != NULL, STATUS_NULL_ARG);

    *pDidFindCodec = FALSE;

    CHK_STATUS(doubleListGetHeadNode(pKvsPeerConnection->pTransceivers, &pCurNode));
    while (pCurNode != NULL) {
        CHK_STATUS(doubleListGetNodeData(pCurNode, &data));
        pKvsRtpTransceiver = (PKvsRtpTransceiver) data;
        if (pKvsRtpTransceiver != NULL && pKvsRtpTransceiver->sender.track.codec == rtcCodec) {
            pTargetKvsRtpTransceiver = pKvsRtpTransceiver;
            doubleListDeleteNode(pKvsPeerConnection->pTransceivers, pCurNode);
            break;
        }
        pCurNode = pCurNode->pNext;
    }
    if (pTargetKvsRtpTransceiver != NULL) {
        CHK_STATUS(doubleListInsertItemTail(pKvsPeerConnection->pTransceivers, (UINT64) pTargetKvsRtpTransceiver));
        *pDidFindCodec = TRUE;
    }

CleanUp:

    return retStatus;
}

STATUS reorderTransceiverByRemoteDescription(PKvsPeerConnection pKvsPeerConnection, PSessionDescription pRemoteSessionDescription)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 currentMedia, currentAttribute, transceiverCount = 0, tokenLen;
    PSdpMediaDescription pMediaDescription = NULL;
    PCHAR attributeValue, end;
    BOOL supportCodec, foundMediaSectionWithCodec;
    RTC_CODEC rtcCodec;

    // change the order of pKvsPeerConnection->pTransceivers to have the same codec order in pRemoteSessionDescription
    CHK_STATUS(doubleListGetNodeCount(pKvsPeerConnection->pTransceivers, &transceiverCount));

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

            // find transceiver with rtcCodec and duplicate it at tail
            if (supportCodec) {
                CHK_STATUS(copyTransceiverWithCodec(pKvsPeerConnection, rtcCodec, &foundMediaSectionWithCodec));
            }
            if (end != NULL) {
                attributeValue = end + 1;
            }
        } while (end != NULL && !foundMediaSectionWithCodec);

        // Scan the media section attributes for codecs we support
        for (currentAttribute = 0; currentAttribute < pMediaDescription->mediaAttributesCount && !foundMediaSectionWithCodec; currentAttribute++) {
            attributeValue = pMediaDescription->sdpAttributes[currentAttribute].attributeValue;

            if (STRSTR(attributeValue, H264_VALUE) != NULL) {
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

            // find transceiver with rtcCodec and duplicate it at tail
            if (supportCodec) {
                CHK_STATUS(copyTransceiverWithCodec(pKvsPeerConnection, rtcCodec, &foundMediaSectionWithCodec));
            }
        }
    }

CleanUp:

    CHK_LOG_ERR(retStatus);

    LEAVES();
    return retStatus;
}

STATUS deserializeRtcIceCandidateInit(PCHAR pJson, UINT32 jsonLen, PRtcIceCandidateInit pRtcIceCandidateInit)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    jsmntok_t tokens[MAX_JSON_TOKEN_COUNT];
    jsmn_parser parser;
    INT32 i, tokenCount;

    CHK(pRtcIceCandidateInit != NULL && pJson != NULL, STATUS_NULL_ARG);
    MEMSET(pRtcIceCandidateInit->candidate, 0x00, MAX_ICE_CANDIDATE_INIT_CANDIDATE_LEN + 1);

    jsmn_init(&parser);

    tokenCount = jsmn_parse(&parser, pJson, jsonLen, tokens, ARRAY_SIZE(tokens));
    CHK(tokenCount > 1, STATUS_INVALID_API_CALL_RETURN_JSON);
    CHK(tokens[0].type == JSMN_OBJECT, STATUS_ICE_CANDIDATE_INIT_MALFORMED);

    for (i = 1; i < (tokenCount - 1); i += 2) {
        if (STRNCMP(CANDIDATE_KEY, pJson + tokens[i].start, ARRAY_SIZE(CANDIDATE_KEY) - 1) == 0) {
            STRNCPY(pRtcIceCandidateInit->candidate, pJson + tokens[i + 1].start, (tokens[i + 1].end - tokens[i + 1].start));
        }
    }

    CHK(pRtcIceCandidateInit->candidate[0] != '\0', STATUS_ICE_CANDIDATE_MISSING_CANDIDATE);

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS setReceiversSsrc(PSessionDescription pRemoteSessionDescription, PDoubleList pTransceivers)
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
                if (STRNCMP(pMediaDescription->sdpAttributes[currentAttribute].attributeName, SSRC_KEY,
                            STRLEN(pMediaDescription->sdpAttributes[currentAttribute].attributeName)) == 0) {
                    if ((end = STRCHR(pMediaDescription->sdpAttributes[currentAttribute].attributeValue, ' ')) != NULL) {
                        CHK_STATUS(STRTOUI32(pMediaDescription->sdpAttributes[currentAttribute].attributeValue, end, 10, &ssrc));
                        foundSsrc = TRUE;
                    }
                }
            }

            if (foundSsrc) {
                CHK_STATUS(doubleListGetHeadNode(pTransceivers, &pCurNode));
                while (pCurNode != NULL) {
                    CHK_STATUS(doubleListGetNodeData(pCurNode, &data));
                    pKvsRtpTransceiver = (PKvsRtpTransceiver) data;
                    codec = pKvsRtpTransceiver->sender.track.codec;
                    isVideoCodec = (codec == RTC_CODEC_VP8 || codec == RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE);
                    isAudioCodec = (codec == RTC_CODEC_MULAW || codec == RTC_CODEC_ALAW || codec == RTC_CODEC_OPUS);

                    if (pKvsRtpTransceiver->jitterBufferSsrc == 0 &&
                        ((isVideoCodec && isVideoMediaSection) || (isAudioCodec && isAudioMediaSection))) {
                        // Finish iteration, we assigned the ssrc move on to next media section
                        pKvsRtpTransceiver->jitterBufferSsrc = ssrc;
                        pKvsRtpTransceiver->inboundStats.received.rtpStream.ssrc = ssrc;
                        STRNCPY(pKvsRtpTransceiver->inboundStats.received.rtpStream.kind,
                                pKvsRtpTransceiver->transceiver.receiver.track.kind == MEDIA_STREAM_TRACK_KIND_VIDEO ? "video" : "audio",
                                ARRAY_SIZE(pKvsRtpTransceiver->inboundStats.received.rtpStream.kind));

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
