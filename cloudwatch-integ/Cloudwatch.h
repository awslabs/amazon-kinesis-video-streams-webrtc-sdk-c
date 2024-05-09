#pragma once

#include "Include.h"
#include "CloudwatchLogs.h"
#include "CloudwatchMonitoring.h"

namespace CppInteg {

class Cloudwatch {
  public:
    Cloudwatch() = delete;
    Cloudwatch(Cloudwatch const&) = delete;
    void operator=(Cloudwatch const&) = delete;

    CloudwatchLogs logs;
    CloudwatchMonitoring monitoring;

    static Cloudwatch& getInstance();
    static STATUS init(PCHAR channelName, PCHAR region, BOOL isMaster);
    static VOID deinit();
    static VOID logger(UINT32, PCHAR, PCHAR, ...);

  private:
    static Cloudwatch& getInstanceImpl(ClientConfiguration* = nullptr);

    Cloudwatch(ClientConfiguration*);
    BOOL terminated;
};
typedef Cloudwatch* PCloudwatch;

} // namespace Canary
