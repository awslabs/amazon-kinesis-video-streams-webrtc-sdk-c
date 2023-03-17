//
// Created by Katey, Anurag on 3/16/22.
//

#ifndef KINESISVIDEOWEBRTCCLIENT_UTILS_H
#define KINESISVIDEOWEBRTCCLIENT_UTILS_H

#include <cstdio>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <array>
#include <vector>
#include <cstdlib>

using namespace std;

class Utils {
  public:
    static int executeBashCommand(const string& command)
    {
        return system(command.c_str());
    }

    static int executeBashScript(const string& scriptPath, string currentTime)
    {
        string command = "bash " + scriptPath;
        return system(command.c_str());
    }

    static int executeBashCommands(const vector<string>& commands)
    {
        auto status = 0;
        for (auto& command : commands) {
            status = system(command.c_str());
            if (status != 0) {
                return status;
            }
        }

        return status;
    }

    static std::string executeBashCommand(const char* cmd)
    {
        std::array<char, 1024> buffer;
        std::string result;
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
        if (!pipe) {
            return "";
        }
        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
            result += buffer.data();
        }
        return result;
    }
};

#endif // KINESISVIDEOWEBRTCCLIENT_UTILS_H

/*
 *      docker network inspect webrtc_peer_network
 *
 */
