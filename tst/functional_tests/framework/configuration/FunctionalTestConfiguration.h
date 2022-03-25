//
// Created by Katey, Anurag on 3/21/22.
//

#ifndef KINESISVIDEOWEBRTCCLIENT_FUNCTIONALTESTCONFIGURATION_H
#define KINESISVIDEOWEBRTCCLIENT_FUNCTIONALTESTCONFIGURATION_H

class FunctionalTestConfiguration {
public:
    int parseConfiguration(const string& configurationFilePath);
    bool validateConfiguration(const vector<string>& expectedConfigKeys);

private:
    unordered_map<string, string> configParamsMap;
    void getConfigParam(string& configLine, vector<string>& configParam);
};


#endif //KINESISVIDEOWEBRTCCLIENT_FUNCTIONALTESTCONFIGURATION_H
