#define LOG_CLASS "SignalingFileCache"
#include "../Include_i.h"

/****************************************************************************************************
 * Content of the caching file will look as follows:
 * channelName,role,region,channelARN,httpEndpoint,wssEndpoint,cacheCreationTimestamp\n
 * channelName,role,region,channelARN,httpEndpoint,wssEndpoint,cacheCreationTimestamp\n
 ****************************************************************************************************/

STATUS createFileIfNotExist(PCHAR fileName)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    BOOL fileExist;

    CHK_STATUS(fileExists(fileName, &fileExist));
    if (!fileExist) {
        CHK_STATUS(createFile(fileName, 0));
    }

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS deserializeSignalingCacheEntries(PCHAR cachedFileContent, UINT64 fileSize, PSignalingFileCacheEntry pSignalingFileCacheEntryList,
                                        PUINT32 pEntryCount, PCHAR cacheFilePath)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 listSize = 0, entryCount = 0, tokenCount = 0, remainingSize, tokenSize = 0;
    PCHAR pCurrent = NULL, nextToken = NULL, nextLine = NULL;

    CHK(cachedFileContent != NULL && pSignalingFileCacheEntryList != NULL && pEntryCount != NULL && cacheFilePath != NULL, STATUS_NULL_ARG);
    listSize = *pEntryCount;

    pCurrent = cachedFileContent;
    remainingSize = (UINT32) fileSize;
    /* detect end of file */
    while (remainingSize > MAX_SIGNALING_CACHE_ENTRY_TIMESTAMP_STR_LEN) {
        nextLine = STRCHR(pCurrent, '\n');
        while ((nextToken = STRCHR(pCurrent, ',')) != NULL && nextToken < nextLine) {
            switch (tokenCount % 7) {
                case 0:
                    STRNCPY(pSignalingFileCacheEntryList[entryCount].channelName, pCurrent, nextToken - pCurrent);
                    break;
                case 1:
                    STRNCPY(pSignalingFileCacheEntryList[entryCount].region, pCurrent, nextToken - pCurrent);
                    if (STRNCMP(pCurrent, SIGNALING_FILE_CACHE_ROLE_TYPE_MASTER_STR, STRLEN(SIGNALING_FILE_CACHE_ROLE_TYPE_MASTER_STR)) == 0) {
                        pSignalingFileCacheEntryList[entryCount].role = SIGNALING_CHANNEL_ROLE_TYPE_MASTER;
                    } else if (STRNCMP(pCurrent, SIGNALING_FILE_CACHE_ROLE_TYPE_VIEWER_STR, STRLEN(SIGNALING_FILE_CACHE_ROLE_TYPE_VIEWER_STR)) == 0) {
                        pSignalingFileCacheEntryList[entryCount].role = SIGNALING_CHANNEL_ROLE_TYPE_VIEWER;
                    } else {
                        CHK_WARN(FALSE, STATUS_INVALID_ARG, "Unknown role type");
                    }
                    break;
                case 2:
                    STRNCPY(pSignalingFileCacheEntryList[entryCount].region, pCurrent, nextToken - pCurrent);
                    break;
                case 3:
                    STRNCPY(pSignalingFileCacheEntryList[entryCount].channelArn, pCurrent, nextToken - pCurrent);
                    break;
                case 4:
                    STRNCPY(pSignalingFileCacheEntryList[entryCount].httpsEndpoint, pCurrent, nextToken - pCurrent);
                    break;
                case 5:
                    STRNCPY(pSignalingFileCacheEntryList[entryCount].wssEndpoint, pCurrent, nextToken - pCurrent);
                    break;
                default:
                    break;
            }
            tokenCount++;
            tokenSize = (UINT32)(nextToken - pCurrent);
            pCurrent += tokenSize + 1;
            remainingSize -= tokenSize + 1;
        }

        /* time stamp element is always 10 characters */
        CHK_STATUS(STRTOUI64(pCurrent, pCurrent + MAX_SIGNALING_CACHE_ENTRY_TIMESTAMP_STR_LEN, 10,
                             &pSignalingFileCacheEntryList[entryCount].creationTsEpochSeconds));
        tokenCount++;
        pCurrent += MAX_SIGNALING_CACHE_ENTRY_TIMESTAMP_STR_LEN + 1;
        remainingSize -= MAX_SIGNALING_CACHE_ENTRY_TIMESTAMP_STR_LEN + 1;

        CHK(!IS_EMPTY_STRING(pSignalingFileCacheEntryList[entryCount].channelArn) &&
                !IS_EMPTY_STRING(pSignalingFileCacheEntryList[entryCount].channelName) &&
                !IS_EMPTY_STRING(pSignalingFileCacheEntryList[entryCount].region) &&
                !IS_EMPTY_STRING(pSignalingFileCacheEntryList[entryCount].httpsEndpoint) &&
                !IS_EMPTY_STRING(pSignalingFileCacheEntryList[entryCount].wssEndpoint),
            STATUS_INVALID_ARG);

        entryCount++;

        /* Stop parsing once we reach cache entry limit */
        if (entryCount == listSize) {
            break;
        }
    }

CleanUp:

    if (pEntryCount != NULL) {
        *pEntryCount = entryCount;
    }

    CHK_LOG_ERR(retStatus);

    if (STATUS_FAILED(retStatus) && cacheFilePath != NULL) {
        FREMOVE(cacheFilePath);
    }

    LEAVES();
    return retStatus;
}

STATUS signalingCacheLoadFromFile(PCHAR channelName, PCHAR region, SIGNALING_CHANNEL_ROLE_TYPE role,
                                  PSignalingFileCacheEntry pSignalingFileCacheEntry, PBOOL pCacheFound, PCHAR cacheFilePath)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT64 fileSize = 0;
    PCHAR fileBuffer = NULL;
    SignalingFileCacheEntry entries[MAX_SIGNALING_CACHE_ENTRY_COUNT];
    UINT32 entryCount = ARRAY_SIZE(entries), i;
    BOOL cacheFound = FALSE;

    CHK(channelName != NULL && region != NULL && pSignalingFileCacheEntry != NULL && pCacheFound != NULL && cacheFilePath != NULL, STATUS_NULL_ARG);
    CHK(!IS_EMPTY_STRING(channelName) && !IS_EMPTY_STRING(region), STATUS_INVALID_ARG);

    CHK_STATUS(createFileIfNotExist(cacheFilePath));

    MEMSET(entries, 0x00, SIZEOF(entries));

    CHK_STATUS(readFile(cacheFilePath, FALSE, NULL, &fileSize));

    if (fileSize > 0) {
        /* +1 for null terminator */
        fileBuffer = MEMCALLOC(1, (fileSize + 1) * SIZEOF(CHAR));
        CHK(fileBuffer != NULL, STATUS_NOT_ENOUGH_MEMORY);
        CHK_STATUS(readFile(cacheFilePath, FALSE, (PBYTE) fileBuffer, &fileSize));

        CHK_STATUS(deserializeSignalingCacheEntries(fileBuffer, fileSize, entries, &entryCount, cacheFilePath));

        for (i = 0; !cacheFound && i < entryCount; ++i) {
            /* Assume channel name and region has been validated */
            if (STRCMP(entries[i].channelName, channelName) == 0 && STRCMP(entries[i].region, region) == 0 && entries[i].role == role) {
                cacheFound = TRUE;
                *pSignalingFileCacheEntry = entries[i];
            }
        }
    }

    *pCacheFound = cacheFound;

CleanUp:

    SAFE_MEMFREE(fileBuffer);

    CHK_LOG_ERR(retStatus);

    LEAVES();
    return retStatus;
}

STATUS signalingCacheSaveToFile(PSignalingFileCacheEntry pSignalingFileCacheEntry, PCHAR cacheFilePath)
{
    ENTERS();

    STATUS retStatus = STATUS_SUCCESS;
    SignalingFileCacheEntry entries[MAX_SIGNALING_CACHE_ENTRY_COUNT];
    UINT32 entryCount = ARRAY_SIZE(entries), i, serializedCacheEntryLen;
    UINT64 fileSize = 0;
    PCHAR fileBuffer = NULL;
    PSignalingFileCacheEntry pExistingCacheEntry = NULL;
    CHAR serializedCacheEntry[MAX_SERIALIZED_SIGNALING_CACHE_ENTRY_LEN];

    CHK(cacheFilePath != NULL && pSignalingFileCacheEntry != NULL, STATUS_NULL_ARG);
    CHK(!IS_EMPTY_STRING(pSignalingFileCacheEntry->channelArn) && !IS_EMPTY_STRING(pSignalingFileCacheEntry->channelName) &&
            !IS_EMPTY_STRING(pSignalingFileCacheEntry->region) && !IS_EMPTY_STRING(pSignalingFileCacheEntry->httpsEndpoint) &&
            !IS_EMPTY_STRING(pSignalingFileCacheEntry->wssEndpoint),
        STATUS_INVALID_ARG);

    MEMSET(entries, 0x00, SIZEOF(entries));

    CHK_STATUS(createFileIfNotExist(cacheFilePath));

    /* read entire file into buffer */
    CHK_STATUS(readFile(cacheFilePath, FALSE, NULL, &fileSize));
    /* deserialize if file is not empty */
    if (fileSize > 0) {
        /* +1 for null terminator */
        fileBuffer = MEMCALLOC(1, (fileSize + 1) * SIZEOF(CHAR));
        CHK(fileBuffer != NULL, STATUS_NOT_ENOUGH_MEMORY);
        CHK_STATUS(readFile(cacheFilePath, FALSE, (PBYTE) fileBuffer, &fileSize));

        CHK_STATUS(deserializeSignalingCacheEntries(fileBuffer, fileSize, entries, &entryCount, cacheFilePath));
    } else {
        entryCount = 0;
    }

    for (i = 0; pExistingCacheEntry == NULL && i < entryCount; ++i) {
        /* Assume channel name and region has been validated */
        if (STRCMP(entries[i].channelName, pSignalingFileCacheEntry->channelName) == 0 &&
            STRCMP(entries[i].region, pSignalingFileCacheEntry->region) == 0 && entries[i].role == pSignalingFileCacheEntry->role) {
            pExistingCacheEntry = &entries[i];
        }
    }

    /* at this point i is at most entryCount */
    CHK_WARN(entryCount < MAX_SIGNALING_CACHE_ENTRY_COUNT, STATUS_INVALID_OPERATION,
             "Failed to store signaling cache because max entry count of %u reached", MAX_SIGNALING_CACHE_ENTRY_COUNT);

    entries[i] = *pSignalingFileCacheEntry;
    entryCount++;

    for (i = 0; i < entryCount; ++i) {
        serializedCacheEntryLen =
            SNPRINTF(serializedCacheEntry, ARRAY_SIZE(serializedCacheEntry), "%s,%s,%s,%s,%s,%s,%.10" PRIu64 "\n", entries[i].channelName,
                     entries[i].role == SIGNALING_CHANNEL_ROLE_TYPE_MASTER ? SIGNALING_FILE_CACHE_ROLE_TYPE_MASTER_STR
                                                                           : SIGNALING_FILE_CACHE_ROLE_TYPE_VIEWER_STR,
                     entries[i].region, entries[i].channelArn, entries[i].httpsEndpoint, entries[i].wssEndpoint, entries[i].creationTsEpochSeconds);
        CHK_STATUS(writeFile(cacheFilePath, FALSE, i == 0 ? FALSE : TRUE, (PBYTE) serializedCacheEntry, serializedCacheEntryLen));
    }

CleanUp:

    SAFE_MEMFREE(fileBuffer);

    CHK_LOG_ERR(retStatus);

    LEAVES();
    return retStatus;
}
