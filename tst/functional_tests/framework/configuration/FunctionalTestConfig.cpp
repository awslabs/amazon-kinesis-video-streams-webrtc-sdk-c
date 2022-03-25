#include "PeerConfiguration.h"
#include <sstream>
#include <fstream>
#include "../utils/ErrorCodes.h"

int FunctionalTestConfiguration::parsePeerConfiguration(const string& configurationFilePath) {
    return parseConfiguration(configurationFilePath, VALID_PEER_CONFIG_KEYS);
}

int FunctionalTestConfiguration::parseConfiguration(const string& configurationFilePath, const vector<string>& validConfigKeys) {

    std::ifstream configFile(configurationFilePath);

    std::string line;
    while (std::getline(configFile, line)) {
        vector<string> configParam;
        getConfigParam(line, configParam);

        if (configParam.empty()) {
            continue;
        }

        if (configParam.size() != 2) {
            return CONFIG_PARSING_ERROR_INVALID_CONFIG_LINE;
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

    return SUCCESS;
}

void FunctionalTestConfiguration::getConfigParam(string& configLine, vector<string>& configParam) {
    stringstream ss(configLine);
    string token;
    while (getline(ss, token, '=')) {
        configParam.emplace_back(token);
    }
}

bool FunctionalTestConfiguration::isConfigValid(const vector<string>& validConfigKeys) {
    for (auto& key : validConfigKeys) {
        if (configParamsMap.count(key) == 0 || configParamsMap[key].empty()) {
            return false;
        }
    }
    return true;
}