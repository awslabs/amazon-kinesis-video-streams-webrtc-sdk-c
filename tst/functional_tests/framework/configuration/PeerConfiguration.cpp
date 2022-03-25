//
// Created by Katey, Anurag on 3/20/22.
//

#include "PeerConfiguration.h"
#include <sstream>
#include <fstream>
#include "../utils/ErrorCodes.h"

int PeerConfiguration::parsePeerConfiguration(const string& configurationFilePath) {
    return parseConfiguration(configurationFilePath, VALID_PEER_CONFIG_KEYS);
}

int PeerConfiguration::parseConfiguration(const string& configurationFilePath, const vector<string>& validConfigKeys) {

    std::ifstream configFile(configurationFilePath);

    std::string line;
    while (std::getline(configFile, line)) {
        vector<string> configParam;
        getConfigParam(line, configParam);

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

    return SUCCESS;
}

void PeerConfiguration::getConfigParam(string& configLine, vector<string>& configParam) {
    stringstream ss(configLine);
    string token;
    while (getline(ss, token, '=')) {
        configParam.emplace_back(token);
    }
}

bool PeerConfiguration::isConfigValid(const vector<string>& validConfigKeys) {
    for (auto& key : validConfigKeys) {
        if (configParamsMap.count(key) == 0 || configParamsMap[key].empty()) {
            return false;
        }
    }
    return true;
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