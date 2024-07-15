#define LOG_CLASS "RTCPUtils"

#include "../Include_i.h"

STATUS convertRtcpErrorCode(RtcpResult_t rtcpResult)
{
    STATUS retStatus = STATUS_RTCP_UNKNOWN_ERROR;

    switch (rtcpResult) {
        case RTCP_RESULT_OK:
            retStatus = STATUS_SUCCESS;
            break;
        case RTCP_RESULT_BAD_PARAM:
            retStatus = STATUS_INVALID_ARG;
            break;
        case RTCP_RESULT_OUT_OF_MEMORY:
            retStatus = STATUS_NOT_ENOUGH_MEMORY;
            break;
        case RTCP_RESULT_WRONG_VERSION:
            retStatus = STATUS_RTCP_INPUT_PACKET_INVALID_VERSION;
            break;
        case RTCP_RESULT_INPUT_PACKET_TOO_SMALL:
            retStatus = STATUS_RTCP_INPUT_PACKET_TOO_SMALL;
            break;
        case RTCP_RESULT_INPUT_REMB_PACKET_INVALID:
            retStatus = STATUS_RTCP_INPUT_REMB_INVALID;
            break;
        default:
            break;
    }

    return retStatus;
}