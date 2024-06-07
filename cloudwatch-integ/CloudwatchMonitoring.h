#pragma once

#include "../samples/Samples.h"
namespace CppInteg {

class CloudwatchMonitoring {
  public:
    CloudwatchMonitoring(ClientConfiguration*);
    STATUS init(PCHAR channelName, PCHAR region, BOOL isMaster, BOOL isStorage);
    VOID deinit();
    VOID push(const MetricDatum&);
    VOID pushExitStatus(STATUS);
    VOID pushSignalingRoundtripStatus(STATUS);
    VOID pushSignalingInitDelay(UINT64, Aws::CloudWatch::Model::StandardUnit);
    VOID pushTimeToFirstFrame(UINT64, Aws::CloudWatch::Model::StandardUnit);
    VOID pushSignalingRoundtripLatency(UINT64, Aws::CloudWatch::Model::StandardUnit);
    VOID pushSignalingConnectionDuration(UINT64, Aws::CloudWatch::Model::StandardUnit);
    VOID pushOutboundRtpStats(POutgoingRTPStatsCtx);
    VOID pushInboundRtpStats(PIncomingRTPStatsCtx);
    VOID pushPeerConnectionMetrics(PPeerConnectionMetrics);
    VOID pushKvsIceAgentMetrics(PKvsIceAgentMetrics);
    VOID pushSignalingClientMetrics(PSignalingClientMetrics);
    VOID pushEndToEndMetrics(PEndToEndMetricsCtx);
    VOID pushRetryCount(UINT32);

    VOID pushStorageDisconnectToFrameSentTime(UINT64, Aws::CloudWatch::Model::StandardUnit);
    VOID pushJoinSessionTime(UINT64, Aws::CloudWatch::Model::StandardUnit);

  private:
    Dimension channelDimension;
    Dimension labelDimension;
    CloudWatchClient client;
    std::atomic<UINT64> pendingMetrics;
    BOOL isStorage;
};

} // namespace CppInteg
