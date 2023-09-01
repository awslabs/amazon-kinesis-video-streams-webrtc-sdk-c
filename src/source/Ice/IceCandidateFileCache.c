#define LOG_CLASS "IceCandidateFileCache"
#include "../Include_i.h"

/****************************************************************************************************
 * Content of the caching file will look as follows:
 * family,port,address,isPointToPoint|
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

STATUS deserializeIceCandidateCacheEntries(PCHAR cachedFileContent, UINT64 fileSize, PKvsIpAddress pKvsIpAddressEntryList, PCHAR cacheFilePath)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 entryCount = 0, tokenCount = 0, remainingSize, tokenSize, i, j, tempLen;
    PCHAR pCurrent = NULL, nextToken = NULL, nextLine = NULL;
    CHAR temp[KVS_MAX_IPV4_ADDRESS_STRING_LEN], pos;

    CHK(cachedFileContent != NULL && pKvsIpAddressEntryList != NULL && cacheFilePath != NULL, STATUS_NULL_ARG);

    pCurrent = cachedFileContent;
    remainingSize = (UINT32) fileSize;
    /* detect end of file */
    while (remainingSize > 1) {
        nextLine = STRCHR(pCurrent, '|');
        DLOGI("========Remaining size: %d ======LINE: %d", remainingSize, __LINE__);
        while (((nextToken = STRCHR(pCurrent, ',')) != NULL) && nextToken < nextLine) {
            switch (tokenCount % 4) {
                case 0:
                    STRNCPY(temp, pCurrent, nextToken - pCurrent);
                    pKvsIpAddressEntryList[entryCount].family = (unsigned short int) strtoul(temp, NULL, 10);
                    DLOGI("==================: %d LINE: %d", pKvsIpAddressEntryList[entryCount].family, __LINE__);
                    break;
                case 1:
                    STRNCPY(temp, pCurrent, nextToken - pCurrent);
                    pKvsIpAddressEntryList[entryCount].port = (unsigned short int) strtoul(temp, NULL, 10);
                    DLOGI("==================: %d LINE: %d", pKvsIpAddressEntryList[entryCount].family, __LINE__);
                    break;
                case 2:
                    STRNCPY(temp, pCurrent, nextToken - pCurrent);
                    for (i = 0; i < STRLEN(temp); i++) {
                        pKvsIpAddressEntryList[entryCount].address[i] = (unsigned char) temp[i];
                    }
                    break;
                default:
                    break;
            }
            tokenCount++;
            tokenSize = (UINT32) (nextToken - pCurrent);
            pCurrent += tokenSize + 1;
            remainingSize -= tokenSize + 1;
        }

        pKvsIpAddressEntryList[entryCount].isPointToPoint = pCurrent[0] == 49 ? TRUE : FALSE;
        DLOGI("==================: %d LINE: %d", pKvsIpAddressEntryList[entryCount].isPointToPoint, __LINE__);

        tokenCount++;
        pCurrent += 2;
        remainingSize -= 1;

        ++entryCount;
    }

CleanUp:

    CHK_LOG_ERR(retStatus);

    if (STATUS_FAILED(retStatus) && cacheFilePath != NULL) {
        FREMOVE(cacheFilePath);
    }

    LEAVES();
    return retStatus;
}

STATUS iceCandidateCacheLoadFromFile(PKvsIpAddress pKvsIpAddressEntryList, PBOOL pCacheFound, PCHAR cacheFilePath)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT64 fileSize = 0;
    PCHAR fileBuffer = NULL;
    KvsIpAddress entries[MAX_ICE_CANDIDATE_CACHE_ENTRY_COUNT];
    UINT32 entryCount = ARRAY_SIZE(entries);
    BOOL cacheFound = FALSE;

    CHK(pKvsIpAddressEntryList != NULL && pCacheFound != NULL && cacheFilePath != NULL, STATUS_NULL_ARG);

    CHK_STATUS(createFileIfNotExist(cacheFilePath));

    MEMSET(entries, 0x00, SIZEOF(entries));

    CHK_STATUS(readFile(cacheFilePath, FALSE, NULL, &fileSize));

    if (fileSize > 0) {
        /* +1 for null terminator */
        fileBuffer = MEMCALLOC(1, (fileSize + 1) * SIZEOF(CHAR));
        CHK(fileBuffer != NULL, STATUS_NOT_ENOUGH_MEMORY);
        CHK_STATUS(readFile(cacheFilePath, FALSE, (PBYTE) fileBuffer, &fileSize));

        CHK_STATUS(deserializeIceCandidateCacheEntries(fileBuffer, fileSize, pKvsIpAddressEntryList, cacheFilePath));
    }

    *pCacheFound = cacheFound;

CleanUp:

    SAFE_MEMFREE(fileBuffer);

    CHK_LOG_ERR(retStatus);

    LEAVES();
    return retStatus;
}

STATUS iceCandidateCacheSaveToFile(PKvsIpAddress pKvsIpAddressEntries, UINT32 interfaceCount, PCHAR cacheFilePath)
{
    ENTERS();

    STATUS retStatus = STATUS_SUCCESS;
    KvsIpAddress entries[MAX_ICE_CANDIDATE_CACHE_ENTRY_COUNT];
    PKvsIpAddress pIpAddress;
    UINT32 entryCount = ARRAY_SIZE(entries), i, j, serializedCacheEntryLen;
    UINT64 fileSize = 0;
    PCHAR fileBuffer = NULL;
    BOOL newEntry = TRUE;
    CHAR serializedCacheEntry[MAX_SERIALIZED_ICE_CANDIDATE_CACHE_ENTRY_LEN], temp[IPV4_ADDRESS_LENGTH];

    CHK(cacheFilePath != NULL && pKvsIpAddressEntries != NULL, STATUS_NULL_ARG);

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

        CHK_STATUS(deserializeIceCandidateCacheEntries(fileBuffer, fileSize, entries, cacheFilePath));
    } else {
        entryCount = 0;
    }

    for (i = 0; i < interfaceCount; ++i) {
        pIpAddress = &pKvsIpAddressEntries[i];
        for (i = 0; i < entryCount; ++i) {
        /* Assume channel name and region has been validated */
            if (entries[i].family == pIpAddress->family && entries[i].port == pIpAddress->port 
                && entries[i].isPointToPoint == pIpAddress->isPointToPoint) {
                newEntry = FALSE;
                break;
            }
        }
            /* at this point i is at most entryCount */
        if (entryCount >= MAX_ICE_CANDIDATE_CACHE_ENTRY_COUNT) {
            DLOGW("Overwrote 32nd entry to store signaling cache because max entry count of %u reached", MAX_ICE_CANDIDATE_CACHE_ENTRY_COUNT);
            i = MAX_ICE_CANDIDATE_CACHE_ENTRY_COUNT - 1;
            newEntry = FALSE;
        }

        entries[i] = *pIpAddress;
        if (newEntry) {
            entryCount++;
        }
    }


    for (i = 0; i < entryCount; ++i) {
        SNPRINTF(serializedCacheEntry, ARRAY_SIZE(serializedCacheEntry), "%d,%d,", entries[i].family,
                 entries[i].port);
        MEMCPY(serializedCacheEntry + 4, entries[i].address, 4);
        SNPRINTF(serializedCacheEntry + 8, ARRAY_SIZE(serializedCacheEntry), ",%d|", entries[i].isPointToPoint);
//        serializedCacheEntryLen =
//            SNPRINTF(serializedCacheEntry, ARRAY_SIZE(serializedCacheEntry), "%d,%d,%s,%d|", entries[i].family,
//                     entries[i].port, temp, entries[i].isPointToPoint);
        CHK_STATUS(writeFile(cacheFilePath, FALSE, i == 0 ? FALSE : TRUE, (PBYTE) serializedCacheEntry, serializedCacheEntryLen));
    }

CleanUp:

    SAFE_MEMFREE(fileBuffer);

    CHK_LOG_ERR(retStatus);

    LEAVES();
    return retStatus;
}
