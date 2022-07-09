#include <ctime>
#include <thread>
#include <chrono>
#include <unistd.h>
#include <string>
#include  <iostream>

#include "../framework/peers/WebRTCPeer.h"
#include "../framework/peers/Samples.h"
#include "../framework/utils/ErrorCodes.h"
#include "../framework/configuration/PeerConfiguration.h"
#include "../framework/utils/ErrorCodes.h"

using namespace std;

/*

    export PEER_TEST_CONFIGURATION_FILE_PATH="/tmp/webrtc_functional_tests/test_artifacts/test-1647833030/logs/sdk-build-log"
    export PEER_LOG_FILE="/tmp/webrtc_functional_tests/test_artifacts/test-1647833030/logs/sdk-build-log"
    export MASTER_METRICS_FILE="/tmp/webrtc_functional_tests/test_artifacts/test-1647833030/logs/sdk-build-log"
 */

int startPeer() {

    char* peerConfigurationFilePath = getenv("PEER_TEST_CONFIGURATION_FILE_PATH");
    if (peerConfigurationFilePath == nullptr) {
        cout << "[Error] peerConfigurationFilePath is required";
        return PEER_CONFIGURATION_FILE_MISSING;
    }

    // Config file does not exist
    if (access(peerConfigurationFilePath, F_OK) != 0) {
        cout << "[Error] Test configuration file is not present at the path " << peerConfigurationFilePath;
        return PEER_CONFIGURATION_FILE_INVALID_PATH;
    }

    PeerConfiguration peerConfig;
    auto ret = peerConfig.parsePeerConfiguration(peerConfigurationFilePath);
    if (ret != TEST_SUCCESS) {
        cout << "[Error] Test configuration file is not present at the path " << peerConfigurationFilePath;
        return ret;
    }

    switch (peerConfig.getPeerType()) {
        case MASTER:
            cout << "[Debug] Test configuration peer type - Master";
            startMaster(peerConfig);
            break;
        case VIEWER:
            cout << "[Debug] Test configuration peer type - Viewer";
            startViewer(peerConfig);
            break;
        default:
            cout << "[Error] Invalid peer type in test configuration. Expected either master or viewer" << peerConfigurationFilePath;
            return GENERIC_FAILURE;
    }

    return TEST_SUCCESS;
}
