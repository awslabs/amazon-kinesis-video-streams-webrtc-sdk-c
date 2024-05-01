#include "Samples.h"

STATUS initMetricsModule(UINT64 metrics) {
    if(metrics & ICE_CANDIDATE_PAIR_METRICS) {

    }
    if(metrics & ICE_SERVER_METRICS) {

    }
    if(metrics & DATA_CHANNEL_METRICS) {

    }
    if(metrics & INBOUND_RTP_METRICS) {

    }
    if(metrics & ICE_LOCAL_CANDIDATE_METRICS) {

    }
    if(metrics & OUTBOUND_RTP_METRICS) {

    }
    if(metrics & ICE_REMOTE_CANDIDATE_METRICS) {

    }
    if(metrics & REMOTE_INBOUND_RTP_METRICS) {

    }
    if(metrics & REMOTE_OUTBOUND_RTP_METRICS) {

    }
    if(metrics & TRANSPORT_METRICS) {

    }
    if(metrics & ALL_METRICS) {

    }
    return STATUS_SUCCESS;
}