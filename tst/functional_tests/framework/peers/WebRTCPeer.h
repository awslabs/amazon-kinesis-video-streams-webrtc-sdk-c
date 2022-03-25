
#ifndef KINESISVIDEOWEBRTCCLIENT_WEBRTCMASTER_H
#define KINESISVIDEOWEBRTCCLIENT_WEBRTCMASTER_H

#include "../../../Samples.h"
#include "../configuration/PeerConfiguration.h"
#include "../logger/FunctionalTestLogger.h"

STATUS startMaster(PeerConfiguration& functionalTestConfiguration);
STATUS startViewer(PeerConfiguration& functionalTestConfiguration);

#endif //KINESISVIDEOWEBRTCCLIENT_WEBRTCMASTER_H
