#pragma once
#include "../samples/Samples.h"
#include "Include.h"
#include <numeric>

namespace CppInteg {

    class Config;
    typedef Config* PConfig;

    class Config {
    public:
        STATUS init(INT32 argc, PCHAR argv[]);
        template <typename T> class Value {
        public:
            T value;
            BOOL initialized = FALSE;
        };

        Value<std::string> endpoint;
        Value<std::string> label;
        Value<std::string> channelName;
        Value<std::string> clientId;
        Value<std::string> videoCodec;
        Value<BOOL> isMaster;
        Value<BOOL> runBothPeers;
        Value<BOOL> trickleIce;
        Value<BOOL> useTurn;
        Value<BOOL> forceTurn;
        Value<BOOL> useIotCredentialProvider;
        Value<BOOL> isProfilingMode;

        BOOL isStorage;

        // credentials
        Value<std::string> accessKey;
        Value<std::string> secretKey;
        Value<std::string> sessionToken;
        Value<std::string> region;
        Value<std::string> iotCoreCredentialEndPointFile;
        Value<std::string> iotCoreCert;
        Value<std::string> iotCorePrivateKey;
        Value<std::string> iotCoreRoleAlias;

        // logging
        Value<UINT32> logLevel;
        Value<std::string> logGroupName;
        Value<std::string> logStreamName;

        Value<UINT64> duration;
        Value<UINT64> iterationDuration;
        Value<UINT64> bitRate;
        Value<UINT64> frameRate;

        Value<std::string> caCertPath;
        Value<std::string> storageFristFrameSentTSFileName;

        BYTE iotEndpoint[MAX_CONFIG_JSON_FILE_SIZE];

        VOID print();

    private:
        STATUS initWithEnvVars();
    };

} // namespace Canary
