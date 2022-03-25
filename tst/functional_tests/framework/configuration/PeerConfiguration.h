//
// Created by Katey, Anurag on 3/20/22.
//

#ifndef KINESISVIDEOWEBRTCCLIENT_FUNCTIONALTESTCONFIGURATION_H
#define KINESISVIDEOWEBRTCCLIENT_FUNCTIONALTESTCONFIGURATION_H

#include "../logger/FunctionalTestLogger.h"
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
        "USE_TURN",
        "USE_TRICKLE_ICE",
        "LOG_DIRECTORY",
        "WEBRTC_SIGNALING_CONTROL_PLANE_IP",
        "WEBRTC_SIGNALING_CONTROL_PLANE_PORT",
        "WEBRTC_SIGNALING_DATA_PLANE_IP",
        "WEBRTC_SIGNALING_DATA_PLANE_PORT"
};

class PeerConfiguration : public FunctionalTestConfiguration {
public:
    PeerType getPeerType();
    char* getChannelName();
    bool useTrickleIce();
    bool useTurn();
    char* getLogDirectoryPath();
};


#endif //KINESISVIDEOWEBRTCCLIENT_FUNCTIONALTESTCONFIGURATION_H
