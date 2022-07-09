#ifndef KINESISVIDEOWEBRTCCLIENT_ABSTRACTFUNCTIONALTESTCONFIGURATION_H
#define KINESISVIDEOWEBRTCCLIENT_ABSTRACTFUNCTIONALTESTCONFIGURATION_H

#include <unordered_map>
#include <vector>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>

using namespace std;

class AbstractFunctionalTestConfiguration {
public:
    unordered_map<string, string> configParamsMap;

    AbstractFunctionalTestConfiguration();
    int parseConfiguration(const string& configurationFilePath);
    bool validateConfiguration(const vector<string>& expectedConfigKeys);

    void parseConfigParam(string& configLine, vector<string>& configParam);
    bool isConfigValid(const vector<string>& validConfigKeys);
};


#endif //KINESISVIDEOWEBRTCCLIENT_ABSTRACTFUNCTIONALTESTCONFIGURATION_H
