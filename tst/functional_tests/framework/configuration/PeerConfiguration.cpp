#include "PeerConfiguration.h"
#include <sstream>
#include <fstream>
#include <algorithm>
#include "../utils/ErrorCodes.h"

int PeerConfiguration::parsePeerConfiguration(const string& configurationFilePath) {
    return parseConfiguration(configurationFilePath, VALID_PEER_CONFIG_KEYS);
}

int PeerConfiguration::parseConfiguration(const string& configurationFilePath, const vector<string>& validConfigKeys) {

    std::ifstream configFile(configurationFilePath);

    std::string line;
    while (std::getline(configFile, line)) {
        vector<string> configParam;
        parseConfigParam(line, configParam);

        if (configParam.empty()) {
            continue;
        }

        if (configParam.size() != 2) {
            continue;
        }

        auto& key = configParam[0];
        auto& value = configParam[1];

        if (find(validConfigKeys.begin(), validConfigKeys.end(), key) == validConfigKeys.end()) {
            return CONFIG_PARSING_ERROR_INVALID_CONFIG_KEY;
        }

        configParamsMap[key] = value;
    }

    if (!isConfigValid(validConfigKeys)) {
        return CONFIG_PARSING_ERROR_MISSING_REQUIRED_CONFIG;
    }

    return TEST_SUCCESS;
}

PeerType PeerConfiguration::getPeerType() {
    return configParamsMap["PEER_TYPE"] == "MASTER" ? MASTER : VIEWER;
}

char* PeerConfiguration::getChannelName() {
    return const_cast<char*>(configParamsMap["SIGNALING_CHANNEL_NAME"].c_str());
}

bool PeerConfiguration::useTrickleIce() {
    return configParamsMap["USE_TRICKLE_ICE"] == "true";
}

bool PeerConfiguration::useTurn() {
    return configParamsMap["USE_TURN"] == "true";
}

char* PeerConfiguration::getLogDirectoryPath() {
    return const_cast<char*>(configParamsMap["LOG_DIRECTORY"].c_str());
}

char* PeerConfiguration::getMasterMetricsFilePath() {
    return const_cast<char*>(configParamsMap["MASTER_METRICS_FILE_PATH"].c_str());
}

char* PeerConfiguration::getViewerMetricsFilePath() {
    return const_cast<char*>(configParamsMap["VIEWER_METRICS_FILE_PATH"].c_str());
}
