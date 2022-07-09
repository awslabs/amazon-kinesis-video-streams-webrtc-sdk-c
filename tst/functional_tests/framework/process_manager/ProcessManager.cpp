//
// Created by Katey, Anurag on 3/16/22.
//

#include "ProcessManager.h"
#include "../utils/Common.h"
#include "../utils/Utils.h"

string ProcessManager::getContainerPid(const string& containerName) {
    if (containerName.empty()) {
        return "";
    }
    string command = GET_CONTAINER_ID_COMMAND_PREFIX + containerName;
    return Utils::executeBashCommand(command.c_str());
}