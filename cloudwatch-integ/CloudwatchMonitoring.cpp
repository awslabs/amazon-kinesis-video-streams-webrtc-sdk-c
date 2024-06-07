#include "Include.h"
#include "CloudwatchMonitoring.h"

namespace CppInteg {

CloudwatchMonitoring::CloudwatchMonitoring(ClientConfiguration* pClientConfig) : client(*pClientConfig)
{
}

STATUS CloudwatchMonitoring::init(PCHAR channelName, PCHAR region, BOOL isMaster, BOOL isStorage)
{
    STATUS retStatus = STATUS_SUCCESS;
    this->isStorage = isStorage;
    isStorage ? this->channelDimension.SetName(INDIVIDUAL_STORAGE_CW_DIMENSION) : this->channelDimension.SetName(INDIVIDUAL_CW_DIMENSION);
    this->channelDimension.SetValue(channelName);

    isStorage ? this->labelDimension.SetName(AGGREGATE_STORAGE_CW_DIMENSION) : this->labelDimension.SetName(AGGREGATE_CW_DIMENSION);
    this->labelDimension.SetValue(SCENARIO_LABEL);

    return retStatus;
}

VOID CloudwatchMonitoring::deinit()
{
    // need to wait all metrics to be flushed out, otherwise we'll get a segfault.
    // https://docs.aws.amazon.com/sdk-for-cpp/v1/developer-guide/basic-use.html
    // TODO: maybe add a timeout? But, this might cause a segfault if it hits a timeout.
    while (this->pendingMetrics.load() > 0) {
        THREAD_SLEEP(HUNDREDS_OF_NANOS_IN_A_MILLISECOND * 500);
    }
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

VOID CloudwatchMonitoring::push(const MetricDatum& datum)
{
    Aws::CloudWatch::Model::PutMetricDataRequest cwRequest;
    MetricDatum single = datum;
    MetricDatum aggregated = datum;

    single.AddDimensions(this->channelDimension);
    single.AddDimensions(this->labelDimension);
    aggregated.AddDimensions(this->labelDimension);

    cwRequest.SetNamespace(DEFAULT_CLOUDWATCH_NAMESPACE);
    cwRequest.AddMetricData(single);
    cwRequest.AddMetricData(aggregated);

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

    DLOGD("%s", ss.str().c_str());
}

VOID CloudwatchMonitoring::pushExitStatus(STATUS retStatus)
{
    MetricDatum datum;
    Dimension statusDimension;
    CHAR status[MAX_STATUS_CODE_LENGTH];

    statusDimension.SetName("Code");
    SPRINTF(status, "0x%08x", retStatus);
    statusDimension.SetValue(status);

    datum.SetMetricName("ExitStatus");
    datum.SetValue(1.0);
    datum.SetUnit(Aws::CloudWatch::Model::StandardUnit::Count);

    datum.AddDimensions(statusDimension);

    this->push(datum);
}

VOID CloudwatchMonitoring::pushSignalingRoundtripStatus(STATUS retStatus)
{
    MetricDatum datum;
    Dimension statusDimension;
    CHAR status[MAX_STATUS_CODE_LENGTH];

    statusDimension.SetName("Code");
    SPRINTF(status, "0x%08x", retStatus);
    statusDimension.SetValue(status);

    datum.SetMetricName("SignalingRoundtripStatus");
    datum.SetValue(1.0);
    datum.SetUnit(Aws::CloudWatch::Model::StandardUnit::Count);

    datum.AddDimensions(statusDimension);

    this->push(datum);
}

VOID CloudwatchMonitoring::pushSignalingRoundtripLatency(UINT64 delay, Aws::CloudWatch::Model::StandardUnit unit)
{
    MetricDatum datum;

    datum.SetMetricName("SignalingRoundtripLatency");
    datum.SetValue(delay);
    datum.SetUnit(unit);

    this->push(datum);
}

VOID CloudwatchMonitoring::pushSignalingConnectionDuration(UINT64 duration, Aws::CloudWatch::Model::StandardUnit unit)
{
    MetricDatum datum;

    datum.SetMetricName("SignalingConnectionDuration");
    datum.SetValue(duration);
    datum.SetUnit(unit);

    this->push(datum);
}

VOID CloudwatchMonitoring::pushTimeToFirstFrame(UINT64 timeToFirstFrame, Aws::CloudWatch::Model::StandardUnit unit)
{
    MetricDatum datum;

    datum.SetMetricName("TimeToFirstFrame");
    datum.SetValue(timeToFirstFrame);
    datum.SetUnit(unit);

    this->push(datum);
}



VOID CloudwatchMonitoring::pushStorageDisconnectToFrameSentTime(UINT64 storageDisconnectToFrameSentTime, Aws::CloudWatch::Model::StandardUnit unit)
{
    MetricDatum datum;

    datum.SetMetricName("StorageDisconnectToFrameSentTime");
    datum.SetValue(storageDisconnectToFrameSentTime);
    datum.SetUnit(unit);

    this->push(datum);
}

VOID CloudwatchMonitoring::pushJoinSessionTime(UINT64 joinSessionTime, Aws::CloudWatch::Model::StandardUnit unit)
{
    MetricDatum datum;

    datum.SetMetricName("JoinSessionTime");
    datum.SetValue(joinSessionTime);
    datum.SetUnit(unit);

    this->push(datum);
}

VOID CloudwatchMonitoring::pushSignalingInitDelay(UINT64 delay, Aws::CloudWatch::Model::StandardUnit unit)
{
    MetricDatum datum;

    datum.SetMetricName("SignalingInitDelay");
    datum.SetValue(delay);
    datum.SetUnit(unit);

    this->push(datum);
}

VOID CloudwatchMonitoring::pushOutboundRtpStats(POutgoingRTPStatsCtx pOutboundRtpStats)
{
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

VOID CloudwatchMonitoring::pushPeerConnectionMetrics(PPeerConnectionMetrics pPeerConnectionMetrics)
{
    MetricDatum pcCreationDatum, dtlsSetupDatum, iceHolePunchingDatum;

    pcCreationDatum.SetMetricName("PcCreationTime");
    pcCreationDatum.SetValue(pPeerConnectionMetrics->peerConnectionStats.peerConnectionCreationTime);
    pcCreationDatum.SetUnit(Aws::CloudWatch::Model::StandardUnit::Milliseconds);
    this->push(pcCreationDatum);

    dtlsSetupDatum.SetMetricName("DtlsSetupTime");
    dtlsSetupDatum.SetValue(pPeerConnectionMetrics->peerConnectionStats.dtlsSessionSetupTime);
    dtlsSetupDatum.SetUnit(Aws::CloudWatch::Model::StandardUnit::Milliseconds);
    this->push(dtlsSetupDatum);

    iceHolePunchingDatum.SetMetricName("ICEHolePunchingDelay");
    iceHolePunchingDatum.SetValue(pPeerConnectionMetrics->peerConnectionStats.iceHolePunchingTime);
    iceHolePunchingDatum.SetUnit(Aws::CloudWatch::Model::StandardUnit::Milliseconds);
    this->push(iceHolePunchingDatum);
}

VOID CloudwatchMonitoring::pushKvsIceAgentMetrics(PKvsIceAgentMetrics pKvsIceAgentMetrics)
{
    MetricDatum localCandidateGatheringDatum, hostCandidateSetupDatum, srflxCandidateSetUpDatum,
                iceAgentSetupDatum, relayCandidateSetUpDatum, iceServerParseDatum,
                iceCandidatePairNominationDatum, iceCandidateGatheringDatum;

    localCandidateGatheringDatum.SetMetricName("LocalCandidateGatheringTime");
    localCandidateGatheringDatum.SetValue(pKvsIceAgentMetrics->kvsIceAgentStats.localCandidateGatheringTime);
    localCandidateGatheringDatum.SetUnit(Aws::CloudWatch::Model::StandardUnit::Milliseconds);
    this->push(localCandidateGatheringDatum);

    hostCandidateSetupDatum.SetMetricName("HostCandidateSetUpTime");
    hostCandidateSetupDatum.SetValue(pKvsIceAgentMetrics->kvsIceAgentStats.hostCandidateSetUpTime);
    hostCandidateSetupDatum.SetUnit(Aws::CloudWatch::Model::StandardUnit::Milliseconds);
    this->push(hostCandidateSetupDatum);

    srflxCandidateSetUpDatum.SetMetricName("SrflxCandidateSetUpTime");
    srflxCandidateSetUpDatum.SetValue(pKvsIceAgentMetrics->kvsIceAgentStats.srflxCandidateSetUpTime);
    srflxCandidateSetUpDatum.SetUnit(Aws::CloudWatch::Model::StandardUnit::Milliseconds);
    this->push(srflxCandidateSetUpDatum);

    relayCandidateSetUpDatum.SetMetricName("RelayCandidateSetUpTime");
    relayCandidateSetUpDatum.SetValue(pKvsIceAgentMetrics->kvsIceAgentStats.relayCandidateSetUpTime);
    relayCandidateSetUpDatum.SetUnit(Aws::CloudWatch::Model::StandardUnit::Milliseconds);
    this->push(relayCandidateSetUpDatum);

    iceServerParseDatum.SetMetricName("IceServerResolutionTime");
    iceServerParseDatum.SetValue(pKvsIceAgentMetrics->kvsIceAgentStats.iceServerParsingTime);
    iceServerParseDatum.SetUnit(Aws::CloudWatch::Model::StandardUnit::Milliseconds);
    this->push(iceServerParseDatum);

    iceCandidatePairNominationDatum.SetMetricName("IceCandidatePairNominationTime");
    iceCandidatePairNominationDatum.SetValue(pKvsIceAgentMetrics->kvsIceAgentStats.iceCandidatePairNominationTime);
    iceCandidatePairNominationDatum.SetUnit(Aws::CloudWatch::Model::StandardUnit::Milliseconds);
    this->push(iceCandidatePairNominationDatum);

    iceCandidateGatheringDatum.SetMetricName("IcecandidateGatheringTime");
    iceCandidateGatheringDatum.SetValue(pKvsIceAgentMetrics->kvsIceAgentStats.candidateGatheringTime);
    iceCandidateGatheringDatum.SetUnit(Aws::CloudWatch::Model::StandardUnit::Milliseconds);
    this->push(iceCandidateGatheringDatum);

    iceAgentSetupDatum.SetMetricName("IceAgentSetUpTime");
    iceAgentSetupDatum.SetValue(pKvsIceAgentMetrics->kvsIceAgentStats.iceAgentSetUpTime);
    iceAgentSetupDatum.SetUnit(Aws::CloudWatch::Model::StandardUnit::Milliseconds);
    this->push(iceAgentSetupDatum);
}

VOID CloudwatchMonitoring::pushSignalingClientMetrics(PSignalingClientMetrics pSignalingClientMetrics)
{
    MetricDatum offerToAnswerDatum, getTokenDatum, describeDatum, createDatum, endpointDatum,
                iceConfigDatum, connectDatum, createClientDatum, fetchDatum, connectClientDatum, joinSessionToOfferDatum;

    UINT64 joinSessionToOffer, joinSessionCallTime;

    offerToAnswerDatum.SetMetricName("OfferToAnswerTime");
    offerToAnswerDatum.SetValue(pSignalingClientMetrics->signalingClientStats.offerToAnswerTime);
    offerToAnswerDatum.SetUnit(Aws::CloudWatch::Model::StandardUnit::Milliseconds);
    this->push(offerToAnswerDatum);

    getTokenDatum.SetMetricName("GetTokenTime");
    getTokenDatum.SetValue(pSignalingClientMetrics->signalingClientStats.getTokenCallTime);
    getTokenDatum.SetUnit(Aws::CloudWatch::Model::StandardUnit::Milliseconds);
    this->push(getTokenDatum);

    describeDatum.SetMetricName("DescribeCallTime");
    describeDatum.SetValue(pSignalingClientMetrics->signalingClientStats.describeCallTime);
    describeDatum.SetUnit(Aws::CloudWatch::Model::StandardUnit::Milliseconds);
    this->push(describeDatum);

    createDatum.SetMetricName("CreateCallTime");
    createDatum.SetValue(pSignalingClientMetrics->signalingClientStats.createCallTime);
    createDatum.SetUnit(Aws::CloudWatch::Model::StandardUnit::Milliseconds);
    this->push(createDatum);

    endpointDatum.SetMetricName("GetEndpointCallTime");
    endpointDatum.SetValue(pSignalingClientMetrics->signalingClientStats.getEndpointCallTime);
    endpointDatum.SetUnit(Aws::CloudWatch::Model::StandardUnit::Milliseconds);
    this->push(endpointDatum);

    iceConfigDatum.SetMetricName("GetIceConfigCallTime");
    iceConfigDatum.SetValue(pSignalingClientMetrics->signalingClientStats.getIceConfigCallTime);
    iceConfigDatum.SetUnit(Aws::CloudWatch::Model::StandardUnit::Milliseconds);
    this->push(iceConfigDatum);

    connectDatum.SetMetricName("ConnectCallTime");
    connectDatum.SetValue(pSignalingClientMetrics->signalingClientStats.connectCallTime);
    connectDatum.SetUnit(Aws::CloudWatch::Model::StandardUnit::Milliseconds);
    this->push(connectDatum);

    createClientDatum.SetMetricName("CreateClientTotalTime");
    createClientDatum.SetValue(pSignalingClientMetrics->signalingClientStats.createClientTime);
    createClientDatum.SetUnit(Aws::CloudWatch::Model::StandardUnit::Milliseconds);
    this->push(createClientDatum);

    fetchDatum.SetMetricName("FetchClientTotalTime");
    fetchDatum.SetValue(pSignalingClientMetrics->signalingClientStats.fetchClientTime);
    fetchDatum.SetUnit(Aws::CloudWatch::Model::StandardUnit::Milliseconds);
    this->push(fetchDatum);

    connectClientDatum.SetMetricName("ConnectClientTotalTime");
    connectClientDatum.SetValue(pSignalingClientMetrics->signalingClientStats.connectClientTime);
    connectClientDatum.SetUnit(Aws::CloudWatch::Model::StandardUnit::Milliseconds);
    this->push(connectClientDatum);

    if (this->isStorage) {
        joinSessionToOffer = pSignalingClientMetrics->signalingClientStats.joinSessionToOfferRecvTime;
        if (joinSessionToOffer > 0) {
            joinSessionToOfferDatum.SetMetricName("JoinSessionToOfferReceived");
            joinSessionToOfferDatum.SetValue(joinSessionToOffer / HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
            joinSessionToOfferDatum.SetUnit(Aws::CloudWatch::Model::StandardUnit::Milliseconds);
            this->push(joinSessionToOfferDatum);
        }

        joinSessionCallTime = (pSignalingClientMetrics->signalingClientStats.joinSessionCallTime);
        if (joinSessionToOffer > 0) {
            this->pushJoinSessionTime(joinSessionCallTime, Aws::CloudWatch::Model::StandardUnit::Milliseconds);
        }
    }
}

VOID CloudwatchMonitoring::pushInboundRtpStats(PIncomingRTPStatsCtx pIncomingRtpStats)
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

VOID CloudwatchMonitoring::pushRetryCount(UINT32 retryCount)
{
    MetricDatum currentRetryCountDatum;

    currentRetryCountDatum.SetMetricName("APICallRetryCount");
    currentRetryCountDatum.SetUnit(Aws::CloudWatch::Model::StandardUnit::Count);
    currentRetryCountDatum.SetValue(retryCount);
    this->push(currentRetryCountDatum);
}

VOID CloudwatchMonitoring::pushEndToEndMetrics(PEndToEndMetricsCtx pEndToEndMetricsCtx)
{
    MetricDatum endToEndLatencyDatum, sizeMatchDatum;
    DOUBLE latency = pEndToEndMetricsCtx->frameLatencyAvg / (DOUBLE) HUNDREDS_OF_NANOS_IN_A_MILLISECOND;

    // TODO: due to https://github.com/aws-samples/amazon-kinesis-video-streams-demos/issues/96,
    //       it's not clear why the emitted metric shows -nan. Since -nan is a string that's outputted
    //       from Datum's value stringify implementation, we should try to get the original value by
    //       printing it ourself.
    //
    //       If the issues doesn't exist anymore, please remove this as this is intended for debugging only.
    //       The generic metric logging should be sufficient.
    DLOGI("Current end-to-end frame latency: %4.2lf", latency);
    endToEndLatencyDatum.SetMetricName("EndToEndFrameLatency");
    endToEndLatencyDatum.SetUnit(Aws::CloudWatch::Model::StandardUnit::Milliseconds);
    endToEndLatencyDatum.SetValue(latency);
    this->push(endToEndLatencyDatum);

    sizeMatchDatum.SetMetricName("FrameSizeMatch");
    sizeMatchDatum.SetUnit(Aws::CloudWatch::Model::StandardUnit::Count);
    sizeMatchDatum.SetValue(pEndToEndMetricsCtx->sizeMatchAvg);
    DLOGI("Size match? %d", pEndToEndMetricsCtx->sizeMatchAvg);
    this->push(sizeMatchDatum);
}

} // namespace Canary
