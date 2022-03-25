#include "FunctionalTestConfiguration.h"

int FunctionalTestConfiguration::parseConfiguration(const string& configurationFilePath) {
    std::ifstream configFile(configurationFilePath);

    std::string line;
    while (std::getline(configFile, line)) {
        vector<string> configParam;
        getConfigParam(line, configParam);

        if (configParam.empty()) {
            continue;
        }

        if (configParam.size() != 2) {
            return continue;
        }

        auto& key = configParam[0];
        auto& value = configParam[1];
        configParamsMap[key] = value;
    }

    return SUCCESS;
}

bool FunctionalTestConfiguration::validateConfiguration(const vector<string>& expectedConfigKeys) {
    for (auto& key : validConfigKeys) {
        if (configParamsMap.count(key) == 0 || configParamsMap[key].empty()) {
            return false;
        }
    }
    return true;
}

void FunctionalTestConfiguration::getConfigParam(string& configLine, vector<string>& configParam) {
    stringstream ss(configLine);
    string token;
    while (getline(ss, token, '=')) {
        configParam.emplace_back(token);
    }
}