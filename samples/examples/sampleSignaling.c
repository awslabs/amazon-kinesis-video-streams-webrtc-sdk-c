#include "sampleSignaling.h"

static STATUS traverseDirectoryPEMFileScan(UINT64 customData, DIR_ENTRY_TYPES entryType, PCHAR fullPath, PCHAR fileName)
{
    UNUSED_PARAM(entryType);
    UNUSED_PARAM(fullPath);

    PCHAR certName = (PCHAR) customData;
    UINT32 fileNameLen = STRLEN(fileName);

    if (fileNameLen > ARRAY_SIZE(CA_CERT_PEM_FILE_EXTENSION) + 1 &&
        (STRCMPI(CA_CERT_PEM_FILE_EXTENSION, &fileName[fileNameLen - ARRAY_SIZE(CA_CERT_PEM_FILE_EXTENSION) + 1]) == 0)) {
        certName[0] = FPATHSEPARATOR;
        certName++;
        STRCPY(certName, fileName);
    }

    return STATUS_SUCCESS;
}

static STATUS lookForSslCert(PCHAR* ppCaCertPath)
{
    STATUS retStatus = STATUS_SUCCESS;
    struct stat pathStat;
    CHAR certName[MAX_PATH_LEN];
    PCHAR pCaCertPath;

    MEMSET(certName, 0x0, ARRAY_SIZE(certName));
    pCaCertPath = GETENV(CACERT_PATH_ENV_VAR);

    // if ca cert path is not set from the environment, try to use the one that cmake detected
    if (pCaCertPath == NULL) {
        CHK_ERR(STRNLEN(DEFAULT_KVS_CACERT_PATH, MAX_PATH_LEN) > 0, STATUS_INVALID_OPERATION, "No ca cert path given (error:%s)", strerror(errno));
        pCaCertPath = DEFAULT_KVS_CACERT_PATH;
    } else {
        // Check if the environment variable is a path
        CHK(0 == FSTAT(pCaCertPath, &pathStat), STATUS_DIRECTORY_ENTRY_STAT_ERROR);

        if (S_ISDIR(pathStat.st_mode)) {
            CHK_STATUS(traverseDirectory(pCaCertPath, (UINT64) &certName, /* iterate */ FALSE, traverseDirectoryPEMFileScan));

            if (certName[0] != 0x0) {
                STRCAT(pCaCertPath, certName);
            } else {
                DLOGW("Cert not found in path set...checking if CMake detected a path");
                CHK_ERR(STRNLEN(DEFAULT_KVS_CACERT_PATH, MAX_PATH_LEN) > 0, STATUS_INVALID_OPERATION, "No ca cert path given (error:%s)",
                        strerror(errno));
                DLOGI("CMake detected cert path");
                pCaCertPath = DEFAULT_KVS_CACERT_PATH;
            }
        }
    }

    CleanUp:
    *ppCaCertPath = pCaCertPath;
    CHK_LOG_ERR(retStatus);
    return retStatus;
}

STATUS initializeSignaling(PSignalingCtx pSignalingCtx)
{
    STATUS retStatus = STATUS_SUCCESS;
//    pAppCtx->signalingClientCallbacks.messageReceivedFn = signalingMessageReceived;
    STRCPY(pSignalingCtx->clientInfo.clientId, "ProducerMaster");
    lookForSslCert(&pSignalingCtx->channelInfo.pCertPath);
    DLOGI("Cert: %s", pSignalingCtx->channelInfo.pCertPath);
    pSignalingCtx->channelInfo.channelRoleType = SIGNALING_CHANNEL_ROLE_TYPE_MASTER;
    CHK_STATUS(createSignalingClientSync(&pSignalingCtx->clientInfo, &pSignalingCtx->channelInfo,
                                         &pSignalingCtx->signalingClientCallbacks, pSignalingCtx->pCredentialProvider,
                                         &pSignalingCtx->signalingClientHandle));
    // Enable the processing of the messages
    CHK_STATUS(signalingClientFetchSync(pSignalingCtx->signalingClientHandle));
    CHK_STATUS(signalingClientConnectSync(pSignalingCtx->signalingClientHandle));

    CleanUp:
    return retStatus;
}