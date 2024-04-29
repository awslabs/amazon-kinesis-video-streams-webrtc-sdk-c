#define LOG_CLASS "RTPUtils"

#include "../Include_i.h"

STATUS convertRtpErrorCode(RtpResult_t rtpResult)
{
    STATUS retStatus;

    switch (rtpResult) {
        case RTP_RESULT_OK:
            retStatus = STATUS_SUCCESS;
            break;
        case RTP_RESULT_BAD_PARAM:
            retStatus = STATUS_INVALID_ARG;
            break;
        case RTP_RESULT_OUT_OF_MEMORY:
            retStatus = STATUS_NOT_ENOUGH_MEMORY;
            break;
        case RTP_RESULT_WRONG_VERSION:
            retStatus = STATUS_RTP_INVALID_VERSION;
            break;
        case RTP_RESULT_MALFORMED_PACKET:
            retStatus = STATUS_RTP_INPUT_PACKET_TOO_SMALL;
            break;
        default:
            retStatus = STATUS_RTP_UNKNOWN_ERROR;
            break;
    }

    return retStatus;
}