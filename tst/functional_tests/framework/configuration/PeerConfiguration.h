#ifndef KINESISVIDEOWEBRTCCLIENT_PEERCONFIGURATION_H
#define KINESISVIDEOWEBRTCCLIENT_PEERCONFIGURATION_H

#include "AbstractFunctionalTestConfiguration.h"
#include <string>
#include <vector>
#include <unordered_map>

using namespace std;

enum PeerType {
    MASTER,
    VIEWER
};

static const vector<string> VALID_PEER_CONFIG_KEYS = {
        "PEER_TYPE",
        "SIGNALING_CHANNEL_NAME",
        "USE_TURN",
        "USE_TRICKLE_ICE",
        "LOG_DIRECTORY",
        "WEBRTC_SIGNALING_CONTROL_PLANE_IP",
        "WEBRTC_SIGNALING_CONTROL_PLANE_PORT",
        "WEBRTC_SIGNALING_DATA_PLANE_IP",
        "WEBRTC_SIGNALING_DATA_PLANE_PORT"
};

class PeerConfiguration : public AbstractFunctionalTestConfiguration {
public:
    PeerType getPeerType();
    char* getChannelName();
    bool useTrickleIce();
    bool useTurn();
    char* getLogDirectoryPath();
    char* getMasterMetricsFilePath();
    char* getViewerMetricsFilePath();

    int parsePeerConfiguration(const string& configurationFilePath);
    int parseConfiguration(const string& configurationFilePath, const vector<string>& validConfigKeys);
};


#endif //KINESISVIDEOWEBRTCCLIENT_PEERCONFIGURATION_H
