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

    while ((next = STRNCHR(curr, (UINT32) (tail - curr), '\n')) != NULL) {
        lineLen = (UINT32) (next - curr);

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
                lineLen = (INT32) (next - curr);

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
    CHK_STATUS(hashTableUpsert(codecTable, RTC_CODEC_H265, DEFAULT_PAYLOAD_H265));

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
    UINT8 currentAttribute;
    UINT16 currentMedia;
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
            CHK_STATUS(hashTableContains(codecTable, RTC_CODEC_H265, &supportCodec));
            if (supportCodec && (end = STRSTR(attributeValue, H265_VALUE)) != NULL) {
                CHK_STATUS(STRTOUI64(attributeValue, end - 1, 10, &parsedPayloadType));
                DLOGV("Found H265 payload type %" PRId64 ".", parsedPayloadType);
                CHK_STATUS(hashTableUpsert(codecTable, RTC_CODEC_H265, parsedPayloadType));
            }
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
                    aptFmtpVals[aptFmtpValCount++] = (UINT32) ((fmtpVal << 8u) & parsedPayloadType);
                }
            }
        }

        for (i = 0; i < aptFmtpValCount; i++) {
            aptFmtVal = aptFmtpVals[i];
            fmtpVal = aptFmtVal >> 8u;
            aptVal = aptFmtVal & 0xFFu;

            CHK_STATUS(hashTableContains(codecTable, RTC_CODEC_H265, &supportCodec));
            if (supportCodec) {
                CHK_STATUS(hashTableGet(codecTable, RTC_CODEC_H265, &hashmapPayloadType));
                if (aptVal == hashmapPayloadType) {
                    CHK_STATUS(hashTableUpsert(rtxTable, RTC_RTX_CODEC_H265, fmtpVal));
                    DLOGV("h265 found apt type %" PRId64 " for fmtp %" PRId64, aptVal, fmtpVal);
                }
            }

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
    INT32 amountWritten = 0;

    MEMSET(payloadStr, 0x00, MAX_SDP_ATTRIBUTE_VALUE_LENGTH);
    amountWritten = SNPRINTF(payloadStr, SIZEOF(payloadStr), "%" PRId64, payloadType);

    if (amountWritten < 0) {
        DLOGE("Internal error: Full payload type for fmtp could not be written");
    } else {
        for (currentMedia = 0; currentMedia < pSessionDescription->mediaCount; currentMedia++) {
            pMediaDescription = &(pSessionDescription->mediaDescriptions[currentMedia]);
            for (currentAttribute = 0; currentAttribute < pMediaDescription->mediaAttributesCount; currentAttribute++) {
                if (STRCMP(pMediaDescription->sdpAttributes[currentAttribute].attributeName, "fmtp") == 0 &&
                    STRNCMP(pMediaDescription->sdpAttributes[currentAttribute].attributeValue, payloadStr, STRLEN(payloadStr)) == 0) {
                    return pMediaDescription->sdpAttributes[currentAttribute].attributeValue + STRLEN(payloadStr) + 1;
                }
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
                                  PCHAR pCertificateFingerprint, UINT32 mediaSectionId, PCHAR pDtlsRole, PHashTable pUnknownCodecPayloadTypesTable,
                                  PHashTable pUnknownCodecRtpmapTable, UINT32 unknownCodecHashTableKey)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT64 payloadType, rtxPayloadType;
    BOOL containRtx = FALSE;
    BOOL directionFound = FALSE;
    UINT32 i, remoteAttributeCount, attributeCount = 0;
    PRtcMediaStreamTrack pRtcMediaStreamTrack = &(pKvsRtpTransceiver->sender.track);
    PSdpMediaDescription pSdpMediaDescriptionRemote;
    PCHAR currentFmtp = NULL, rtpMapValue = NULL;
    CHAR remoteSdpAttributeValue[MAX_SDP_ATTRIBUTE_VALUE_LENGTH];
    INT32 amountWritten = 0;

    MEMSET(remoteSdpAttributeValue, 0, MAX_SDP_ATTRIBUTE_VALUE_LENGTH);

    if (pRtcMediaStreamTrack->codec == RTC_CODEC_UNKNOWN && pUnknownCodecPayloadTypesTable != NULL) {
        CHK_STATUS(hashTableGet(pUnknownCodecPayloadTypesTable, unknownCodecHashTableKey, &payloadType));
    } else {
        CHK_STATUS(hashTableGet(pKvsPeerConnection->pCodecTable, pRtcMediaStreamTrack->codec, &payloadType));
        currentFmtp = fmtpForPayloadType(payloadType, &(pKvsPeerConnection->remoteSessionDescription));
    }

    if (pRtcMediaStreamTrack->kind == MEDIA_STREAM_TRACK_KIND_VIDEO) {
        if (pRtcMediaStreamTrack->codec == RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE) {
            retStatus = hashTableGet(pKvsPeerConnection->pRtxTable, RTC_RTX_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE,
                                     &rtxPayloadType);
        } else if (pRtcMediaStreamTrack->codec == RTC_CODEC_VP8) {
            retStatus = hashTableGet(pKvsPeerConnection->pRtxTable, RTC_RTX_CODEC_VP8, &rtxPayloadType);
        } else if (pRtcMediaStreamTrack->codec == RTC_CODEC_H265) {
            retStatus = hashTableGet(pKvsPeerConnection->pRtxTable, RTC_RTX_CODEC_H265, &rtxPayloadType);
            payloadType = DEFAULT_PAYLOAD_H265;
        } else {
            retStatus = STATUS_HASH_KEY_NOT_PRESENT;
        }
        CHK(retStatus == STATUS_SUCCESS || retStatus == STATUS_HASH_KEY_NOT_PRESENT, retStatus);
        containRtx = (retStatus == STATUS_SUCCESS);
        retStatus = STATUS_SUCCESS;
        if (containRtx) {
            amountWritten = SNPRINTF(pSdpMediaDescription->mediaName, SIZEOF(pSdpMediaDescription->mediaName),
                                     "video 9 UDP/TLS/RTP/SAVPF %" PRId64 " %" PRId64, payloadType, rtxPayloadType);
            CHK_ERR(amountWritten > 0, STATUS_INTERNAL_ERROR, "Full video (with rtx) media name attribute could not be written");
        } else {
            amountWritten =
                SNPRINTF(pSdpMediaDescription->mediaName, SIZEOF(pSdpMediaDescription->mediaName), "video 9 UDP/TLS/RTP/SAVPF %" PRId64, payloadType);
            CHK_ERR(amountWritten > 0, STATUS_INTERNAL_ERROR, "Full video media name attribute could not be written");
        }
    } else if (pRtcMediaStreamTrack->kind == MEDIA_STREAM_TRACK_KIND_AUDIO) {
        amountWritten =
            SNPRINTF(pSdpMediaDescription->mediaName, SIZEOF(pSdpMediaDescription->mediaName), "audio 9 UDP/TLS/RTP/SAVPF %" PRId64, payloadType);
        CHK_ERR(amountWritten > 0, STATUS_INTERNAL_ERROR, "Full audio media name attribute could not be written");
    }

    CHK_STATUS(iceAgentPopulateSdpMediaDescriptionCandidates(pKvsPeerConnection->pIceAgent, pSdpMediaDescription, MAX_SDP_ATTRIBUTE_VALUE_LENGTH,
                                                             &attributeCount));

    if (containRtx) {
        STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "msid");
        amountWritten = SNPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue,
                                 SIZEOF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue), "%s %sRTX",
                                 pRtcMediaStreamTrack->streamId, pRtcMediaStreamTrack->trackId);
        CHK_ERR(amountWritten > 0, STATUS_INTERNAL_ERROR, "Full msid value (with rtx) could not be written");
        attributeCount++;

        STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "ssrc-group");
        amountWritten = SNPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue,
                                 SIZEOF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue), "FID %u %u",
                                 pKvsRtpTransceiver->sender.ssrc, pKvsRtpTransceiver->sender.rtxSsrc);
        CHK_ERR(amountWritten > 0, STATUS_INTERNAL_ERROR, "Full ssrc-grp value (with rtx) could not be written");
        attributeCount++;
    } else {
        STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "msid");
        amountWritten = SNPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue,
                                 SIZEOF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue), "%s %s", pRtcMediaStreamTrack->streamId,
                                 pRtcMediaStreamTrack->trackId);
        CHK_ERR(amountWritten > 0, STATUS_INTERNAL_ERROR, "Full msid value could not be written");
        attributeCount++;
    }

    STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "ssrc");
    amountWritten = SNPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue,
                             SIZEOF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue), "%u cname:%s",
                             pKvsRtpTransceiver->sender.ssrc, pKvsPeerConnection->localCNAME);
    CHK_ERR(amountWritten > 0, STATUS_INTERNAL_ERROR, "Full ssrc cname could not be written");
    attributeCount++;

    STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "ssrc");
    amountWritten = SNPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue,
                             SIZEOF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue), "%u msid:%s %s",
                             pKvsRtpTransceiver->sender.ssrc, pRtcMediaStreamTrack->streamId, pRtcMediaStreamTrack->trackId);
    CHK_ERR(amountWritten > 0, STATUS_INTERNAL_ERROR, "Full ssrc msid could not be written");
    attributeCount++;

    STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "ssrc");
    amountWritten = SNPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue,
                             SIZEOF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue), "%u mslabel:%s",
                             pKvsRtpTransceiver->sender.ssrc, pRtcMediaStreamTrack->streamId);
    CHK_ERR(amountWritten > 0, STATUS_INTERNAL_ERROR, "Full ssrc mslabel could not be written");
    attributeCount++;

    STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "ssrc");
    amountWritten = SNPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue,
                             SIZEOF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue), "%u label:%s",
                             pKvsRtpTransceiver->sender.ssrc, pRtcMediaStreamTrack->trackId);
    CHK_ERR(amountWritten > 0, STATUS_INTERNAL_ERROR, "Full ssrc label could not be written");
    attributeCount++;

    if (containRtx) {
        STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "ssrc");
        amountWritten = SNPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue,
                                 SIZEOF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue), "%u cname:%s",
                                 pKvsRtpTransceiver->sender.rtxSsrc, pKvsPeerConnection->localCNAME);
        CHK_ERR(amountWritten > 0, STATUS_INTERNAL_ERROR, "Full ssrc cname (with rtx) could not be written");
        attributeCount++;

        STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "ssrc");
        amountWritten = SNPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue,
                                 SIZEOF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue), "%u msid:%s %sRTX",
                                 pKvsRtpTransceiver->sender.rtxSsrc, pRtcMediaStreamTrack->streamId, pRtcMediaStreamTrack->trackId);
        CHK_ERR(amountWritten > 0, STATUS_INTERNAL_ERROR, "Full ssrc msid (with rtx) could not be written");
        attributeCount++;

        STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "ssrc");
        amountWritten = SNPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue,
                                 SIZEOF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue), "%u mslabel:%sRTX",
                                 pKvsRtpTransceiver->sender.rtxSsrc, pRtcMediaStreamTrack->streamId);
        CHK_ERR(amountWritten > 0, STATUS_INTERNAL_ERROR, "Full ssrc mslabel (with rtx) could not be written");
        attributeCount++;

        STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "ssrc");
        amountWritten = SNPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue,
                                 SIZEOF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue), "%u label:%sRTX",
                                 pKvsRtpTransceiver->sender.rtxSsrc, pRtcMediaStreamTrack->trackId);
        CHK_ERR(amountWritten > 0, STATUS_INTERNAL_ERROR, "Full ssrc label (with rtx) could not be written");
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

    if (pKvsPeerConnection->canTrickleIce.value) {
        STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "ice-options");
        STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue, "trickle");
        attributeCount++;
    }

    STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "fingerprint");
    STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue, "sha-256 ");
    STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue + 8, pCertificateFingerprint);
    attributeCount++;

    STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "setup");
    STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue, pDtlsRole);
    attributeCount++;

    STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "mid");
    // check all session attribute lines to see if a line with mid is present. If it is present, copy its content and break
    for (i = 0; i < pRemoteSessionDescription->mediaDescriptions[mediaSectionId].mediaAttributesCount; i++) {
        if (STRCMP(pRemoteSessionDescription->mediaDescriptions[mediaSectionId].sdpAttributes[i].attributeName, MID_KEY) == 0) {
            STRCPY(remoteSdpAttributeValue, pRemoteSessionDescription->mediaDescriptions[mediaSectionId].sdpAttributes[i].attributeValue);
            break;
        }
    }

    // check if we already have a value for the "mid" session attribute from remote description. If we have it, we use it.
    // If we don't have it, we loop over, create and add them
    if (STRLEN(remoteSdpAttributeValue) > 0) {
        CHK(STRLEN(remoteSdpAttributeValue) < MAX_SDP_ATTRIBUTE_VALUE_LENGTH, STATUS_BUFFER_TOO_SMALL);
        amountWritten = SNPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue,
                                 SIZEOF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue), "%s", remoteSdpAttributeValue);
        CHK_ERR(amountWritten > 0, STATUS_INTERNAL_ERROR, "Mid exists, but remote SDP value could not be written");
    } else {
        amountWritten = SNPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue,
                                 SIZEOF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue), "%d", mediaSectionId);
        CHK_ERR(amountWritten > 0, STATUS_INTERNAL_ERROR, "Full media section Id could not be written");
    }
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

        // in case of a missing m-line, we respond with the same m-line but direction set to inactive
        if (pKvsRtpTransceiver->transceiver.direction == RTC_RTP_TRANSCEIVER_DIRECTION_INACTIVE) {
            STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "inactive");
            directionFound = TRUE;
        }
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

    if (mediaSectionId != 0) {
        STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "rtcp-rsize");
        attributeCount++;
    }

    if (pRtcMediaStreamTrack->codec == RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE) {
        // TODO: Need additional condition for a signaling channel with an ENABLED media storage configuration
        if (pKvsPeerConnection->isOffer) {
            currentFmtp = DEFAULT_H264_FMTP;
        }
        STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "rtpmap");
        amountWritten = SNPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue,
                                 SIZEOF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue), "%" PRId64 " H264/90000", payloadType);
        CHK_ERR(amountWritten > 0, STATUS_INTERNAL_ERROR, "Full H264 payload type could not be written");
        attributeCount++;

        STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "rtcp-fb");
        amountWritten = SNPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue,
                                 SIZEOF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue), "%" PRId64 " nack", payloadType);
        CHK_ERR(amountWritten > 0, STATUS_INTERNAL_ERROR, "Full H264 rtcp-fb nack value could not be written");
        amountWritten = SNPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue,
                                 SIZEOF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue), "%" PRId64 " nack pli", payloadType);
        CHK_ERR(amountWritten > 0, STATUS_INTERNAL_ERROR, "Full H264 rtcp-fb nack-pli value could not be written");
        attributeCount++;

        STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "rtcp-fb");
        SPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue, "%" PRId64 " nack", payloadType);
        SPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue, "%" PRId64 " nack pli", payloadType);
        attributeCount++;

        STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "rtcp-fb");
        SPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue, "%" PRId64 " nack", payloadType);
        SPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue, "%" PRId64 " nack pli", payloadType);
        attributeCount++;

        // TODO: If level asymmetry is allowed, consider sending back DEFAULT_H264_FMTP instead of the received fmtp value.
        if (currentFmtp != NULL) {
            STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "fmtp");
            amountWritten =
                SNPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue,
                         SIZEOF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue), "%" PRId64 " %s", payloadType, currentFmtp);
            CHK_ERR(amountWritten > 0, STATUS_INTERNAL_ERROR, "Full H264 fmtp value could not be written");
            attributeCount++;
        }

        if (containRtx) {
            STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "rtpmap");
            amountWritten =
                SNPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue,
                         SIZEOF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue), "%" PRId64 " " RTX_VALUE, rtxPayloadType);
            CHK_ERR(amountWritten > 0, STATUS_INTERNAL_ERROR, "Full H264 rtpmap (with rtx) could not be written");
            attributeCount++;

            STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "fmtp");
            amountWritten = SNPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue,
                                     SIZEOF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue), "%" PRId64 " apt=%" PRId64 "",
                                     rtxPayloadType, payloadType);
            CHK_ERR(amountWritten > 0, STATUS_INTERNAL_ERROR, "Full H264 fmtp apt value (with rtx) could not be written");
            attributeCount++;
        }
    } else if (pRtcMediaStreamTrack->codec == RTC_CODEC_OPUS) {
        if (pKvsPeerConnection->isOffer) {
            currentFmtp = DEFAULT_OPUS_FMTP;
        }

        STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "rtpmap");
        amountWritten = SNPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue,
                                 SIZEOF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue), "%" PRId64 " opus/48000/2", payloadType);
        CHK_ERR(amountWritten > 0, STATUS_INTERNAL_ERROR, "Full Opus rtpmap could not be written");
        attributeCount++;

        if (currentFmtp != NULL) {
            STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "fmtp");
            amountWritten =
                SNPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue,
                         SIZEOF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue), "%" PRId64 " %s", payloadType, currentFmtp);
            CHK_ERR(amountWritten > 0, STATUS_INTERNAL_ERROR, "Full Opus fmtp could not be written");
            attributeCount++;
        }
    } else if (pRtcMediaStreamTrack->codec == RTC_CODEC_VP8) {
        STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "rtpmap");
        amountWritten = SNPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue,
                                 SIZEOF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue), "%" PRId64 " " VP8_VALUE, payloadType);
        CHK_ERR(amountWritten > 0, STATUS_INTERNAL_ERROR, "Full VP8 rtpmap could not be written");
        attributeCount++;

        if (containRtx) {
            CHK_STATUS(hashTableGet(pKvsPeerConnection->pRtxTable, RTC_RTX_CODEC_VP8, &rtxPayloadType));
            STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "rtpmap");
            amountWritten =
                SNPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue,
                         SIZEOF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue), "%" PRId64 " " RTX_VALUE, rtxPayloadType);
            CHK_ERR(amountWritten > 0, STATUS_INTERNAL_ERROR, "Full VP8 rtpmap payload type (with rtx) could not be written");
            attributeCount++;

            STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "fmtp");
            amountWritten = SNPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue,
                                     SIZEOF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue), "%" PRId64 " apt=%" PRId64 "",
                                     rtxPayloadType, payloadType);
            CHK_ERR(amountWritten > 0, STATUS_INTERNAL_ERROR, "Full VP8 rtpmap fmtp apt value (with rtx) could not be written");
            attributeCount++;
        }
    } else if (pRtcMediaStreamTrack->codec == RTC_CODEC_MULAW) {
        STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "rtpmap");
        amountWritten = SNPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue,
                                 SIZEOF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue), "%" PRId64 " " MULAW_VALUE, payloadType);
        CHK_ERR(amountWritten > 0, STATUS_INTERNAL_ERROR, "Full MULAW rtpmap could not be written");
        attributeCount++;
    } else if (pRtcMediaStreamTrack->codec == RTC_CODEC_ALAW) {
        STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "rtpmap");
        amountWritten = SNPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue,
                                 SIZEOF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue), "%" PRId64 " " ALAW_VALUE, payloadType);
        CHK_ERR(amountWritten > 0, STATUS_INTERNAL_ERROR, "Full ALAW rtpmap could not be written");
        attributeCount++;
    } else if (pRtcMediaStreamTrack->codec == RTC_CODEC_H265) {
        if (pKvsPeerConnection->isOffer) {
            currentFmtp = DEFAULT_H265_FMTP;
        }
        STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "rtpmap");
        amountWritten = SNPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue,
                                 SIZEOF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue), "%" PRId64 " H265/90000", payloadType);
        CHK_ERR(amountWritten > 0, STATUS_INTERNAL_ERROR, "Full H265 rtpmap could not be written");
        attributeCount++;

        STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "rtcp-fb");
        amountWritten = SNPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue,
                                 SIZEOF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue), "%" PRId64 " nack", payloadType);
        CHK_ERR(amountWritten > 0, STATUS_INTERNAL_ERROR, "Full H265 rtcp-fb nack value could not be written");
        amountWritten = SNPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue,
                                 SIZEOF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue), "%" PRId64 " nack pli", payloadType);
        CHK_ERR(amountWritten > 0, STATUS_INTERNAL_ERROR, "Full H265 rtcp-fb nack-pli value could not be written");
        attributeCount++;

        // TODO: If level asymmetry is allowed, consider sending back DEFAULT_H265_FMTP instead of the received fmtp value.
        if (currentFmtp != NULL) {
            STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "fmtp");
            amountWritten =
                SNPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue,
                         SIZEOF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue), "%" PRId64 " %s", payloadType, currentFmtp);
            CHK_ERR(amountWritten > 0, STATUS_INTERNAL_ERROR, "Full H265 fmtp value could not be written");
            attributeCount++;
        }

        if (containRtx) {
            STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "rtpmap");
            amountWritten =
                SNPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue,
                         SIZEOF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue), "%" PRId64 " " RTX_VALUE, rtxPayloadType);
            CHK_ERR(amountWritten > 0, STATUS_INTERNAL_ERROR, "Full H265 rtpmap (with rtx) could not be written");
            attributeCount++;

            STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "fmtp");
            amountWritten = SNPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue,
                                     SIZEOF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue), "%" PRId64 " apt=%" PRId64 "",
                                     rtxPayloadType, payloadType);
            CHK_ERR(amountWritten > 0, STATUS_INTERNAL_ERROR, "Full H265 fmtp apt value (with rtx) could not be written");
            attributeCount++;
        }
    } else if (pRtcMediaStreamTrack->codec == RTC_CODEC_UNKNOWN) {
        CHK_STATUS(hashTableGet(pUnknownCodecRtpmapTable, unknownCodecHashTableKey, (PUINT64) &rtpMapValue));
        STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "rtpmap");
        amountWritten =
            SNPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue,
                     SIZEOF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue), "%" PRId64 " %s", payloadType, rtpMapValue);
        CHK_ERR(amountWritten > 0, STATUS_INTERNAL_ERROR, "Full Unknown rtpmap could not be written");
        attributeCount++;
    }

    STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "ssrc");
    amountWritten = SNPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue,
                             SIZEOF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue), "%u cname:%s",
                             pKvsRtpTransceiver->sender.ssrc, pKvsPeerConnection->localCNAME);
    CHK_ERR(amountWritten > 0, STATUS_INTERNAL_ERROR, "Full transceiver ssrc cname could not be written");
    attributeCount++;

    STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "ssrc");
    amountWritten = SNPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue,
                             SIZEOF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue), "%u msid:%s %s",
                             pKvsRtpTransceiver->sender.ssrc, pRtcMediaStreamTrack->streamId, pRtcMediaStreamTrack->trackId);
    CHK_ERR(amountWritten > 0, STATUS_INTERNAL_ERROR, "Full transceiver ssrc msid could not be written");
    attributeCount++;

    STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "rtcp-fb");
    amountWritten = SNPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue,
                             SIZEOF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue), "%" PRId64 " goog-remb", payloadType);
    CHK_ERR(amountWritten > 0, STATUS_INTERNAL_ERROR, "Full rtcp-fb goog-remb could not be written");
    attributeCount++;

    if (pKvsPeerConnection->twccExtId != 0) {
        STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "rtcp-fb");
        amountWritten =
            SNPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue,
                     SIZEOF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue), "%" PRId64 " " TWCC_SDP_ATTR, payloadType);
        CHK_ERR(amountWritten > 0, STATUS_INTERNAL_ERROR, "Full rtcp-fb twcc could not be written");
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
    INT32 amountWritten = 0;

    amountWritten =
        SNPRINTF(pSdpMediaDescription->mediaName, SIZEOF(pSdpMediaDescription->mediaName), "application 9 UDP/DTLS/SCTP webrtc-datachannel");
    CHK_ERR(amountWritten > 0, STATUS_INTERNAL_ERROR, "Full data channel media name could not be written");

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

    if (pKvsPeerConnection->canTrickleIce.value) {
        STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "ice-options");
        STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue, "trickle");
        attributeCount++;
    }

    STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "fingerprint");
    STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue, "sha-256 ");
    STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue + 8, pCertificateFingerprint);
    attributeCount++;

    STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "setup");
    STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue, pDtlsRole);
    attributeCount++;

    STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "mid");
    amountWritten = SNPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue,
                             SIZEOF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue), "%d", mediaSectionId);
    CHK_ERR(amountWritten > 0, STATUS_INTERNAL_ERROR, "Full data channel mid media section could not be written");
    attributeCount++;

    STRCPY(pSdpMediaDescription->sdpAttributes[attributeCount].attributeName, "sctp-port");
    amountWritten = SNPRINTF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue,
                             SIZEOF(pSdpMediaDescription->sdpAttributes[attributeCount].attributeValue), "5000");
    CHK_ERR(amountWritten > 0, STATUS_INTERNAL_ERROR, "Full data channel sctp-port could not be written");
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
    PHashTable pUnknownCodecPayloadTypesTable = NULL, pUnknownCodecRtpmapTable = NULL;
    UINT32 unknownCodecHashTableKey = 0;

    CHK_STATUS(dtlsSessionGetLocalCertificateFingerprint(pKvsPeerConnection->pDtlsSession, certificateFingerprint, CERTIFICATE_FINGERPRINT_LENGTH));

    if (pKvsPeerConnection->isOffer) {
        pDtlsRole = DTLS_ROLE_ACTPASS;

        CHK_STATUS(doubleListGetHeadNode(pKvsPeerConnection->pTransceivers, &pCurNode));
        while (pCurNode != NULL) {
            CHK_STATUS(doubleListGetNodeData(pCurNode, &data));
            pCurNode = pCurNode->pNext;
            pKvsRtpTransceiver = (PKvsRtpTransceiver) data;
            if (pKvsRtpTransceiver != NULL) {
                CHK(pLocalSessionDescription->mediaCount < MAX_SDP_SESSION_MEDIA_COUNT, STATUS_SESSION_DESCRIPTION_MAX_MEDIA_COUNT);

                // If generating answer, need to check if Local Description is present in remote -- if not, we don't need to create a local
                // description for it or else our Answer will have an extra m-line, for offer the local is the offer itself, don't care about remote
                CHK_STATUS(populateSingleMediaSection(
                    pKvsPeerConnection, pKvsRtpTransceiver, &(pLocalSessionDescription->mediaDescriptions[pLocalSessionDescription->mediaCount]),
                    pRemoteSessionDescription, certificateFingerprint, pLocalSessionDescription->mediaCount, pDtlsRole, NULL, NULL, 0));
                pLocalSessionDescription->mediaCount++;
            }
        }
    } else {
        pDtlsRole = DTLS_ROLE_ACTIVE;
        CHK_STATUS(hashTableCreate(&pUnknownCodecPayloadTypesTable));
        CHK_STATUS(hashTableCreate(&pUnknownCodecRtpmapTable));

        // this function creates a list of transceivers corresponding to each m-line and adds it answerTransceivers
        // if an m-line does not have a corresponding transceiver created by the user, we create a fake transceiver
        CHK_STATUS(findTransceiversByRemoteDescription(pKvsPeerConnection, pRemoteSessionDescription, pUnknownCodecPayloadTypesTable,
                                                       pUnknownCodecRtpmapTable));

        // pAnswerTransceivers contains transceivers created by the user as well as fake transceivers
        CHK_STATUS(doubleListGetHeadNode(pKvsPeerConnection->pAnswerTransceivers, &pCurNode));
        while (pCurNode != NULL) {
            CHK_STATUS(doubleListGetNodeData(pCurNode, &data));
            pCurNode = pCurNode->pNext;
            pKvsRtpTransceiver = (PKvsRtpTransceiver) data;
            if (pKvsRtpTransceiver != NULL) {
                CHK(pLocalSessionDescription->mediaCount < MAX_SDP_SESSION_MEDIA_COUNT, STATUS_SESSION_DESCRIPTION_MAX_MEDIA_COUNT);
                if (isPresentInRemote(pKvsRtpTransceiver, pRemoteSessionDescription)) {
                    if (pKvsRtpTransceiver->sender.track.codec == RTC_CODEC_UNKNOWN) {
                        CHK_STATUS(populateSingleMediaSection(pKvsPeerConnection, pKvsRtpTransceiver,
                                                              &(pLocalSessionDescription->mediaDescriptions[pLocalSessionDescription->mediaCount]),
                                                              pRemoteSessionDescription, certificateFingerprint, pLocalSessionDescription->mediaCount,
                                                              pDtlsRole, pUnknownCodecPayloadTypesTable, pUnknownCodecRtpmapTable,
                                                              unknownCodecHashTableKey));
                        unknownCodecHashTableKey++;
                        // unknownCodecHashTableKey is the key for pUnknownCodecRtpmapTable and pUnknownCodecPayloadTypesTable
                        // a value for the same key in both hashtables corresponds to rtpmap and payloadtype for the same m-line / unknown codec

                    } else {
                        // in case of a user-added transceiver, the pUnknownCodecPayloadTypesTable, pUnknownCodecRtpmapTable are not populated by
                        // the function findTransceiversByRemoteDescription and are NULL
                        CHK_STATUS(populateSingleMediaSection(pKvsPeerConnection, pKvsRtpTransceiver,
                                                              &(pLocalSessionDescription->mediaDescriptions[pLocalSessionDescription->mediaCount]),
                                                              pRemoteSessionDescription, certificateFingerprint, pLocalSessionDescription->mediaCount,
                                                              pDtlsRole, NULL, NULL, 0));
                    }
                    pLocalSessionDescription->mediaCount++;
                }
            }
        }
    }

    if (ATOMIC_LOAD_BOOL(&pKvsPeerConnection->sctpIsEnabled)) {
        CHK(pLocalSessionDescription->mediaCount < MAX_SDP_SESSION_MEDIA_COUNT, STATUS_SESSION_DESCRIPTION_MAX_MEDIA_COUNT);
        CHK_STATUS(populateSessionDescriptionDataChannel(pKvsPeerConnection,
                                                         &(pLocalSessionDescription->mediaDescriptions[pLocalSessionDescription->mediaCount]),
                                                         certificateFingerprint, pLocalSessionDescription->mediaCount, pDtlsRole));
        pLocalSessionDescription->mediaCount++;
    }

CleanUp:

    if (pUnknownCodecPayloadTypesTable != NULL) {
        CHK_STATUS(hashTableFree(pUnknownCodecPayloadTypesTable));
    }
    if (pUnknownCodecRtpmapTable != NULL) {
        CHK_STATUS(hashTableFree(pUnknownCodecRtpmapTable));
    }

    LEAVES();
    return retStatus;
}

// Populate a SessionDescription with the current state of the KvsPeerConnection
STATUS populateSessionDescription(PKvsPeerConnection pKvsPeerConnection, PSessionDescription pRemoteSessionDescription,
                                  PSessionDescription pLocalSessionDescription)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    CHAR bundleValue[MAX_SDP_ATTRIBUTE_VALUE_LENGTH], wmsValue[MAX_SDP_ATTRIBUTE_VALUE_LENGTH],
        remoteSdpAttributeValue[MAX_SDP_ATTRIBUTE_VALUE_LENGTH];
    PCHAR curr = NULL;
    UINT32 i, sizeRemaining;
    INT32 charsCopied;

    CHK(pKvsPeerConnection != NULL && pLocalSessionDescription != NULL && pRemoteSessionDescription != NULL, STATUS_NULL_ARG);
    CHK_STATUS(populateSessionDescriptionMedia(pKvsPeerConnection, pRemoteSessionDescription, pLocalSessionDescription));
    MEMSET(bundleValue, 0, MAX_SDP_ATTRIBUTE_VALUE_LENGTH);
    MEMSET(wmsValue, 0, MAX_SDP_ATTRIBUTE_VALUE_LENGTH);
    MEMSET(remoteSdpAttributeValue, 0, MAX_SDP_ATTRIBUTE_VALUE_LENGTH);

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
    pLocalSessionDescription->sessionAttributesCount++;

    if (pKvsPeerConnection->canTrickleIce.value) {
        STRCPY(pLocalSessionDescription->sdpAttributes[pLocalSessionDescription->sessionAttributesCount].attributeName, "ice-options");
        STRCPY(pLocalSessionDescription->sdpAttributes[pLocalSessionDescription->sessionAttributesCount].attributeValue, "trickle");
        pLocalSessionDescription->sessionAttributesCount++;
    }

    // check all session attribute lines to see if a line with BUNDLE is present. If it is present, copy its content and break
    for (i = 0; i < pRemoteSessionDescription->sessionAttributesCount; i++) {
        if (STRSTR(pRemoteSessionDescription->sdpAttributes[i].attributeValue, BUNDLE_KEY) != NULL) {
            STRCPY(remoteSdpAttributeValue, pRemoteSessionDescription->sdpAttributes[i].attributeValue + ARRAY_SIZE(BUNDLE_KEY) - 1);
            break;
        }
    }

    // check if we already have a value for the "group" session attribute from remote description. If we have it, we use it.
    // If we don't have it, we loop over, create and add them
    if (STRLEN(remoteSdpAttributeValue) > 0) {
        CHK(STRLEN(remoteSdpAttributeValue) < MAX_SDP_ATTRIBUTE_VALUE_LENGTH, STATUS_BUFFER_TOO_SMALL);
        STRCAT(pLocalSessionDescription->sdpAttributes[0].attributeValue, remoteSdpAttributeValue);
    } else {
        for (curr = (pLocalSessionDescription->sdpAttributes[0].attributeValue + ARRAY_SIZE(BUNDLE_KEY) - 1), i = 0;
             i < pLocalSessionDescription->mediaCount; i++) {
            sizeRemaining = MAX_SDP_ATTRIBUTE_VALUE_LENGTH - (curr - pLocalSessionDescription->sdpAttributes[0].attributeValue);
            charsCopied = SNPRINTF(curr, sizeRemaining, " %d", i);

            CHK(charsCopied > 0 && (UINT32) charsCopied < sizeRemaining, STATUS_BUFFER_TOO_SMALL);

            curr += charsCopied;
        }
    }

    for (i = 0; i < pLocalSessionDescription->mediaCount; i++) {
        STRCPY(pLocalSessionDescription->mediaDescriptions[i].sdpConnectionInformation.networkType, "IN");
        STRCPY(pLocalSessionDescription->mediaDescriptions[i].sdpConnectionInformation.addressType, "IP4");
        STRCPY(pLocalSessionDescription->mediaDescriptions[i].sdpConnectionInformation.connectionAddress, "127.0.0.1");
    }

    STRCPY(pLocalSessionDescription->sdpAttributes[pLocalSessionDescription->sessionAttributesCount].attributeName, "msid-semantic");
    STRCPY(pLocalSessionDescription->sdpAttributes[pLocalSessionDescription->sessionAttributesCount].attributeValue, " WMS myKvsVideoStream");
    pLocalSessionDescription->sessionAttributesCount++;

CleanUp:

    LEAVES();
    return retStatus;
}

// primarily meant to be used by findTransceiversByRemoteDescription. This function checks if a codec is present in the user-created transceivers
STATUS findCodecInTransceivers(PKvsPeerConnection pKvsPeerConnection, RTC_CODEC rtcCodec, PBOOL pDidFindCodec, PHashTable pSeenTransceivers)
{
    STATUS retStatus = STATUS_SUCCESS;
    PDoubleListNode pCurNode = NULL;
    PKvsRtpTransceiver pKvsRtpTransceiver;
    UINT64 data;
    BOOL contains = FALSE;

    CHK_STATUS(doubleListGetHeadNode(pKvsPeerConnection->pTransceivers, &pCurNode));
    while (pCurNode != NULL) {
        CHK_STATUS(doubleListGetNodeData(pCurNode, &data));
        pKvsRtpTransceiver = (PKvsRtpTransceiver) data;
        // if we have already seen / added the transceiver to the answerTransceivers list, we do not want to add it again. This is to ensure that
        // we have one transceiver per m-line. In case of two video m-lines with the same codec, we need two different transceivers
        CHK_STATUS(hashTableContains(pSeenTransceivers, (UINT64) pKvsRtpTransceiver, &contains));
        if (pKvsRtpTransceiver != NULL && pKvsRtpTransceiver->sender.track.codec == rtcCodec && contains == FALSE) {
            CHK_STATUS(doubleListInsertItemTail(pKvsPeerConnection->pAnswerTransceivers, (UINT64) pKvsRtpTransceiver));
            CHK_STATUS(hashTablePut(pSeenTransceivers, (UINT64) pKvsRtpTransceiver, 0));
            *pDidFindCodec = TRUE;
            break;
        }
        pCurNode = pCurNode->pNext;
    }

CleanUp:

    return retStatus;
}

// primarily used for creating a list of transceivers corresponding to each media m-line to respond to an offer with an answer
// This function generates the pAnswerTransceivers list which contains transceivers corresponding to each m-line in correct order
// To generate this list, it traverses over each m-line, first checks if it has a user-created transceiver present using findCodecInTransceivers
// if found, it adds that transceiver to pAnswerTransceivers
// if not, it creates a fake transceiver and adds it to pAnswerTransceivers
// In case a fake transceiver is created, the codec corresponding to that is RTC_CODEC_UNKNOWN.
// This function also obtains the payload type and rtpmap value for each unknown codec and adds it
// to pUnknownCodecPayloadTypesTable and pUnknownCodecRtpmapTable respectively. The keys for both hashtables are integers;
// a key in both hashtables corresponds to the payloadtype and rtpmap value for the same unknown codec,
// meaning, the same key can be used to retrieve a value for an unknown codec from both hashtables
STATUS findTransceiversByRemoteDescription(PKvsPeerConnection pKvsPeerConnection, PSessionDescription pRemoteSessionDescription,
                                           PHashTable pUnknownCodecPayloadTypesTable, PHashTable pUnknownCodecRtpmapTable)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 currentMedia, currentAttribute, tokenLen = 0, codec = 0, count = 0, unknownCodecCounter = 0;
    PSdpMediaDescription pMediaDescription = NULL;
    PCHAR attributeValue, end, codecs = NULL;
    PCHAR rtpMapValue = NULL;
    CHAR firstCodec[MAX_PAYLOAD_TYPE_LENGTH];
    BOOL supportCodec, foundMediaSectionWithCodec, containsPayloadType = FALSE, containsRtpMap = FALSE;
    PHashTable pSeenTransceivers;
    RTC_CODEC rtcCodec;
    MEDIA_STREAM_TRACK_KIND streamKind;
    PKvsRtpTransceiver pKvsRtpFakeTransceiver = NULL;
    PRtcMediaStreamTrack pRtcMediaStreamTrack;
    RtcMediaStreamTrack track;

    CHK_STATUS(hashTableCreate(&pSeenTransceivers)); // to be used by findCodecInTransceivers

    // sample m-lines
    // m=audio 9 UDP/TLS/RTP/SAVPF 111 63 103 104 9 0 8 106 105 13 110 112 113 126
    // m=video 9 UDP/TLS/RTP/SAVPF 96 97 98 99 100 101 102 121 127 120 125 107 108 109 124 119 123 117 35 36 114 115 116 62 118
    // this loop iterates over all the m-lines
    for (currentMedia = 0; currentMedia < pRemoteSessionDescription->mediaCount; currentMedia++) {
        pMediaDescription = &(pRemoteSessionDescription->mediaDescriptions[currentMedia]);
        foundMediaSectionWithCodec = FALSE;
        count = 0;
        MEMSET(firstCodec, 0x00, MAX_PAYLOAD_TYPE_LENGTH);

        // Scan the media section name for any codecs we support
        // sample attributeValue=audio 9 UDP/TLS/RTP/SAVPF 111 63 103 104 9 0 8 106 105 13 110 112 113 126
        attributeValue = pMediaDescription->mediaName;

        if ((end = STRCHR(attributeValue, ' ')) != NULL) {
            tokenLen = (end - attributeValue);
        } else {
            tokenLen = STRLEN(attributeValue);
        }

        if (STRNCMP(MEDIA_SECTION_AUDIO_VALUE, attributeValue, tokenLen) == 0) {
            streamKind = MEDIA_STREAM_TRACK_KIND_AUDIO;
        } else if (STRNCMP(MEDIA_SECTION_VIDEO_VALUE, attributeValue, tokenLen) == 0) {
            streamKind = MEDIA_STREAM_TRACK_KIND_VIDEO;
        } else {
            continue; // ignore non-media m-lines
        }

        do {
            count++;
            if ((end = STRCHR(attributeValue, ' ')) != NULL) {
                tokenLen = (end - attributeValue);
            } else {
                tokenLen = STRLEN(attributeValue);
            }
            if (count == 4) {
                codecs = attributeValue; // codecs = 111 63 103 104 9 0 8 106 105 13 110 112 113 126
            }
            if (count > 3) { // look for codec values from payload types (111 63 103 104 9 0 8 106 105 13 110 112 113 126)
                if (STRNCMP(DEFAULT_PAYLOAD_MULAW_STR, attributeValue, tokenLen) == 0) {
                    supportCodec = TRUE;
                    rtcCodec = RTC_CODEC_MULAW;
                } else if (STRNCMP(DEFAULT_PAYLOAD_ALAW_STR, attributeValue, tokenLen) == 0) {
                    supportCodec = TRUE;
                    rtcCodec = RTC_CODEC_ALAW;
                } else {
                    supportCodec = FALSE;
                }

                // if a supported codec is found, check if a user has created a transceiver for that
                if (supportCodec) {
                    CHK_STATUS(findCodecInTransceivers(pKvsPeerConnection, rtcCodec, &foundMediaSectionWithCodec, pSeenTransceivers));
                }
            }
            if (end != NULL) {
                attributeValue = end + 1;
            }
        } while (end != NULL && !foundMediaSectionWithCodec);

        // get the first payload type from codecs in case we need to use it to generate a fake transceiver to respond to an m-line
        // if we don't have a user-created one corresponding to an m-line
        // we can respond to an m-line by including any one codec the offer had
        // e.g: codecs = 111 63 103 104 9 0 8 106 105 13 110 112 113 126
        //      firstCodec = 111
        if ((end = STRCHR(codecs, ' ')) != NULL) {
            tokenLen = (end - codecs);
        } else {
            tokenLen = STRLEN(codecs);
        }
        STRNCPY(firstCodec, codecs, tokenLen);

        // in case a supported codec was not found from the payload types, check the rtpmaps in the a-lines for that particular m-line
        for (currentAttribute = 0; currentAttribute < pMediaDescription->mediaAttributesCount && !foundMediaSectionWithCodec; currentAttribute++) {
            attributeValue = pMediaDescription->sdpAttributes[currentAttribute].attributeValue;
            rtcCodec = RTC_CODEC_UNKNOWN;

            // check for supported codec in rtpmap values only if an a-line contains the keyword "rtpmap" to save string comparisons
            if (STRNCMP(RTPMAP_VALUE, pMediaDescription->sdpAttributes[currentAttribute].attributeName, 6) == 0) {
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
                } else if (STRSTR(attributeValue, H265_VALUE) != NULL) {
                    supportCodec = TRUE;
                    rtcCodec = RTC_CODEC_H265;
                } else {
                    supportCodec = FALSE;
                }

                // if a supported codec is found, check if a user has created a transceiver for that
                if (supportCodec) {
                    CHK_STATUS(findCodecInTransceivers(pKvsPeerConnection, rtcCodec, &foundMediaSectionWithCodec, pSeenTransceivers));
                }

                // if the m-line / codec is not supported or the user has not created a transceiver corresponding to it
                // find the rtpmap value for the firstcodec that we saved previously
                // e.g a-line: a=rtpmap:111 opus/48000/2
                //     attributeValue = 111 opus/48000/2
                //     firstCodec = 111
                //     rtpMapValue = opus/48000/2
                //     tokenLen = 3 (still contains length of the firstCodec)
                if (foundMediaSectionWithCodec == FALSE) {
                    if ((end = STRCHR(attributeValue, ' ')) != NULL)
                        if (STRNCMP(attributeValue, firstCodec, tokenLen) == 0) {
                            rtpMapValue = end + 1;
                        }
                }
            }
        }

        // if we have not found a transceiver for an m-line or if the codec is unsupported, got to this section to generate a fake transceiver
        if (!foundMediaSectionWithCodec && (streamKind == MEDIA_STREAM_TRACK_KIND_AUDIO || streamKind == MEDIA_STREAM_TRACK_KIND_VIDEO)) {
            MEMSET(&track, 0x00, SIZEOF(RtcMediaStreamTrack));
            track.kind = streamKind;
            track.codec = RTC_CODEC_UNKNOWN;
            STRCPY(track.streamId, "fakeStream");
            STRCPY(track.trackId, "fakeTrack");
            pRtcMediaStreamTrack = &track;
            CHK_STATUS(createKvsRtpTransceiver(RTC_RTP_TRANSCEIVER_DIRECTION_INACTIVE, pKvsPeerConnection, (UINT32) RAND(), (UINT32) RAND(),
                                               pRtcMediaStreamTrack, NULL, RTC_CODEC_UNKNOWN, &pKvsRtpFakeTransceiver));

            // add the new transceiver to a list of fake transceivers to keep a track of them
            // add the same to the pAnswerTransceivers since it will be needed later to serialize
            CHK_STATUS(doubleListInsertItemTail(pKvsPeerConnection->pFakeTransceivers, (UINT64) pKvsRtpFakeTransceiver));
            CHK_STATUS(doubleListInsertItemTail(pKvsPeerConnection->pAnswerTransceivers, (UINT64) pKvsRtpFakeTransceiver));

            CHK_STATUS(STRTOUI32(firstCodec, firstCodec + tokenLen, 10, &codec));

            // Insert (int)(firstCodec) and rtpMapValue into the hashtables with the same key so they can be retrieved later during serialization
            CHK_STATUS(hashTableContains(pUnknownCodecPayloadTypesTable, (UINT64) codec, &containsPayloadType));
            CHK_STATUS(hashTableContains(pUnknownCodecRtpmapTable, (UINT64) rtpMapValue, &containsRtpMap));
            if (containsPayloadType == FALSE && containsRtpMap == FALSE) {
                CHK_STATUS(hashTablePut(pUnknownCodecPayloadTypesTable, unknownCodecCounter, (UINT64) codec));
                CHK_STATUS(hashTablePut(pUnknownCodecRtpmapTable, unknownCodecCounter, (UINT64) rtpMapValue));
                unknownCodecCounter++;
                containsPayloadType = FALSE;
                containsRtpMap = FALSE;
            }
        }
    }

CleanUp:

    CHK_STATUS(hashTableFree(pSeenTransceivers));
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
                    isVideoCodec = (codec == RTC_CODEC_VP8 || codec == RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE ||
                                    codec == RTC_CODEC_H265);
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
