#ifndef KINESISVIDEOWEBRTCCLIENT_CONTAINERSDKPROCESSMANAGER_H
#define KINESISVIDEOWEBRTCCLIENT_CONTAINERSDKPROCESSMANAGER_H

#include <string>

using namespace std;

class ProcessManager {
    const string GET_CONTAINER_ID_COMMAND_PREFIX = "docker ps -q -f name=";

  public:
    string getContainerPid(const string& containerName);
};

#endif // KINESISVIDEOWEBRTCCLIENT_CONTAINERSDKPROCESSMANAGER_H
