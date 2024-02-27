#include "abstraction.h"



static UINT32 setLogLevel() {
    PCHAR pLogLevel;
    UINT32 logLevel = LOG_LEVEL_DEBUG;
    if (NULL == (pLogLevel = GETENV(DEBUG_LOG_LEVEL_ENV_VAR)) || STATUS_SUCCESS != STRTOUI32(pLogLevel, NULL, 10, &logLevel) ||
        logLevel < LOG_LEVEL_VERBOSE || logLevel > LOG_LEVEL_SILENT) {
        logLevel = LOG_LEVEL_WARN;
    }
    SET_LOGGER_LOG_LEVEL(logLevel);
    return logLevel;
}

STATUS initializeLibrary(PAppCtx pAppCtx) {
    STATUS retStatus = STATUS_SUCCESS;
    PCHAR pAccessKey, pSecretKey, pSessionToken;
    pAppCtx->logLevel = setLogLevel();
    CHK_STATUS(initKvsWebRtc());

    CHK_ERR((pAccessKey = GETENV(ACCESS_KEY_ENV_VAR)) != NULL, STATUS_INVALID_OPERATION, "AWS_ACCESS_KEY_ID must be set");
    CHK_ERR((pSecretKey = GETENV(SECRET_KEY_ENV_VAR)) != NULL, STATUS_INVALID_OPERATION, "AWS_SECRET_ACCESS_KEY must be set");
    pSessionToken = GETENV(SESSION_TOKEN_ENV_VAR);
    if (pSessionToken != NULL && IS_EMPTY_STRING(pSessionToken)) {
        DLOGW("Session token is set but its value is empty. Ignoring.");
        pSessionToken = NULL;
    }

    CHK_STATUS(createStaticCredentialProvider(pAccessKey, 0, pSecretKey, 0, pSessionToken, 0, MAX_UINT64, &pAppCtx->signalingCtx.pCredentialProvider));
CleanUp:
    return retStatus;
}

STATUS initializeAppCtx(PAppCtx pAppCtx, PCHAR channelName, PCHAR region) {
    STATUS retStatus = STATUS_SUCCESS;
    pAppCtx->signalingCtx.channelInfo.pChannelName = channelName;
    pAppCtx->signalingCtx.channelInfo.pRegion = region;
    pAppCtx->signalingCtx.clientInfo.loggingLevel = pAppCtx->logLevel;
    initializeSignaling(&pAppCtx->signalingCtx);
CleanUp:
    return retStatus;
}

STATUS deinitializeLibrary() {
    STATUS retStatus = STATUS_SUCCESS;
    CHK_STATUS(deinitKvsWebRtc());
CleanUp:
    return retStatus;
}