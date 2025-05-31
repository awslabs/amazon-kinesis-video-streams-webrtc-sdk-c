#define LOG_CLASS "SDPUtils"
#include "../Include_i.h"
#include "kvssdp/sdp_data_types.h"

STATUS convertSdpErrorCode(SdpResult_t sdpResult)
{
    STATUS retStatus = STATUS_SUCCESS;

    switch (sdpResult) {
        case SDP_RESULT_OK:
        case SDP_RESULT_MESSAGE_END:
            /* SDP_RESULT_MESSAGE_END means content is completely parsed, no error happens. */
            retStatus = STATUS_SUCCESS;
            break;
        case SDP_RESULT_BAD_PARAM:
            retStatus = STATUS_INVALID_ARG;
            break;
        case SDP_RESULT_MESSAGE_MALFORMED:
            retStatus = STATUS_SDP_MESSAGE_MALFORMED;
            break;
        case SDP_RESULT_MESSAGE_MALFORMED_NOT_ENOUGH_INFO:
            retStatus = STATUS_SDP_MESSAGE_MALFORMED_NOT_ENOUGH_INFO;
            break;
        case SDP_RESULT_MESSAGE_MALFORMED_EQUAL_NOT_FOUND:
            retStatus = STATUS_SDP_MESSAGE_MALFORMED_EQUAL_NOT_FOUND;
            break;
        case SDP_RESULT_MESSAGE_MALFORMED_NEWLINE_NOT_FOUND:
            retStatus = STATUS_SDP_MESSAGE_MALFORMED_NEWLINE_NOT_FOUND;
            break;
        case SDP_RESULT_MESSAGE_MALFORMED_NO_VALUE:
            retStatus = STATUS_SDP_MESSAGE_MALFORMED_NO_VALUE;
            break;
        case SDP_RESULT_MESSAGE_MALFORMED_NO_SESSION_ID:
            retStatus = STATUS_SDP_MESSAGE_MALFORMED_NO_SESSION_ID;
            break;
        case SDP_RESULT_MESSAGE_MALFORMED_NO_SESSION_VERSION:
            retStatus = STATUS_SDP_MESSAGE_MALFORMED_NO_SESSION_VERSION;
            break;
        case SDP_RESULT_MESSAGE_MALFORMED_INVALID_NETWORK_TYPE:
            retStatus = STATUS_SDP_MESSAGE_MALFORMED_INVALID_NETWORK_TYPE;
            break;
        case SDP_RESULT_MESSAGE_MALFORMED_INVALID_ADDRESS_TYPE:
            retStatus = STATUS_SDP_MESSAGE_MALFORMED_INVALID_ADDRESS_TYPE;
            break;
        case SDP_RESULT_MESSAGE_MALFORMED_REDUNDANT_INFO:
            retStatus = STATUS_SDP_MESSAGE_MALFORMED_REDUNDANT_INFO;
            break;
        case SDP_RESULT_MESSAGE_MALFORMED_INVALID_BANDWIDTH:
            retStatus = STATUS_SDP_MESSAGE_MALFORMED_INVALID_BANDWIDTH;
            break;
        case SDP_RESULT_MESSAGE_MALFORMED_INVALID_START_TIME:
            retStatus = STATUS_SDP_MESSAGE_MALFORMED_INVALID_START_TIME;
            break;
        case SDP_RESULT_MESSAGE_MALFORMED_INVALID_STOP_TIME:
            retStatus = STATUS_SDP_MESSAGE_MALFORMED_INVALID_STOP_TIME;
            break;
        case SDP_RESULT_MESSAGE_MALFORMED_INVALID_PORT:
            retStatus = STATUS_SDP_MESSAGE_MALFORMED_INVALID_PORT;
            break;
        case SDP_RESULT_MESSAGE_MALFORMED_INVALID_PORTNUM:
            retStatus = STATUS_SDP_MESSAGE_MALFORMED_INVALID_PORTNUM;
            break;
        case SDP_RESULT_OUT_OF_MEMORY:
            retStatus = STATUS_BUFFER_TOO_SMALL;
            break;
        case SDP_RESULT_SNPRINTF_ERROR:
            retStatus = STATUS_SDP_SNPRINTF_ERROR;
            break;
        default:
            retStatus = STATUS_SDP_UNKNOWN_ERROR;
            break;
    }

    return retStatus;
}
