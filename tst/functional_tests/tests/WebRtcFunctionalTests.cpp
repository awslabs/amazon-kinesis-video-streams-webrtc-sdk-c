#include "../../WebRTCClientTestFixture.h"
#include "../framework/orchestrator/FunctionalTestOrchestrator.h"
#include "../framework/configuration/FunctionalTestOrchestratorConfiguration.h"
#include "../framework/configuration/AbstractFunctionalTestConfiguration.h"
#include "FunctionalTestPeerRunner.h"

#include <ctime>
#include <thread>
#include <chrono>
#include <cassert>

namespace com {
namespace amazonaws {
namespace kinesis {
namespace video {
namespace webrtcclient {

class WebRtcFunctionalTests : public WebRtcClientTestBase {
};

TEST_F(WebRtcFunctionalTests, start_peer)
{
    EXPECT_EQ(TEST_SUCCESS, startPeer());
}

TEST_F(WebRtcFunctionalTests, test_basic_peer_to_peer_communication)
{
    string testIdentifier = to_string(time(nullptr));
    testIdentifier = "1649744247";
    FunctionalTestOrchestratorConfiguration functionalTestOrchestratorConfiguration(testIdentifier);

    functionalTestOrchestratorConfiguration.testIdentifier = "sample";
    functionalTestOrchestratorConfiguration.webRTCControlPlaneServerConfigPath = "/tmp/webrtc_test_config/test_1/control_plane_server_config";
    functionalTestOrchestratorConfiguration.webRTCDataPlaneServerConfigPath = "/tmp/webrtc_test_config/test_1/data_plane_server_config";
    functionalTestOrchestratorConfiguration.webRTCMockServersScripts = "/tmp/scripts/setup_mock_servers.sh";
    functionalTestOrchestratorConfiguration.webRTCDataPlaneServerFile = "/tmp/scripts/signaling_websockets_server.py";
    functionalTestOrchestratorConfiguration.scripts = "/tmp/scripts";
    functionalTestOrchestratorConfiguration.preserveTestArtifacts = false;
    functionalTestOrchestratorConfiguration.sourceRepoPath = "~/kvs_git_workspace/amazon-kinesis-video-streams-webrtc-sdk-c";
    functionalTestOrchestratorConfiguration.repoName = "amazon-kinesis-video-streams-webrtc-sdk-c";

    cout << "\n ************ testArtifactsDirectoryRoot = " << functionalTestOrchestratorConfiguration.testArtifactsDirectory;

    FunctionalTestOrchestrator functionalTestOrchestrator(functionalTestOrchestratorConfiguration);

    EXPECT_EQ(SUCCESS, functionalTestOrchestrator.setupTestResources());

    EXPECT_EQ(SUCCESS, functionalTestOrchestrator.syncSourceCode());
    EXPECT_EQ(SUCCESS, functionalTestOrchestrator.buildSourceCodeOnMasterContainer(false /* clean build */));
    EXPECT_EQ(SUCCESS, functionalTestOrchestrator.launchSignalingControlPlaneServer());
    EXPECT_EQ(SUCCESS, functionalTestOrchestrator.launchSignalingDataPlaneServer());



    //EXPECT_EQ(SUCCESS, functionalTestOrchestrator.shutDownSignalingControlPlaneServer());
    //EXPECT_EQ(SUCCESS, functionalTestOrchestrator.shutDownSignalingDataPlaneServer());



//    int counter = 5;
//    while (counter-- > 0) {
//        this_thread::sleep_for(std::chrono::seconds(1));
//        functionalTestOrchestrator.checkAllResourcesRunning();
//    }
    //functionalTestOrchestrator.cleanTestResources();
//    EXPECT_EQ(SUCCESS, functionalTestOrchestrator.setupTestResources());
//    EXPECT_EQ(SUCCESS, functionalTestOrchestrator.checkAllResourcesRunning());
//    EXPECT_EQ(SUCCESS, functionalTestOrchestrator.startMaster());
//    EXPECT_EQ(SUCCESS, functionalTestOrchestrator.isolateMasterAndViewerNetwork());
//    EXPECT_EQ(SUCCESS, functionalTestOrchestrator.startViewer());
//    EXPECT_EQ(SUCCESS, functionalTestOrchestrator.setupResourceMonitoringThread());
//    EXPECT_EQ(SUCCESS, functionalTestOrchestrator.waitForTestToTerminate());
//    EXPECT_EQ(SUCCESS, functionalTestOrchestrator.validateMetrics());
//    EXPECT_EQ(SUCCESS, functionalTestOrchestrator.uploadTestArtifacts());


    /*
        Disconnect peer to peer connectivity and verify turn
     */


}

} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com
