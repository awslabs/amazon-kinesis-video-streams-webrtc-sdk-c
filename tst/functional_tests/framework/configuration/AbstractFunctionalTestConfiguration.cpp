#include "AbstractFunctionalTestConfiguration.h"
#include "../utils/ErrorCodes.h"

AbstractFunctionalTestConfiguration::AbstractFunctionalTestConfiguration() {

}

int AbstractFunctionalTestConfiguration::parseConfiguration(const string& configurationFilePath) {
    std::ifstream configFile(configurationFilePath);

    std::string line;
    while (getline(configFile, line)) {
        vector<string> configParam;
        parseConfigParam(line, configParam);

        if (configParam.empty()) {
            continue;
        }

        if (configParam.size() != 2) {
            return CONFIG_PARSING_ERROR_INVALID_CONFIG_KEY;
        }

        auto& key = configParam[0];
        auto& value = configParam[1];
        configParamsMap[key] = value;
    }

    return TEST_SUCCESS;
}

bool AbstractFunctionalTestConfiguration::validateConfiguration(const vector<string>& expectedConfigKeys) {
    for (auto& key : expectedConfigKeys) {
        if (configParamsMap.count(key) == 0 || configParamsMap[key].empty()) {
            return false;
        }
    }
    return true;
}

void AbstractFunctionalTestConfiguration::parseConfigParam(string& configLine, vector<string>& configParam) {
    stringstream ss(configLine);
    string token;
    while (getline(ss, token, '=')) {
        configParam.emplace_back(token);
    }
}

bool AbstractFunctionalTestConfiguration::isConfigValid(const vector<string>& validConfigKeys) {
    for (auto& key : validConfigKeys) {
        if (configParamsMap.count(key) == 0 || configParamsMap[key].empty()) {
            return false;
        }
    }
    return true;
}