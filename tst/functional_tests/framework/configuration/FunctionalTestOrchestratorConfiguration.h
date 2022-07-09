//
// Created by Katey, Anurag on 3/16/22.
//

#ifndef KINESISVIDEOWEBRTCCLIENT_FUNCTIONALTESTORCHESTRATORCONFIGURATION_H
#define KINESISVIDEOWEBRTCCLIENT_FUNCTIONALTESTORCHESTRATORCONFIGURATION_H

#include <string>
#include "../utils/Common.h"
#include "AbstractFunctionalTestConfiguration.h"

using namespace std;

static const vector<string> VALID_TEST_CONFIG_KEYS = {
        "CONTAINER_IMAGE",
        "TEST_EXECUTION_TIME_SECONDS",
        "CLEAN_ARTIFACTS_ON_EXIT",
        "UPLOAD_ARTIFACTS_TO_S3_ON_FAILURE"
};

class FunctionalTestOrchestratorConfiguration : public AbstractFunctionalTestConfiguration {
public:

    string testIdentifier;
    string testArtifactsDirectory;
    string testArtifactsLoggingDirectory;
    string testArtifactsMarkerFileDirectory;

    string webRTCControlPlaneServerConfigPath;
    string webRTCDataPlaneServerConfigPath;

    string webRTCMockServersScripts;
    string webRTCDataPlaneServerFile;
    string scripts;
    bool preserveTestArtifacts;
    string sourceRepoPath;
    string repoName;

    // container configuration
    string containerImageName;
    string masterContainerName;
    string viewerContainerName;
    string signalingCPServerContainerName;
    string signalingDPServerContainerName;

    FunctionalTestOrchestratorConfiguration(string testIdentifier);
    FunctionalTestOrchestratorConfiguration();

    int parsePeerConfiguration(const string& configurationFilePath);
    int parseConfiguration(const string& configurationFilePath, const vector<string>& validConfigKeys);
    bool isConfigValid(const vector<string>& validConfigKeys);
};

#endif //KINESISVIDEOWEBRTCCLIENT_FUNCTIONALTESTORCHESTRATORCONFIGURATION_H
