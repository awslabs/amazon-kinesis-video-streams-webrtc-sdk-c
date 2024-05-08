#include "Config.h"

namespace CppInteg {

    STATUS Config::init(INT32 argc, PCHAR argv[])
    {
        // TODO: Probably also support command line args to fill the config
        STATUS retStatus = STATUS_SUCCESS;

        CHK(argv != NULL, STATUS_NULL_ARG);
        CHK_STATUS(initWithEnvVars());

        // Need to impose a min duration
        if (duration.value != 0 && duration.value < CANARY_MIN_DURATION) {
            DLOGW("Canary duration should be at least %u seconds. Overriding with minimal duration.",
                  CANARY_MIN_DURATION / HUNDREDS_OF_NANOS_IN_A_SECOND);
            duration.value = CANARY_MIN_DURATION;
        }

        // Need to impose a min iteration duration
        if (iterationDuration.value < CANARY_MIN_ITERATION_DURATION) {
            DLOGW("Canary iterations duration should be at least %u seconds. Overriding with minimal iterations duration.",
                  CANARY_MIN_ITERATION_DURATION / HUNDREDS_OF_NANOS_IN_A_SECOND);
            iterationDuration.value = CANARY_MIN_ITERATION_DURATION;
        }

        CleanUp:

        return retStatus;
    }

    BOOL strtobool(const CHAR* value)
    {
        if (STRCMPI(value, "on") == 0 || STRCMPI(value, "true") == 0) {
            return TRUE;
        }

        return FALSE;
    }

    STATUS mustenv(CHAR const* pKey, Config::Value<std::string>* pResult)
    {
        STATUS retStatus = STATUS_SUCCESS;
        const CHAR* value;

        CHK(pResult != NULL, STATUS_NULL_ARG);
        CHK(!pResult->initialized, retStatus);

        CHK_ERR((value = getenv(pKey)) != NULL, STATUS_INVALID_OPERATION, "%s must be set", pKey);
        pResult->value = value;
        pResult->initialized = TRUE;

        CleanUp:

        return retStatus;
    }

    STATUS optenv(CHAR const* pKey, Config::Value<std::string>* pResult, std::string defaultValue)
    {
        STATUS retStatus = STATUS_SUCCESS;
        const CHAR* value;

        CHK(pResult != NULL, STATUS_NULL_ARG);
        CHK(!pResult->initialized, retStatus);

        if (NULL != (value = getenv(pKey))) {
            pResult->value = value;
        } else {
            pResult->value = defaultValue;
        }
        pResult->initialized = TRUE;

        CleanUp:

        return retStatus;
    }

    STATUS mustenvBool(CHAR const* pKey, Config::Value<BOOL>* pResult)
    {
        STATUS retStatus = STATUS_SUCCESS;
        Config::Value<std::string> raw;

        CHK(pResult != NULL, STATUS_NULL_ARG);
        CHK(!pResult->initialized, retStatus);
        CHK_STATUS(mustenv(pKey, &raw));

        pResult->value = strtobool(raw.value.c_str());
        pResult->initialized = TRUE;

        CleanUp:

        return retStatus;
    }

    STATUS optenvBool(CHAR const* pKey, Config::Value<BOOL>* pResult, BOOL defVal)
    {
        STATUS retStatus = STATUS_SUCCESS;
        Config::Value<std::string> raw;

        CHK(pResult != NULL, STATUS_NULL_ARG);
        CHK(!pResult->initialized, retStatus);
        CHK_STATUS(optenv(pKey, &raw, ""));
        if (!raw.value.empty()) {
            pResult->value = strtobool(raw.value.c_str());
        } else {
            pResult->value = defVal;
        }
        pResult->initialized = TRUE;

        CleanUp:

        return retStatus;
    }

    STATUS mustenvUint64(CHAR const* pKey, Config::Value<UINT64>* pResult)
    {
        STATUS retStatus = STATUS_SUCCESS;
        Config::Value<std::string> raw;

        CHK(pResult != NULL, STATUS_NULL_ARG);
        CHK(!pResult->initialized, retStatus);
        CHK_STATUS(mustenv(pKey, &raw));

        STRTOUI64((PCHAR) raw.value.c_str(), NULL, 10, &pResult->value);
        pResult->initialized = TRUE;

        CleanUp:

        return retStatus;
    }

    STATUS optenvUint64(CHAR const* pKey, Config::Value<UINT64>* pResult, UINT64 defVal)
    {
        STATUS retStatus = STATUS_SUCCESS;
        Config::Value<std::string> raw;

        CHK(pResult != NULL, STATUS_NULL_ARG);
        CHK(!pResult->initialized, retStatus);
        CHK_STATUS(optenv(pKey, &raw, ""));
        if (!raw.value.empty()) {
            STRTOUI64((PCHAR) raw.value.c_str(), NULL, 10, &pResult->value);
        } else {
            pResult->value = defVal;
        }
        pResult->initialized = TRUE;

        CleanUp:

        return retStatus;
    }

    STATUS Config::initWithEnvVars()
    {
        STATUS retStatus = STATUS_SUCCESS;
        Config::Value<UINT64> logLevel64;
        std::stringstream defaultLogStreamName;
        UINT64 fileSize;

        /* This is ignored for master. Master can extract the info from offer. Viewer has to know if peer can trickle or
        * not ahead of time. */
        CHK_STATUS(optenvBool(CANARY_TRICKLE_ICE_ENV_VAR, &trickleIce, TRUE));
        CHK_STATUS(optenvBool(CANARY_USE_TURN_ENV_VAR, &useTurn, TRUE));
        CHK_STATUS(optenvBool(CANARY_FORCE_TURN_ENV_VAR, &forceTurn, FALSE));
        CHK_STATUS(optenvBool(CANARY_USE_IOT_CREDENTIALS_ENV_VAR, &useIotCredentialProvider, FALSE));
        CHK_STATUS(optenvBool(CANARY_RUN_IN_PROFILING_MODE_ENV_VAR, &isProfilingMode, FALSE));

        CHK_STATUS(optenv(CANARY_VIDEO_CODEC_ENV_VAR, &videoCodec, CANARY_VIDEO_CODEC_H264));
        CHK_STATUS(optenv(CACERT_PATH_ENV_VAR, &caCertPath, KVS_CA_CERT_PATH));

        if(useIotCredentialProvider.value == TRUE) {
            CHK_STATUS(mustenv(IOT_CORE_CREDENTIAL_ENDPOINT_ENV_VAR, &iotCoreCredentialEndPointFile));
            CHK_STATUS(readFile((PCHAR)iotCoreCredentialEndPointFile.value.c_str(), TRUE, NULL, &fileSize));
            CHK_ERR(fileSize != 0, STATUS_WEBRTC_EMPTY_IOT_CRED_FILE, "Empty credential file");
            CHK_STATUS(readFile((PCHAR)iotCoreCredentialEndPointFile.value.c_str(), TRUE, iotEndpoint, &fileSize));
            iotEndpoint[fileSize - 1] = '\0';
            CHK_STATUS(mustenv(IOT_CORE_CERT_ENV_VAR, &iotCoreCert));
            CHK_STATUS(mustenv(IOT_CORE_PRIVATE_KEY_ENV_VAR, &iotCorePrivateKey));
            CHK_STATUS(mustenv(IOT_CORE_ROLE_ALIAS_ENV_VAR, &iotCoreRoleAlias));
            CHK_STATUS(mustenv(IOT_CORE_THING_NAME_ENV_VAR, &channelName));
        }
        else {
            CHK_STATUS(mustenv(ACCESS_KEY_ENV_VAR, &accessKey));
            CHK_STATUS(mustenv(SECRET_KEY_ENV_VAR, &secretKey));
            CHK_STATUS(optenv(CANARY_CHANNEL_NAME_ENV_VAR, &channelName, CANARY_DEFAULT_CHANNEL_NAME));
        }
        CHK_STATUS(optenv(SESSION_TOKEN_ENV_VAR, &sessionToken, ""));
        CHK_STATUS(optenv(DEFAULT_REGION_ENV_VAR, &region, DEFAULT_AWS_REGION));

        // Set the logger log level
        if (!logLevel.initialized) {
            CHK_STATUS(optenvUint64(DEBUG_LOG_LEVEL_ENV_VAR, &logLevel64, LOG_LEVEL_WARN));
            logLevel.value = (UINT32) logLevel64.value;
            logLevel.initialized = TRUE;
        }

        CHK_STATUS(optenv(CANARY_ENDPOINT_ENV_VAR, &endpoint, ""));
        CHK_STATUS(optenv(CANARY_LABEL_ENV_VAR, &label, CANARY_DEFAULT_LABEL));

        CHK_STATUS(optenv(CANARY_CLIENT_ID_ENV_VAR, &clientId, CANARY_DEFAULT_CLIENT_ID));
        CHK_STATUS(optenvBool(CANARY_IS_MASTER_ENV_VAR, &isMaster, TRUE));
        CHK_STATUS(optenvBool(CANARY_RUN_BOTH_PEERS_ENV_VAR, &runBothPeers, FALSE));

        CHK_STATUS(optenv(CANARY_LOG_GROUP_NAME_ENV_VAR, &this->logGroupName, CANARY_DEFAULT_LOG_GROUP_NAME));
        defaultLogStreamName << channelName.value << '-' << (isMaster.value ? "master" : "viewer") << '-'
                             << GETTIME() / HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
        CHK_STATUS(optenv(CANARY_LOG_STREAM_NAME_ENV_VAR, &this->logStreamName, defaultLogStreamName.str()));

        if (!duration.initialized) {
            CHK_STATUS(optenvUint64(CANARY_DURATION_IN_SECONDS_ENV_VAR, &duration, 0));
            duration.value *= HUNDREDS_OF_NANOS_IN_A_SECOND;
        }

        // Iteration duration is an optional param
        if (!iterationDuration.initialized) {
            CHK_STATUS(optenvUint64(CANARY_ITERATION_IN_SECONDS_ENV_VAR, &iterationDuration, CANARY_DEFAULT_ITERATION_DURATION_IN_SECONDS));
            iterationDuration.value *= HUNDREDS_OF_NANOS_IN_A_SECOND;
        }

        CHK_STATUS(optenvUint64(CANARY_BIT_RATE_ENV_VAR, &bitRate, CANARY_DEFAULT_BITRATE));
        CHK_STATUS(optenvUint64(CANARY_FRAME_RATE_ENV_VAR, &frameRate, CANARY_DEFAULT_FRAMERATE));

        if (this->isStorage) {
            CHK_STATUS(optenv(STORAGE_CANARY_FIRST_FRAME_TS_FILE_ENV_VAR, &storageFristFrameSentTSFileName, STORAGE_CANARY_DEFAULT_FIRST_FRAME_TS_FILE));
        }

        CleanUp:

        return retStatus;
    }

    VOID Config::print()
    {
        DLOGD("Applied configuration:\n\n"
              "\tEndpoint        : %s\n"
              "\tRegion          : %s\n"
              "\tLabel           : %s\n"
              "\tChannel Name    : %s\n"
              "\tClient ID       : %s\n"
              "\tRole            : %s\n"
              "\tTrickle ICE     : %s\n"
              "\tUse TURN        : %s\n"
              "\tLog Level       : %u\n"
              "\tLog Group       : %s\n"
              "\tLog Stream      : %s\n"
              "\tDuration        : %lu seconds\n"
              "\tVideo codec     : %s\n"
              "\tIteration       : %lu seconds\n"
              "\tRun both peers  : %s\n"
              "\tCredential type : %s\n"
              "\tStorage         : %s\n"
              "\n",
              this->endpoint.value.c_str(), this->region.value.c_str(), this->label.value.c_str(), this->channelName.value.c_str(),
              this->clientId.value.c_str(), this->isMaster.value ? "Master" : "Viewer", this->trickleIce.value ? "True" : "False",
              this->useTurn.value ? "True" : "False", this->logLevel.value, this->logGroupName.value.c_str(), this->logStreamName.value.c_str(),
              this->duration.value / HUNDREDS_OF_NANOS_IN_A_SECOND, this->videoCodec.value.c_str(), this->iterationDuration.value / HUNDREDS_OF_NANOS_IN_A_SECOND,
              this->runBothPeers.value ? "True" : "False", this->useIotCredentialProvider.value ? "IoT" : "Static", this->isStorage ? "True" : "False");
        if(this->useIotCredentialProvider.value) {
            DLOGD("\tIoT endpoint : %s\n"
                  "\tIoT cert filename : %s\n"
                  "\tIoT private key filename : %s\n"
                  "\tIoT role alias : %s\n",
                  (PCHAR) this->iotEndpoint,
                    this->iotCoreCert.value.c_str(),
                    this->iotCorePrivateKey.value.c_str(),
                    this->iotCoreRoleAlias.value.c_str());
        }
        if(this->isStorage) {
            DLOGD("\n\n\tFirstFrameSentTSFileName : %s\n",
                  this->storageFristFrameSentTSFileName.value.c_str()
            );
        }
    }

} // namespace Cloudwatch
