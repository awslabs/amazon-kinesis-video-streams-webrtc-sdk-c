#pragma once

namespace CppInteg {

class CloudwatchLogs {
  public:
    CloudwatchLogs(ClientConfiguration*);
    STATUS init(PCHAR channelName, PCHAR region, BOOL isMaster, BOOL isStorage);
    VOID deinit();
    VOID push(string log);
    VOID flush(BOOL sync = FALSE);
    std::string logGroupName, logStreamName;

  private:
    class Synchronization {
      public:
        std::atomic<bool> pending;
        std::recursive_mutex mutex;
        std::condition_variable_any await;
    };

    CloudWatchLogsClient client;
    Synchronization sync;
    Aws::Vector<InputLogEvent> logs;
    Aws::String token;
};

} // namespace Canary
