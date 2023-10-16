#define LOG_CLASS "ChannelInfo"
#include "../Include_i.h"

#define ARN_DELIMETER_CHAR                  ':'
#define ARN_CHANNEL_NAME_CODE_SEP           '/'
#define ARN_BEGIN                           "arn:aws"
#define SIGNALING_CHANNEL_ARN_SERVICE_NAME  "kinesisvideo"
#define SIGNALING_CHANNEL_ARN_RESOURCE_TYPE "channel/"
#define AWS_ACCOUNT_ID_LENGTH               12
#define AWS_KVS_ARN_CODE_LENGTH             13
#define SIGNALING_CHANNEL_ARN_MIN_LENGTH    59

// Example: arn:aws:kinesisvideo:region:account-id:channel/channel-name/code
// Min Length of ":account-id:channel/channel-name/code"
// = len(":") + len(account-id) + len(":") + len("channel") + len("/") + len(channel-name) + len("/") + len(code)
// channel name must be at least 1 char
// = 1 + 12 + 1 + 7 + 1 + 1 + 1 + 13 = 37
#define CHANNEL_ARN_MIN_DIST_FROM_REGION_END_TO_END_OF_ARN 37

STATUS createValidateChannelInfo(PChannelInfo pOrigChannelInfo, PChannelInfo* ppChannelInfo)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT16 channelNameStartPos = 0, channelNameLen = 0;

    UINT32 allocSize, channelArnLen = 0, storageStreamArnLen = 0, regionLen = 0, cpUrlLen = 0, certPathLen = 0, userAgentPostfixLen = 0,
                      customUserAgentLen = 0, userAgentLen = 0, kmsLen = 0, tagsSize;
    PCHAR pCurPtr, pRegionPtr, pUserAgentPostfixPtr;
    CHAR agentString[MAX_CUSTOM_USER_AGENT_NAME_POSTFIX_LEN + 1];

    PChannelInfo pChannelInfo = NULL;

    CHK(pOrigChannelInfo != NULL && ppChannelInfo != NULL, STATUS_NULL_ARG);

    CHK((pOrigChannelInfo->pChannelName != NULL || pOrigChannelInfo->pChannelArn != NULL) && ppChannelInfo != NULL, STATUS_NULL_ARG);

    // Get and validate the lengths for all strings and store lengths excluding null terminator
    if (pOrigChannelInfo->pChannelName != NULL) {
        CHK((channelNameLen = (UINT32) STRNLEN(pOrigChannelInfo->pChannelName, MAX_CHANNEL_NAME_LEN + 1)) <= MAX_CHANNEL_NAME_LEN,
            STATUS_SIGNALING_INVALID_CHANNEL_NAME_LENGTH);
    }

    if (pOrigChannelInfo->pChannelArn != NULL) {
        CHK((channelArnLen = (UINT32) STRNLEN(pOrigChannelInfo->pChannelArn, MAX_ARN_LEN + 1)) <= MAX_ARN_LEN,
            STATUS_SIGNALING_INVALID_CHANNEL_ARN_LENGTH);

        if (pOrigChannelInfo->pChannelName == NULL) {
            CHK_STATUS(validateKvsSignalingChannelArnAndExtractChannelName(pOrigChannelInfo, &channelNameStartPos, &channelNameLen));
        }
    }

    if (pOrigChannelInfo->pStorageStreamArn != NULL) {
        CHK((storageStreamArnLen = (UINT32) STRNLEN(pOrigChannelInfo->pStorageStreamArn, MAX_ARN_LEN + 1)) <= MAX_ARN_LEN,
            STATUS_SIGNALING_INVALID_CHANNEL_ARN_LENGTH);
    }

    // Fix-up the region
    if (pOrigChannelInfo->pRegion != NULL) {
        CHK((regionLen = (UINT32) STRNLEN(pOrigChannelInfo->pRegion, MAX_REGION_NAME_LEN + 1)) <= MAX_REGION_NAME_LEN,
            STATUS_SIGNALING_INVALID_REGION_LENGTH);
        pRegionPtr = pOrigChannelInfo->pRegion;
    } else {
        regionLen = ARRAY_SIZE(DEFAULT_AWS_REGION) - 1;
        pRegionPtr = DEFAULT_AWS_REGION;
    }

    if (pOrigChannelInfo->pControlPlaneUrl != NULL) {
        CHK((cpUrlLen = (UINT32) STRNLEN(pOrigChannelInfo->pControlPlaneUrl, MAX_URI_CHAR_LEN + 1)) <= MAX_URI_CHAR_LEN,
            STATUS_SIGNALING_INVALID_CPL_LENGTH);
    } else {
        cpUrlLen = MAX_CONTROL_PLANE_URI_CHAR_LEN;
    }

    if (pOrigChannelInfo->pCertPath != NULL) {
        CHK((certPathLen = (UINT32) STRNLEN(pOrigChannelInfo->pCertPath, MAX_PATH_LEN + 1)) <= MAX_PATH_LEN,
            STATUS_SIGNALING_INVALID_CERTIFICATE_PATH_LENGTH);
    }

    userAgentLen = MAX_USER_AGENT_LEN;

    if (pOrigChannelInfo->pUserAgentPostfix != NULL && STRCMP(pOrigChannelInfo->pUserAgentPostfix, EMPTY_STRING) != 0) {
        CHK((userAgentPostfixLen = (UINT32) STRNLEN(pOrigChannelInfo->pUserAgentPostfix, MAX_CUSTOM_USER_AGENT_NAME_POSTFIX_LEN + 1)) <=
                MAX_CUSTOM_USER_AGENT_NAME_POSTFIX_LEN,
            STATUS_SIGNALING_INVALID_AGENT_POSTFIX_LENGTH);
        pUserAgentPostfixPtr = pOrigChannelInfo->pUserAgentPostfix;
    } else {
        // Account for the "/" in the agent string.
        // The default user agent postfix is:AWS-WEBRTC-KVS-AGENT/<SDK-version>
        userAgentPostfixLen = STRLEN(SIGNALING_USER_AGENT_POSTFIX_NAME) + STRLEN(SIGNALING_USER_AGENT_POSTFIX_VERSION) + 1;
        CHK(userAgentPostfixLen <= MAX_CUSTOM_USER_AGENT_NAME_POSTFIX_LEN, STATUS_SIGNALING_INVALID_AGENT_POSTFIX_LENGTH);
        SNPRINTF(agentString,
                 userAgentPostfixLen + 1, // account for null terminator
                 (PCHAR) "%s/%s", SIGNALING_USER_AGENT_POSTFIX_NAME, SIGNALING_USER_AGENT_POSTFIX_VERSION);
        pUserAgentPostfixPtr = agentString;
    }

    if (pOrigChannelInfo->pCustomUserAgent != NULL) {
        CHK((customUserAgentLen = (UINT32) STRNLEN(pOrigChannelInfo->pCustomUserAgent, MAX_CUSTOM_USER_AGENT_LEN + 1)) <= MAX_CUSTOM_USER_AGENT_LEN,
            STATUS_SIGNALING_INVALID_AGENT_LENGTH);
    }

    if (pOrigChannelInfo->pKmsKeyId != NULL) {
        CHK((kmsLen = (UINT32) STRNLEN(pOrigChannelInfo->pKmsKeyId, MAX_ARN_LEN + 1)) <= MAX_ARN_LEN, STATUS_SIGNALING_INVALID_KMS_KEY_LENGTH);
    }

    if (pOrigChannelInfo->messageTtl == 0) {
        pOrigChannelInfo->messageTtl = SIGNALING_DEFAULT_MESSAGE_TTL_VALUE;
    } else {
        CHK(pOrigChannelInfo->messageTtl >= MIN_SIGNALING_MESSAGE_TTL_VALUE && pOrigChannelInfo->messageTtl <= MAX_SIGNALING_MESSAGE_TTL_VALUE,
            STATUS_SIGNALING_INVALID_MESSAGE_TTL_VALUE);
    }

    // If tags count is not zero then pTags shouldn't be NULL
    CHK_STATUS(validateTags(pOrigChannelInfo->tagCount, pOrigChannelInfo->pTags));

    // Account for the tags
    CHK_STATUS(packageTags(pOrigChannelInfo->tagCount, pOrigChannelInfo->pTags, 0, NULL, &tagsSize));

    // Allocate enough storage to hold the data with aligned strings size and set the pointers and NULL terminators
    // concatenate the data space with channel info.
    allocSize = SIZEOF(ChannelInfo) + ALIGN_UP_TO_MACHINE_WORD(1 + channelNameLen) + ALIGN_UP_TO_MACHINE_WORD(1 + channelArnLen) +
        ALIGN_UP_TO_MACHINE_WORD(1 + storageStreamArnLen) + ALIGN_UP_TO_MACHINE_WORD(1 + regionLen) + ALIGN_UP_TO_MACHINE_WORD(1 + cpUrlLen) +
        ALIGN_UP_TO_MACHINE_WORD(1 + certPathLen) + ALIGN_UP_TO_MACHINE_WORD(1 + userAgentPostfixLen) +
        ALIGN_UP_TO_MACHINE_WORD(1 + customUserAgentLen) + ALIGN_UP_TO_MACHINE_WORD(1 + userAgentLen) + ALIGN_UP_TO_MACHINE_WORD(1 + kmsLen) +
        tagsSize;
    CHK(NULL != (pChannelInfo = (PChannelInfo) MEMCALLOC(1, allocSize)), STATUS_NOT_ENOUGH_MEMORY);

    pChannelInfo->version = CHANNEL_INFO_CURRENT_VERSION;
    pChannelInfo->channelType = pOrigChannelInfo->channelType;
    pChannelInfo->channelRoleType = pOrigChannelInfo->channelRoleType;
    pChannelInfo->cachingPeriod = pOrigChannelInfo->cachingPeriod;
    pChannelInfo->retry = pOrigChannelInfo->retry;
    pChannelInfo->reconnect = pOrigChannelInfo->reconnect;
    pChannelInfo->messageTtl = pOrigChannelInfo->messageTtl;
    pChannelInfo->tagCount = pOrigChannelInfo->tagCount;
    pChannelInfo->useMediaStorage = pOrigChannelInfo->useMediaStorage;

    // V1 handling
    if (pOrigChannelInfo->version > 0) {
        pChannelInfo->cachingPolicy = pOrigChannelInfo->cachingPolicy;
    } else {
        pChannelInfo->cachingPolicy = SIGNALING_API_CALL_CACHE_TYPE_NONE;
    }

    // Set the current pointer to the end
    pCurPtr = (PCHAR) (pChannelInfo + 1);

    // Set the pointers to the end and copy the data.
    // NOTE: the structure is calloc-ed so the strings will be NULL terminated
    if (channelNameLen != 0) {
        if (pOrigChannelInfo->pChannelName != NULL) {
            STRCPY(pCurPtr, pOrigChannelInfo->pChannelName);
        } else {
            STRNCPY(pCurPtr, pOrigChannelInfo->pChannelArn + channelNameStartPos, channelNameLen);
        }
        pChannelInfo->pChannelName = pCurPtr;
        pCurPtr += ALIGN_UP_TO_MACHINE_WORD(channelNameLen + 1); // For the NULL terminator
    }

    if (channelArnLen != 0) {
        STRCPY(pCurPtr, pOrigChannelInfo->pChannelArn);
        pChannelInfo->pChannelArn = pCurPtr;
        pCurPtr += ALIGN_UP_TO_MACHINE_WORD(channelArnLen + 1);
    }

    if (storageStreamArnLen != 0) {
        STRCPY(pCurPtr, pOrigChannelInfo->pStorageStreamArn);
        pChannelInfo->pStorageStreamArn = pCurPtr;
        pCurPtr += ALIGN_UP_TO_MACHINE_WORD(storageStreamArnLen + 1);
    }

    STRCPY(pCurPtr, pRegionPtr);
    pChannelInfo->pRegion = pCurPtr;
    pCurPtr += ALIGN_UP_TO_MACHINE_WORD(regionLen + 1);

    if (pOrigChannelInfo->pControlPlaneUrl != NULL && *pOrigChannelInfo->pControlPlaneUrl != '\0') {
        STRCPY(pCurPtr, pOrigChannelInfo->pControlPlaneUrl);
    } else {
        // Create a fully qualified URI
        SNPRINTF(pCurPtr, MAX_CONTROL_PLANE_URI_CHAR_LEN, "%s%s.%s%s", CONTROL_PLANE_URI_PREFIX, KINESIS_VIDEO_SERVICE_NAME, pChannelInfo->pRegion,
                 CONTROL_PLANE_URI_POSTFIX);
        // If region is in CN, add CN region uri postfix
        if (STRSTR(pChannelInfo->pRegion, "cn-")) {
            STRCAT(pCurPtr, ".cn");
        }
    }

    pChannelInfo->pControlPlaneUrl = pCurPtr;
    pCurPtr += ALIGN_UP_TO_MACHINE_WORD(cpUrlLen + 1);

    if (certPathLen != 0) {
        STRCPY(pCurPtr, pOrigChannelInfo->pCertPath);
        pChannelInfo->pCertPath = pCurPtr;
        pCurPtr += ALIGN_UP_TO_MACHINE_WORD(certPathLen + 1);
    }

    if (userAgentPostfixLen != 0) {
        STRCPY(pCurPtr, pUserAgentPostfixPtr);
        pChannelInfo->pUserAgentPostfix = pCurPtr;
        pCurPtr += ALIGN_UP_TO_MACHINE_WORD(userAgentPostfixLen + 1);
    }

    if (customUserAgentLen != 0) {
        STRCPY(pCurPtr, pOrigChannelInfo->pCustomUserAgent);
        pChannelInfo->pCustomUserAgent = pCurPtr;
        pCurPtr += ALIGN_UP_TO_MACHINE_WORD(customUserAgentLen + 1);
    }

    getUserAgentString(pUserAgentPostfixPtr, pOrigChannelInfo->pCustomUserAgent, MAX_USER_AGENT_LEN, pCurPtr);
    pChannelInfo->pUserAgent = pCurPtr;
    pChannelInfo->pUserAgent[MAX_USER_AGENT_LEN] = '\0';
    pCurPtr += ALIGN_UP_TO_MACHINE_WORD(userAgentLen + 1);

    if (kmsLen != 0) {
        STRCPY(pCurPtr, pOrigChannelInfo->pKmsKeyId);
        pChannelInfo->pKmsKeyId = pCurPtr;
        pCurPtr += ALIGN_UP_TO_MACHINE_WORD(kmsLen + 1);
    }

    // Fix-up the caching period
    if (pChannelInfo->cachingPeriod == SIGNALING_API_CALL_CACHE_TTL_SENTINEL_VALUE) {
        pChannelInfo->cachingPeriod = SIGNALING_DEFAULT_API_CALL_CACHE_TTL;
    }

    // Process tags
    pChannelInfo->tagCount = pOrigChannelInfo->tagCount;
    if (pOrigChannelInfo->tagCount != 0) {
        pChannelInfo->pTags = (PTag) pCurPtr;

        // Package the tags after the structure
        CHK_STATUS(packageTags(pOrigChannelInfo->tagCount, pOrigChannelInfo->pTags, tagsSize, pChannelInfo->pTags, NULL));
    }

CleanUp:

    if (STATUS_FAILED(retStatus)) {
        freeChannelInfo(&pChannelInfo);
    }

    if (ppChannelInfo != NULL) {
        *ppChannelInfo = pChannelInfo;
    }

    LEAVES();
    return retStatus;
}

STATUS freeChannelInfo(PChannelInfo* ppChannelInfo)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PChannelInfo pChannelInfo;

    CHK(ppChannelInfo != NULL, STATUS_NULL_ARG);
    pChannelInfo = *ppChannelInfo;

    CHK(pChannelInfo != NULL, retStatus);

    // Warn if we have an unknown version as the free might crash or leak
    if (pChannelInfo->version > CHANNEL_INFO_CURRENT_VERSION) {
        DLOGW("Channel info version check failed 0x%08x", STATUS_SIGNALING_INVALID_CHANNEL_INFO_VERSION);
    }

    MEMFREE(*ppChannelInfo);

    *ppChannelInfo = NULL;

CleanUp:

    LEAVES();
    return retStatus;
}

SIGNALING_CHANNEL_STATUS getChannelStatusFromString(PCHAR status, UINT32 length)
{
    // Assume the channel Deleting status first
    SIGNALING_CHANNEL_STATUS channelStatus = SIGNALING_CHANNEL_STATUS_DELETING;

    if (0 == STRNCMP((PCHAR) "ACTIVE", status, length)) {
        channelStatus = SIGNALING_CHANNEL_STATUS_ACTIVE;
    } else if (0 == STRNCMP((PCHAR) "CREATING", status, length)) {
        channelStatus = SIGNALING_CHANNEL_STATUS_CREATING;
    } else if (0 == STRNCMP((PCHAR) "UPDATING", status, length)) {
        channelStatus = SIGNALING_CHANNEL_STATUS_UPDATING;
    } else if (0 == STRNCMP((PCHAR) "DELETING", status, length)) {
        channelStatus = SIGNALING_CHANNEL_STATUS_DELETING;
    }

    return channelStatus;
}

SIGNALING_CHANNEL_TYPE getChannelTypeFromString(PCHAR type, UINT32 length)
{
    // Assume the channel Deleting status first
    SIGNALING_CHANNEL_TYPE channelType = SIGNALING_CHANNEL_TYPE_UNKNOWN;

    if (0 == STRNCMP(SIGNALING_CHANNEL_TYPE_SINGLE_MASTER_STR, type, length)) {
        channelType = SIGNALING_CHANNEL_TYPE_SINGLE_MASTER;
    }

    return channelType;
}

PCHAR getStringFromChannelType(SIGNALING_CHANNEL_TYPE type)
{
    PCHAR typeStr;

    switch (type) {
        case SIGNALING_CHANNEL_TYPE_SINGLE_MASTER:
            typeStr = SIGNALING_CHANNEL_TYPE_SINGLE_MASTER_STR;
            break;
        default:
            typeStr = SIGNALING_CHANNEL_TYPE_UNKNOWN_STR;
            break;
    }

    return typeStr;
}

SIGNALING_CHANNEL_ROLE_TYPE getChannelRoleTypeFromString(PCHAR type, UINT32 length)
{
    // Assume the channel Deleting status first
    SIGNALING_CHANNEL_ROLE_TYPE channelRoleType = SIGNALING_CHANNEL_ROLE_TYPE_UNKNOWN;

    if (0 == STRNCMP(SIGNALING_CHANNEL_ROLE_TYPE_MASTER_STR, type, length)) {
        channelRoleType = SIGNALING_CHANNEL_ROLE_TYPE_MASTER;
    } else if (0 == STRNCMP(SIGNALING_CHANNEL_ROLE_TYPE_VIEWER_STR, type, length)) {
        channelRoleType = SIGNALING_CHANNEL_ROLE_TYPE_VIEWER;
    }

    return channelRoleType;
}

PCHAR getStringFromChannelRoleType(SIGNALING_CHANNEL_ROLE_TYPE type)
{
    PCHAR typeStr;

    switch (type) {
        case SIGNALING_CHANNEL_ROLE_TYPE_MASTER:
            typeStr = SIGNALING_CHANNEL_ROLE_TYPE_MASTER_STR;
            break;
        case SIGNALING_CHANNEL_ROLE_TYPE_VIEWER:
            typeStr = SIGNALING_CHANNEL_ROLE_TYPE_VIEWER_STR;
            break;
        default:
            typeStr = SIGNALING_CHANNEL_ROLE_TYPE_UNKNOWN_STR;
            break;
    }

    return typeStr;
}

// https://docs.aws.amazon.com/kinesisvideostreams-webrtc-dg/latest/devguide/kvswebrtc-how-iam.html#kinesis-using-iam-arn-format
// Example: arn:aws:kinesisvideo:region:account-id:channel/channel-name/code
STATUS validateKvsSignalingChannelArnAndExtractChannelName(PChannelInfo pChannelInfo, PUINT16 pStart, PUINT16 pNumChars)
{
    UINT16 arnLength, currPosIndex = 0, channelNameLength = 0, channelNameStart = 0;
    PCHAR partitionEnd, regionEnd, channelNameEnd;
    PCHAR currPos;
    UINT8 accountIdStart = 1, accountIdEnd = AWS_ACCOUNT_ID_LENGTH;
    UINT8 timeCodeStart = 0, timeCodeEnd = AWS_KVS_ARN_CODE_LENGTH - 1;

    if (pChannelInfo != NULL && pChannelInfo->pChannelArn != NULL) {
        arnLength = STRNLEN(pChannelInfo->pChannelArn, MAX_ARN_LEN);

        if (arnLength >= SIGNALING_CHANNEL_ARN_MIN_LENGTH) {
            currPos = pChannelInfo->pChannelArn;

            if (STRNCMP(currPos, ARN_BEGIN, STRLEN(ARN_BEGIN)) == 0) {
                currPosIndex += STRLEN(ARN_BEGIN);
                partitionEnd = STRNCHR(currPos + currPosIndex, arnLength - currPosIndex, ARN_DELIMETER_CHAR);

                if (partitionEnd != NULL && (partitionEnd - currPos) < arnLength) {
                    currPosIndex = partitionEnd - currPos + 1;

                    if (currPosIndex < arnLength &&
                        STRNCMP(currPos + currPosIndex, SIGNALING_CHANNEL_ARN_SERVICE_NAME, STRLEN(SIGNALING_CHANNEL_ARN_SERVICE_NAME)) == 0) {
                        currPosIndex += STRLEN(SIGNALING_CHANNEL_ARN_SERVICE_NAME);

                        if (currPosIndex < arnLength && *(currPos + currPosIndex) == ARN_DELIMETER_CHAR) {
                            currPosIndex++;

                            regionEnd = STRNCHR(currPos + currPosIndex, arnLength - currPosIndex, ARN_DELIMETER_CHAR);

                            if (regionEnd != NULL) {
                                if (currPosIndex < arnLength && (regionEnd - currPos) < arnLength) {
                                    currPosIndex = regionEnd - currPos;

                                    if (currPosIndex + CHANNEL_ARN_MIN_DIST_FROM_REGION_END_TO_END_OF_ARN <= arnLength) {
                                        while (accountIdStart <= accountIdEnd &&
                                               (*(currPos + currPosIndex + accountIdStart) >= '0' &&
                                                *(currPos + currPosIndex + accountIdStart) <= '9')) {
                                            accountIdStart++;
                                        }

                                        if (accountIdStart == accountIdEnd + 1 && *(currPos + currPosIndex + accountIdStart) == ARN_DELIMETER_CHAR) {
                                            currPosIndex += (accountIdStart + 1);

                                            if (STRNCMP(currPos + currPosIndex, SIGNALING_CHANNEL_ARN_RESOURCE_TYPE,
                                                        STRLEN(SIGNALING_CHANNEL_ARN_RESOURCE_TYPE)) == 0) {
                                                // Channel Name Begins Here, ends when we hit ARN_CHANNEL_NAME_CODE_SEP
                                                currPosIndex += STRLEN(SIGNALING_CHANNEL_ARN_RESOURCE_TYPE);
                                                channelNameEnd = STRNCHR(currPos + currPosIndex, arnLength - currPosIndex - AWS_KVS_ARN_CODE_LENGTH,
                                                                         ARN_CHANNEL_NAME_CODE_SEP);

                                                if (channelNameEnd != NULL) {
                                                    channelNameLength = channelNameEnd - (currPos + currPosIndex);
                                                    if (channelNameLength > 0) {
                                                        channelNameStart = currPosIndex;
                                                        currPosIndex += (channelNameLength + 1);

                                                        if (currPosIndex + AWS_KVS_ARN_CODE_LENGTH == arnLength) {
                                                            // 13 digit time code

                                                            while ((timeCodeStart <= timeCodeEnd) &&
                                                                   *(currPos + currPosIndex + timeCodeStart) >= '0' &&
                                                                   *(currPos + currPosIndex + timeCodeStart) <= '9') {
                                                                timeCodeStart++;
                                                            }

                                                            // Verify that we have 13 digits and that we are not at the end of the arn
                                                            if (currPosIndex + timeCodeStart == arnLength) {
                                                                *pStart = channelNameStart;
                                                                *pNumChars = channelNameLength;
                                                                return STATUS_SUCCESS;
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    return STATUS_SIGNALING_INVALID_CHANNEL_ARN;
}
