#pragma once

#include "Config.h"
#include "../samples/Samples.h"
namespace CppInteg {

    class CloudwatchMetricsMonitoring {
    public:
        CloudwatchMetricsMonitoring(CppInteg::PConfig, ClientConfiguration*);
        STATUS init(CppInteg::PConfig);
        VOID deinit();
        static CloudwatchMetricsMonitoring& getInstance();
        VOID pushOutboundRtpStats(POutgoingRTPMetricsContext pOutboundRtpStats);
        VOID pushInboundRtpStats(PIncomingRTPMetricsContext pIncomingRtpStats);

    private:
        static CloudwatchMetricsMonitoring& getInstanceImpl(CppInteg::PConfig = nullptr, ClientConfiguration* = nullptr);
        Dimension channelDimension;
        Dimension labelDimension;
        PConfig pConfig;
        VOID push(const MetricDatum&);
        CloudWatchClient client;
        std::atomic<UINT64> pendingMetrics;
        BOOL isStorage;
    };

} // namespace Canary
