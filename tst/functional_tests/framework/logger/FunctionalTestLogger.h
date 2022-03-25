//
// Created by Katey, Anurag on 3/20/22.
//

#ifndef KINESISVIDEOWEBRTCCLIENT_FUNCTIONALTESTLOGGER_H
#define KINESISVIDEOWEBRTCCLIENT_FUNCTIONALTESTLOGGER_H

#include <string>
using namespace std;

class FunctionalTestLogger {
    const int MAX_LOG_LINE_LENGTH = 1024;
    string formatLogLine(const char* msg, ...);
public:

    explicit FunctionalTestLogger();
    explicit FunctionalTestLogger(const string& logFile);

    static shared_ptr<FunctionalTestLogger> getInstance() {
        static auto singletonInstance = make_shared<FunctionalTestLogger>();
        return singletonInstance;
    }

    void debug(const char* msg, ...);
    void error(const char* msg, ...);
    void info(const char* msg, ...);
};


#endif //KINESISVIDEOWEBRTCCLIENT_FUNCTIONALTESTLOGGER_H
