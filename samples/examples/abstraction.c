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

static STATUS readFromEnvs(PAppCtx pAppCtx) {
    STATUS retStatus = STATUS_SUCCESS;
    PCHAR pChannelName;
    if (NULL == (pChannelName = GETENV("CHANNEL_NAME"))) {
        STRNCPY(pAppCtx->channelName, "SampleTestChannel", STRLEN("SampleTestChannel"));
    } else {
        STRNCPY(pAppCtx->channelName, pChannelName, SIZEOF(pAppCtx->channelName));
    }
    DLOGI("Channel name: %s", pAppCtx->channelName);
CleanUp:
    return retStatus;
}

STATUS initializeLibrary(PAppCtx pAppCtx) {
    STATUS retStatus = STATUS_SUCCESS;
    setLogLevel();
    CHK_STATUS(initKvsWebRtc());
    CHK_STATUS(readFromEnvs(pAppCtx));
CleanUp:
    return retStatus;
}

STATUS deinitializeLibrary() {
    STATUS retStatus = STATUS_SUCCESS;
    CHK_STATUS(deinitKvsWebRtc());
CleanUp:
    return retStatus;
}