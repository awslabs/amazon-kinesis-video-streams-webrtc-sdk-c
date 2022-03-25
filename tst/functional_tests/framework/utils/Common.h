//
// Created by Katey, Anurag on 3/17/22.
//

#ifndef KINESISVIDEOWEBRTCCLIENT_CONSTANTS_H
#define KINESISVIDEOWEBRTCCLIENT_CONSTANTS_H

#include <string>

using namespace std;

const string TEST_ROOT = "/tmp/webrtc_functional_tests";
const string TEST_SOURCE_CODE_ROOT = TEST_ROOT + "/source/";
const string TEST_SOURCE_CODE_CONTAINER_MAPPED_DIR = "/tmp/test-sdk-source/";
const string TEST_WEBRTC_SDK_SOURCE_CODE_ROOT = TEST_ROOT + "/source/amazon-kinesis-video-streams-webrtc-sdk-c/";
const string TEST_ARTIFACTS_ROOT = TEST_ROOT + "/test_artifacts/";

const string SCRIPTS_ROOT = "/tmp/scripts";
const string SETUP_TEST_DIRECTORY_SCRIPT = SCRIPTS_ROOT + "/setup_test_directories.sh";
const string BUILD_WEBRTC_SDK_SOURCE = SCRIPTS_ROOT + "/sdk_container_build.sh";
const string SETUP_MOCK_CONTROL_PLANE_SERVER_SCRIPT = SCRIPTS_ROOT + "/setup_mock_controlplane_server.sh";
const string SETUP_MOCK_DATA_PLANE_SERVER_SCRIPT = "/tmp/scripts/setup_mock_dataplane_server.sh";
const string SYNC_SOURCE_CODE_WITH_CONTAINER = "/tmp/scripts/sync_source.sh";

const string GET_SIGNALING_CONTROL_PLANE_SERVER_PID_COMMAND = "pgrep -f setup_mock_controlplane_server.sh.py";
const string GET_SIGNALING_DATA_PLANE_SERVER_PID_COMMAND = "pgrep -f setup_mock_controlplane_server.sh.py";

const string DOCKER_NETWORK_NAME = "webrtc_peer_network";
const string DOCKER_NETWORK_COMMAND = "docker network create --subnet=172.22.0.0/16 webrtc_peer_network";
const string MASTER_IP = "172.22.0.10";
const string VIEWER_IP = "172.22.0.11";
const string SIGNALING_CP_SERVER_IP = "172.22.0.12";
const string SIGNALING_DP_SERVER_IP = "172.22.0.13";

const string SIGNALING_CP_SERVER_PORT = "5443";
const string SIGNALING_DP_SERVER_PORT = "8765";

const string KILL_CONTAINER_COMMAND_PREFIX = "docker rm -f ";
const string DELETE_DOCKER_NETWORK_COMMAND = "docker network rm webrtc_peer_network";

#define SUCCESS 0

#endif //KINESISVIDEOWEBRTCCLIENT_CONSTANTS_H
