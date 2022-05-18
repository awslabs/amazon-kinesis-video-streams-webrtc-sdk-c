#define LOG_CLASS "ChannelInfo"
#include "../Include_i.h"

STATUS createValidateChannelInfo(PChannelInfo pOrigChannelInfo, PChannelInfo* ppChannelInfo)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    UINT32 allocSize, nameLen = 0, arnLen = 0, regionLen = 0, cplLen = 0, certLen = 0, postfixLen = 0, agentLen = 0, userAgentLen = 0, kmsLen = 0,
                      tagsSize;
    PCHAR pCurPtr, pRegionPtr;
    PChannelInfo pChannelInfo = NULL;

    CHK(pOrigChannelInfo != NULL && ppChannelInfo != NULL, STATUS_NULL_ARG);

    CHK((pOrigChannelInfo->pChannelName != NULL || pOrigChannelInfo->pChannelArn != NULL) && ppChannelInfo != NULL, STATUS_NULL_ARG);

    // Get and validate the lengths for all strings and store lengths excluding null terminator
    if (pOrigChannelInfo->pChannelName != NULL) {
        CHK((nameLen = (UINT32) STRNLEN(pOrigChannelInfo->pChannelName, MAX_CHANNEL_NAME_LEN + 1)) <= MAX_CHANNEL_NAME_LEN,
            STATUS_SIGNALING_INVALID_CHANNEL_NAME_LENGTH);
    }

    if (pOrigChannelInfo->pChannelArn != NULL) {
        CHK((arnLen = (UINT32) STRNLEN(pOrigChannelInfo->pChannelArn, MAX_ARN_LEN + 1)) <= MAX_ARN_LEN, STATUS_SIGNALING_INVALID_CHANNEL_ARN_LENGTH);
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
        CHK((cplLen = (UINT32) STRNLEN(pOrigChannelInfo->pControlPlaneUrl, MAX_URI_CHAR_LEN + 1)) <= MAX_URI_CHAR_LEN,
            STATUS_SIGNALING_INVALID_CPL_LENGTH);
    } else {
        cplLen = MAX_CONTROL_PLANE_URI_CHAR_LEN;
    }

    if (pOrigChannelInfo->pCertPath != NULL) {
        CHK((certLen = (UINT32) STRNLEN(pOrigChannelInfo->pCertPath, MAX_PATH_LEN + 1)) <= MAX_PATH_LEN,
            STATUS_SIGNALING_INVALID_CERTIFICATE_PATH_LENGTH);
    }

    userAgentLen = MAX_USER_AGENT_LEN;

    if (pOrigChannelInfo->pUserAgentPostfix != NULL) {
        CHK((postfixLen = (UINT32) STRNLEN(pOrigChannelInfo->pUserAgentPostfix, MAX_CUSTOM_USER_AGENT_NAME_POSTFIX_LEN + 1)) <=
                MAX_CUSTOM_USER_AGENT_NAME_POSTFIX_LEN,
            STATUS_SIGNALING_INVALID_AGENT_POSTFIX_LENGTH);
    }

    if (pOrigChannelInfo->pCustomUserAgent != NULL) {
        CHK((agentLen = (UINT32) STRNLEN(pOrigChannelInfo->pCustomUserAgent, MAX_CUSTOM_USER_AGENT_LEN + 1)) <= MAX_CUSTOM_USER_AGENT_LEN,
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
    allocSize = SIZEOF(ChannelInfo) + ALIGN_UP_TO_MACHINE_WORD(1 + nameLen) + ALIGN_UP_TO_MACHINE_WORD(1 + arnLen) +
        ALIGN_UP_TO_MACHINE_WORD(1 + regionLen) + ALIGN_UP_TO_MACHINE_WORD(1 + cplLen) + ALIGN_UP_TO_MACHINE_WORD(1 + certLen) +
        ALIGN_UP_TO_MACHINE_WORD(1 + postfixLen) + ALIGN_UP_TO_MACHINE_WORD(1 + agentLen) + ALIGN_UP_TO_MACHINE_WORD(1 + userAgentLen) +
        ALIGN_UP_TO_MACHINE_WORD(1 + kmsLen) + tagsSize;
    CHK(NULL != (pChannelInfo = (PChannelInfo) MEMCALLOC(1, allocSize)), STATUS_NOT_ENOUGH_MEMORY);

    pChannelInfo->version = CHANNEL_INFO_CURRENT_VERSION;
    pChannelInfo->channelType = pOrigChannelInfo->channelType;
    pChannelInfo->channelRoleType = pOrigChannelInfo->channelRoleType;
    pChannelInfo->cachingPeriod = pOrigChannelInfo->cachingPeriod;
    pChannelInfo->retry = pOrigChannelInfo->retry;
    pChannelInfo->reconnect = pOrigChannelInfo->reconnect;
    pChannelInfo->messageTtl = pOrigChannelInfo->messageTtl;
    pChannelInfo->tagCount = pOrigChannelInfo->tagCount;

    // V1 handling
    if (pOrigChannelInfo->version > 0) {
        pChannelInfo->cachingPolicy = pOrigChannelInfo->cachingPolicy;
    } else {
        pChannelInfo->cachingPolicy = SIGNALING_API_CALL_CACHE_TYPE_NONE;
    }

    // Set the current pointer to the end
    pCurPtr = (PCHAR)(pChannelInfo + 1);

    // Set the pointers to the end and copy the data.
    // NOTE: the structure is calloc-ed so the strings will be NULL terminated
    if (nameLen != 0) {
        STRCPY(pCurPtr, pOrigChannelInfo->pChannelName);
        pChannelInfo->pChannelName = pCurPtr;
        pCurPtr += ALIGN_UP_TO_MACHINE_WORD(nameLen + 1); // For the NULL terminator
    }

    if (arnLen != 0) {
        STRCPY(pCurPtr, pOrigChannelInfo->pChannelArn);
        pChannelInfo->pChannelArn = pCurPtr;
        pCurPtr += ALIGN_UP_TO_MACHINE_WORD(arnLen + 1);
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
    }

    pChannelInfo->pControlPlaneUrl = pCurPtr;
    pCurPtr += ALIGN_UP_TO_MACHINE_WORD(cplLen + 1);

    if (certLen != 0) {
        STRCPY(pCurPtr, pOrigChannelInfo->pCertPath);
        pChannelInfo->pCertPath = pCurPtr;
        pCurPtr += ALIGN_UP_TO_MACHINE_WORD(certLen + 1);
    }

    if (postfixLen != 0) {
        STRCPY(pCurPtr, pOrigChannelInfo->pUserAgentPostfix);
        pChannelInfo->pUserAgentPostfix = pCurPtr;
        pCurPtr += ALIGN_UP_TO_MACHINE_WORD(postfixLen + 1);
    }

    if (agentLen != 0) {
        STRCPY(pCurPtr, pOrigChannelInfo->pCustomUserAgent);
        pChannelInfo->pCustomUserAgent = pCurPtr;
        pCurPtr += ALIGN_UP_TO_MACHINE_WORD(agentLen + 1);
    }

    getUserAgentString(pOrigChannelInfo->pUserAgentPostfix, pOrigChannelInfo->pCustomUserAgent, MAX_USER_AGENT_LEN, pCurPtr);
    pChannelInfo->pUserAgent = pCurPtr;
    pChannelInfo->pUserAgent[MAX_USER_AGENT_LEN] = '\0';
    pCurPtr += ALIGN_UP_TO_MACHINE_WORD(userAgentLen + 1);

    if (kmsLen != 0) {
        STRCPY(pCurPtr, pOrigChannelInfo->pCustomUserAgent);
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
