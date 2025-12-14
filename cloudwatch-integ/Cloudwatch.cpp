#include "Include.h"
#include "Cloudwatch.h"

namespace CppInteg {

Cloudwatch::Cloudwatch(ClientConfiguration* pClientConfig)
    : logs(pClientConfig), monitoring(pClientConfig), terminated(FALSE)
{
}

STATUS Cloudwatch::init(PCHAR channelName, PCHAR region, BOOL isMaster, BOOL isStorage)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    ClientConfiguration clientConfig;
    CreateLogGroupRequest createLogGroupRequest;
    Aws::CloudWatchLogs::Model::CreateLogStreamOutcome createLogStreamOutcome;
    CreateLogStreamRequest createLogStreamRequest;

    clientConfig.region = region;
    auto& instance = getInstanceImpl(&clientConfig);

    if (STATUS_FAILED(instance.logs.init(channelName, region, isMaster, isStorage))) {
        DLOGW("Failed to create Cloudwatch logger");
    } else {
        globalCustomLogPrintFn = logger;
    }

    CHK_STATUS(instance.monitoring.init(channelName, region, isMaster, isStorage));

CleanUp:

    LEAVES();
    return retStatus;
}

Cloudwatch& Cloudwatch::getInstance()
{
    return getInstanceImpl();
}

Cloudwatch& Cloudwatch::getInstanceImpl(ClientConfiguration* pClientConfig)
{
    static Cloudwatch instance{pClientConfig};
    return instance;
}

VOID Cloudwatch::deinit()
{
    auto& instance = getInstance();
    instance.logs.deinit();
    instance.monitoring.deinit();
    instance.terminated = TRUE;
}

VOID Cloudwatch::logger(UINT32 level, PCHAR tag, PCHAR fmt, ...)
{
    CHAR logFmtString[MAX_LOG_FORMAT_LENGTH + 1];
    CHAR cwLogFmtString[MAX_LOG_FORMAT_LENGTH + 1];
    UINT32 logLevel = GET_LOGGER_LOG_LEVEL();
    UNUSED_PARAM(tag);

    if (level >= logLevel) {
        addLogMetadata(logFmtString, (UINT32) ARRAY_SIZE(logFmtString), fmt, level);

        // Creating a copy to store the logFmtString for cloudwatch logging purpose
        va_list valist, valist_cw;
        va_start(valist_cw, fmt);
        vsnprintf(cwLogFmtString, (SIZE_T) SIZEOF(cwLogFmtString), logFmtString, valist_cw);
        va_end(valist_cw);
        va_start(valist, fmt);
        vprintf(logFmtString, valist);
        va_end(valist);

        auto& instance = getInstance();
        if (!instance.terminated) {
            instance.logs.push(cwLogFmtString);
        }
    }
}

} // namespace Canary
