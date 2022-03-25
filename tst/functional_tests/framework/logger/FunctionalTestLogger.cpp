//
// Created by Katey, Anurag on 3/20/22.
//

#include "FunctionalTestLogger.h"
#include  <iostream>

FunctionalTestLogger::FunctionalTestLogger() {}
FunctionalTestLogger::FunctionalTestLogger(const string& logFile) {}

string FunctionalTestLogger::formatLogLine(const char* msg, ...) {
    char buf[MAX_LOG_LINE_LENGTH];
    va_list argptr;
    va_start(argptr, msg);
    vsprintf(buf, msg, argptr);
    va_end(argptr);
    return string(buf);
}

void FunctionalTestLogger::debug(const char* msg, ...) {
    auto logLine = formatLogLine(msg);
    cout << "\n DEBUG = " << logLine;
}

void FunctionalTestLogger::error(const char* msg, ...) {
    auto logLine = formatLogLine(msg);
    cout << "\n ERROR = " << logLine;
}

void FunctionalTestLogger::info(const char* msg, ...) {
    auto logLine = formatLogLine(msg);
    cout << "\n INFO = " << logLine;
}
