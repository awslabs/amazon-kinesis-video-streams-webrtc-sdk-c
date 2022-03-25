//
// Created by Katey, Anurag on 3/16/22.
//

#ifndef KINESISVIDEOWEBRTCCLIENT_FUNCTIONALTESTCONFIG_H
#define KINESISVIDEOWEBRTCCLIENT_FUNCTIONALTESTCONFIG_H

#include <string>
#include "../utils/Common.h"

using namespace std;

static const vector<string> VALID_TEST_CONFIG_KEYS = {
        "CONTAINER_IMAGE",
        "TEST_EXECUTION_TIME_SECONDS",
        "CLEAN_ARTIFACTS_ON_EXIT",
        "UPLOAD_ARTIFACTS_TO_S3_ON_FAILURE",

};

class FunctionalTestConfiguration : public FunctionalTestConfiguration {
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
    string masterContainerName, viewerContainerName;
    string signalingCPServerContainerName, signalingDPServerContainerName;

    explicit FunctionalTestConfiguration() {}

    explicit FunctionalTestConfiguration(string testIdentifier) {
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
};

#endif //KINESISVIDEOWEBRTCCLIENT_FUNCTIONALTESTCONFIG_H
