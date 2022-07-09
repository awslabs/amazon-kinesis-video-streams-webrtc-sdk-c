#include "PeerConfiguration.h"
#include "FunctionalTestOrchestratorConfiguration.h"
#include <sstream>
#include <fstream>
#include <algorithm>
#include "../utils/ErrorCodes.h"

FunctionalTestOrchestratorConfiguration::FunctionalTestOrchestratorConfiguration() {}

FunctionalTestOrchestratorConfiguration::FunctionalTestOrchestratorConfiguration(string testIdentifier) {
    this->testIdentifier = testIdentifier;

    testArtifactsDirectory.append(TEST_ARTIFACTS_ROOT);
    testArtifactsDirectory.append("/test-");
    testArtifactsDirectory.append(this->testIdentifier);

    testArtifactsLoggingDirectory = testArtifactsDirectory;
    testArtifactsLoggingDirectory.append("/logs");

    testArtifactsMarkerFileDirectory = testArtifactsDirectory;
    testArtifactsMarkerFileDirectory.append("/marker_files");

    // container configuration
    containerImageName = "webrtc-alpine-image";

    masterContainerName = "master-container-test-";
    masterContainerName.append(testIdentifier);

    viewerContainerName = "viewer-container-test-";
    viewerContainerName.append(testIdentifier);

    signalingCPServerContainerName = "signaling-cp-container-test-";
    signalingCPServerContainerName.append(testIdentifier);

    signalingDPServerContainerName = "signaling-dp-container-test-";
    signalingDPServerContainerName.append(testIdentifier);

    containerImageName = "webrtc-alpine-image";
}

int FunctionalTestOrchestratorConfiguration::parsePeerConfiguration(const string& configurationFilePath) {
    return parseConfiguration(configurationFilePath, VALID_PEER_CONFIG_KEYS);
}

int FunctionalTestOrchestratorConfiguration::parseConfiguration(
        const string& configurationFilePath, const vector<string>& validConfigKeys) {

    std::ifstream configFile(configurationFilePath);

    std::string line;
    while (std::getline(configFile, line)) {
        vector<string> configParam;
        parseConfigParam(line, configParam);

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

bool FunctionalTestOrchestratorConfiguration::isConfigValid(const vector<string>& validConfigKeys) {
    for (auto& key : validConfigKeys) {
        if (configParamsMap.count(key) == 0 || configParamsMap[key].empty()) {
            return false;
        }
    }
    return true;
}