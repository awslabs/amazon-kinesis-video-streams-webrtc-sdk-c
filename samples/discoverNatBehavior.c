#include <com/amazonaws/kinesis/video/webrtcclient/Include.h>

#define NETWORK_INTERFACE_NAME_PARAM   "-i"
#define STUN_HOSTNAME_PARAM            "-s"
#define MAX_LEN_NETWORK_INTERFACE_NAME 15

BOOL filterFunc(UINT64 data, PCHAR name)
{
    CHAR* desiredInterface = (CHAR*) data;
    if (desiredInterface == NULL || STRNCMP(name, desiredInterface, STRNLEN(name, MAX_LEN_NETWORK_INTERFACE_NAME)) == 0) {
        return TRUE;
    }
    return FALSE;
}

INT32 main(INT32 argc, CHAR** argv)
{
    PCHAR interfaceName = NULL, stunHostname = NULL, pRegion = NULL, pHostnamePostfix = NULL;
    NAT_BEHAVIOR mappingBehavior = NAT_BEHAVIOR_NONE, filteringBehavior = NAT_BEHAVIOR_NONE;
    INT32 i;
    UINT32 logLevel = LOG_LEVEL_DEBUG;
    STATUS status = STATUS_SUCCESS;
    CHAR* logLevelStr = NULL;

    if ((logLevelStr = GETENV(DEBUG_LOG_LEVEL_ENV_VAR)) != NULL) {
        status = STRTOUI32(logLevelStr, NULL, 10, &logLevel);
        if (STATUS_FAILED(status)) {
            DLOGE("Failed to parse log level with error 0x%08x", status);
            exit(1);
        }
        logLevel = MIN(MAX(logLevel, LOG_LEVEL_VERBOSE), LOG_LEVEL_SILENT);
    }
    SET_LOGGER_LOG_LEVEL(logLevel);

    DLOGI("Usage: ./discoverNatBehavior -i network-interface-name -s stun-hostname");

    if ((pRegion = GETENV(DEFAULT_REGION_ENV_VAR)) == NULL) {
        pRegion = DEFAULT_AWS_REGION;
    }
    pHostnamePostfix = KINESIS_VIDEO_STUN_URL_POSTFIX;
    // If region is in CN, add CN region uri postfix
    if (STRSTR(pRegion, "cn-")) {
        pHostnamePostfix = KINESIS_VIDEO_STUN_URL_POSTFIX_CN;
    }
    stunHostname = (PCHAR) MEMALLOC((MAX_ICE_CONFIG_URI_LEN + 1) * SIZEOF(CHAR));
    SNPRINTF(stunHostname, MAX_ICE_CONFIG_URI_LEN + 1, KINESIS_VIDEO_STUN_URL, pRegion, pHostnamePostfix);

    for (i = 1; i < argc; ++i) {
        if (STRNCMP(argv[i], NETWORK_INTERFACE_NAME_PARAM, STRNLEN(argv[i], MAX_LEN_NETWORK_INTERFACE_NAME)) == 0) {
            interfaceName = argv[++i];
            i++;
        } else if (STRNCMP(argv[i], STUN_HOSTNAME_PARAM, STRNLEN(argv[i], MAX_ICE_CONFIG_URI_LEN + 1)) == 0) {
            stunHostname = argv[++i];
            i++;
        } else {
            DLOGW("Unknown param %s", argv[i]);
        }
    }

    DLOGI("Using stun host: %s, local network interface %s.", stunHostname, interfaceName);

    initKvsWebRtc();

    status = discoverNatBehavior(stunHostname, &mappingBehavior, &filteringBehavior, filterFunc, (UINT64) interfaceName);
    if (STATUS_FAILED(status)) {
        DLOGE("Failed to detect NAT behavior with error 0x%08x", status);
        exit(1);
    }

    DLOGI("Detected NAT mapping behavior %s", getNatBehaviorStr(mappingBehavior));
    switch (mappingBehavior) {
        case NAT_BEHAVIOR_NONE:
            DLOGI("Failed to detect NAT mapping behavior");
            break;
        case NAT_BEHAVIOR_NOT_BEHIND_ANY_NAT:
            DLOGI("Host is not behind any NAT. Its IP address is the public IP address. STUN is not needed.");
            break;
        case NAT_BEHAVIOR_NO_UDP_CONNECTIVITY:
            DLOGI("Host does not have any UDP connectivity. STUN is not usable. Can only connect using TCP relay");
            break;
        case NAT_BEHAVIOR_ENDPOINT_INDEPENDENT:
            DLOGI("Host's NAT uses same public IP address regardless of destination address. STUN is usable.");
            break;
        case NAT_BEHAVIOR_ADDRESS_DEPENDENT:
            DLOGI("Host's NAT uses different public IP address for different destination address. STUN is not usable.");
            break;
        case NAT_BEHAVIOR_PORT_DEPENDENT:
            DLOGI("Host's NAT uses different public IP address for different destination address and port. STUN is not usable.");
            break;
    }

    DLOGI("Detected NAT filtering behavior %s", getNatBehaviorStr(filteringBehavior));
    switch (filteringBehavior) {
        case NAT_BEHAVIOR_NONE:
            DLOGI("Failed to detect NAT filtering behavior");
            break;
        case NAT_BEHAVIOR_NOT_BEHIND_ANY_NAT:
            DLOGI("Host is not behind any NAT. Its IP address is the public IP address. STUN is not needed.");
            break;
        case NAT_BEHAVIOR_NO_UDP_CONNECTIVITY:
            DLOGI("Host does not have any UDP connectivity. STUN is not usable. Can only connect using TCP relay");
            break;
        case NAT_BEHAVIOR_ENDPOINT_INDEPENDENT:
            DLOGI("Host's NAT allows to receive UDP packet from any external address. STUN is usable.");
            break;
        case NAT_BEHAVIOR_ADDRESS_DEPENDENT:
            DLOGI("Host's NAT allows to receive UDP packet from external address that host had previously sent data to. STUN is usable.");
            break;
        case NAT_BEHAVIOR_PORT_DEPENDENT:
            DLOGI("Host's NAT allows to receive UDP packet from external address and port that host had previously sent data to. STUN is usable.");
            break;
    }

    deinitKvsWebRtc();
    MEMFREE(stunHostname);
    return 0;
}
