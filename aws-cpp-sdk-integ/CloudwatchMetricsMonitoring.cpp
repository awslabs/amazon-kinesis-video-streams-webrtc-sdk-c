#include "Include.h"
#include "Config.h"
#include "CloudwatchMetricsMonitoring.h"

namespace CppInteg {
    CloudwatchMetricsMonitoring::CloudwatchMetricsMonitoring(PConfig pConfig, ClientConfiguration* pClientConfig) : pConfig(pConfig), client(*pClientConfig)
    {
        pConfig->isStorage ? this->isStorage = true : this->isStorage = false;
    }

    STATUS CloudwatchMetricsMonitoring::init(CppInteg::PConfig pConfig)
    {
        ENTERS();
        STATUS retStatus = STATUS_SUCCESS;
        ClientConfiguration clientConfig;

        clientConfig.region = pConfig->region.value;
        auto& instance = getInstanceImpl(pConfig, &clientConfig);

        this->isStorage ? this->channelDimension.SetName(INDIVIDUAL_STORAGE_CW_DIMENSION) : this->channelDimension.SetName(INDIVIDUAL_CW_DIMENSION);
        this->channelDimension.SetValue(pConfig->channelName.value);

        this->isStorage ? this->labelDimension.SetName(AGGREGATE_STORAGE_CW_DIMENSION) : this->labelDimension.SetName(AGGREGATE_CW_DIMENSION);
        this->labelDimension.SetValue(pConfig->label.value);
        CleanUp:

        LEAVES();
        return retStatus;
    }

    static const CHAR* unitToString(const Aws::CloudWatch::Model::StandardUnit& unit)
    {
        switch (unit) {
            case Aws::CloudWatch::Model::StandardUnit::Count:
                return "Count";
            case Aws::CloudWatch::Model::StandardUnit::Count_Second:
                return "Count_Second";
            case Aws::CloudWatch::Model::StandardUnit::Milliseconds:
                return "Milliseconds";
            case Aws::CloudWatch::Model::StandardUnit::Percent:
                return "Percent";
            case Aws::CloudWatch::Model::StandardUnit::None:
                return "None";
            case Aws::CloudWatch::Model::StandardUnit::Kilobits_Second:
                return "Kilobits_Second";
            default:
                return "Unknown unit";
        }
    }

    CloudwatchMetricsMonitoring& CloudwatchMetricsMonitoring::getInstance()
    {
        return getInstanceImpl();
    }

    CloudwatchMetricsMonitoring& CloudwatchMetricsMonitoring::getInstanceImpl(CppInteg::PConfig pConfig, ClientConfiguration* pClientConfig)
    {
        static CloudwatchMetricsMonitoring instance{pConfig, pClientConfig};
        return instance;
    }

    VOID CloudwatchMetricsMonitoring::deinit()
    {
        auto& instance = getInstance();
        while (this->pendingMetrics.load() > 0) {
            THREAD_SLEEP(HUNDREDS_OF_NANOS_IN_A_MILLISECOND * 500);
        }
    }

    VOID CloudwatchMetricsMonitoring::push(const MetricDatum& datum)
    {
        Aws::CloudWatch::Model::PutMetricDataRequest cwRequest;
        MetricDatum single = datum;
        MetricDatum aggregated = datum;

        single.AddDimensions(this->channelDimension);
        single.AddDimensions(this->labelDimension);

        cwRequest.SetNamespace(DEFAULT_CLOUDWATCH_NAMESPACE);
        cwRequest.AddMetricData(single);


        auto asyncHandler = [this](const Aws::CloudWatch::CloudWatchClient* cwClient, const Aws::CloudWatch::Model::PutMetricDataRequest& request,
                                   const Aws::CloudWatch::Model::PutMetricDataOutcome& outcome,
                                   const std::shared_ptr<const Aws::Client::AsyncCallerContext>& context) {
            UNUSED_PARAM(cwClient);
            UNUSED_PARAM(request);
            UNUSED_PARAM(context);

            if (!outcome.IsSuccess()) {
                DLOGE("Failed to put sample metric data: %s", outcome.GetError().GetMessage().c_str());
            } else {
                DLOGS("Successfully put sample metric data");
            }
            this->pendingMetrics--;
        };
        this->pendingMetrics++;
        this->client.PutMetricDataAsync(cwRequest, asyncHandler);

        std::stringstream ss;

        ss << "Emitted the following metric:\n\n";
        ss << "  Name       : " << datum.GetMetricName() << '\n';
        ss << "  Unit       : " << unitToString(datum.GetUnit()) << '\n';

        ss << "  Values     : ";
        auto& values = datum.GetValues();
        // If the datum uses single value, GetValues will be empty and the data will be accessible
        // from GetValue
        if (values.empty()) {
            ss << datum.GetValue();
        } else {
            for (auto i = 0; i < values.size(); i++) {
                ss << values[i];
                if (i != values.size() - 1) {
                    ss << ", ";
                }
            }
        }
        ss << '\n';

        ss << "  Dimensions : ";
        auto& dimensions = datum.GetDimensions();
        if (dimensions.empty()) {
            ss << "N/A";
        } else {
            ss << '\n';
            for (auto& dimension : dimensions) {
                ss << "    - " << dimension.GetName() << "\t: " << dimension.GetValue() << '\n';
            }
        }
        ss << '\n';

        DLOGI("%s", ss.str().c_str());
    }

    VOID CloudwatchMetricsMonitoring::pushOutboundRtpStats(POutgoingRTPMetricsContext pOutboundRtpStats)
    {
        DLOGI("HEre");
        MetricDatum bytesDiscardedPercentageDatum, averageFramesRateDatum, nackRateDatum, retransmissionPercentDatum;

        bytesDiscardedPercentageDatum.SetMetricName("PercentageFrameDiscarded");
        bytesDiscardedPercentageDatum.SetValue(pOutboundRtpStats->framesPercentageDiscarded);
        bytesDiscardedPercentageDatum.SetUnit(Aws::CloudWatch::Model::StandardUnit::Percent);
        this->push(bytesDiscardedPercentageDatum);

        averageFramesRateDatum.SetMetricName("FramesPerSecond");
        averageFramesRateDatum.SetValue(pOutboundRtpStats->averageFramesSentPerSecond);
        averageFramesRateDatum.SetUnit(Aws::CloudWatch::Model::StandardUnit::Count_Second);
        this->push(averageFramesRateDatum);

        nackRateDatum.SetMetricName("NackPerSecond");
        nackRateDatum.SetValue(pOutboundRtpStats->nacksPerSecond);
        nackRateDatum.SetUnit(Aws::CloudWatch::Model::StandardUnit::Count_Second);
        this->push(nackRateDatum);

        retransmissionPercentDatum.SetMetricName("PercentageFramesRetransmitted");
        retransmissionPercentDatum.SetValue(pOutboundRtpStats->retxBytesPercentage);
        retransmissionPercentDatum.SetUnit(Aws::CloudWatch::Model::StandardUnit::Percent);
        this->push(retransmissionPercentDatum);
    }

    VOID CloudwatchMetricsMonitoring::pushInboundRtpStats(PIncomingRTPMetricsContext pIncomingRtpStats)
    {
        MetricDatum incomingBitrateDatum, incomingPacketRate, incomingFrameDropRateDatum;

        incomingBitrateDatum.SetMetricName("IncomingBitRate");
        incomingBitrateDatum.SetValue(pIncomingRtpStats->incomingBitRate);
        incomingBitrateDatum.SetUnit(Aws::CloudWatch::Model::StandardUnit::Kilobits_Second);
        this->push(incomingBitrateDatum);

        incomingPacketRate.SetMetricName("IncomingPacketsPerSecond");
        incomingPacketRate.SetValue(pIncomingRtpStats->packetReceiveRate);
        incomingPacketRate.SetUnit(Aws::CloudWatch::Model::StandardUnit::Count_Second);
        this->push(incomingPacketRate);

        incomingFrameDropRateDatum.SetMetricName("IncomingFramesDroppedPerSecond");
        incomingFrameDropRateDatum.SetValue(pIncomingRtpStats->framesDroppedPerSecond);
        incomingFrameDropRateDatum.SetUnit(Aws::CloudWatch::Model::StandardUnit::Count_Second);
        this->push(incomingFrameDropRateDatum);
    }

} // namespace Canary
