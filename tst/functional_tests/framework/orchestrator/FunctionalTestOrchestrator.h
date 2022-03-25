//
// Created by Katey, Anurag on 3/16/22.
//

#ifndef KINESISVIDEOWEBRTCCLIENT_FUNCTIONALTESTORCHESTRATOR_H
#define KINESISVIDEOWEBRTCCLIENT_FUNCTIONALTESTORCHESTRATOR_H

#include "../configuration/FunctionalTestConfig.h"
#include <string>
#include "../utils/Common.h"
#include "../process_manager/ProcessManager.h"

using namespace std;

class FunctionalTestOrchestrator {
private:
    FunctionalTestConfiguration functionalTestConfig;
    ProcessManager processManager;

    int createTestDirectory();
    int launchSignalingControlPlaneServerContainer();
    int launchSignalingDataPlaneServerContainer();
    int launchMasterContainer();
    int launchViewerContainer();

    int startMasterApplication();
    int startViewerApplication();

    int launchContainer(const string& containerName,
                        const string& containerIp,
                        const string& network,
                        const string& portForwardingMapping);

    string getSignalingControlPlaneServerPid();
    string getSignalingDataPlaneServerPid();

public:

    explicit FunctionalTestOrchestrator(FunctionalTestConfiguration& functionalTestConfig);

    int setupTestResources();
    void cleanTestResources();

    int syncSourceCode();
    int buildSourceCodeOnMasterContainer();

    bool checkAllResourcesRunning();
    bool ensureNoTestResourcesRunning();

    int isolateMasterAndViewerNetwork();
    int setupResourceMonitoringThread();
    int waitForTestToTerminate();
    int validateMetrics();
    int uploadTestArtifacts();
    int cleanUpTestArtifacts();
};

#endif //KINESISVIDEOWEBRTCCLIENT_FUNCTIONALTESTORCHESTRATOR_H
