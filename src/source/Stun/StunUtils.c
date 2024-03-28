#define LOG_CLASS "STUNUtils"
#include "../Include_i.h"
#include "kvsstun/stun_data_types.h"

STATUS convertStunErrorCode(StunResult_t stunResult)
{
    STATUS retStatus;

    switch (stunResult) {
        case STUN_RESULT_OK:
            retStatus = STATUS_SUCCESS;
            break;
        case STUN_RESULT_BAD_PARAM:
            retStatus = STATUS_INVALID_ARG;
            break;
        case STUN_RESULT_OUT_OF_MEMORY:
            retStatus = STATUS_NOT_ENOUGH_MEMORY;
            break;
        case STUN_RESULT_INVALID_MESSAGE_LENGTH:
            retStatus = STATUS_STUN_INVALID_MESSAGE_LENGTH;
            break;
        case STUN_RESULT_MAGIC_COOKIE_MISMATCH:
            retStatus = STATUS_STUN_MAGIC_COOKIE_MISMATCH;
            break;
        case STUN_RESULT_INVALID_ATTRIBUTE_LENGTH:
            retStatus = STATUS_STUN_INVALID_ATTRIBUTE_LENGTH;
            break;
        case STUN_RESULT_NO_MORE_ATTRIBUTE_FOUND:
            retStatus = STATUS_STUN_NO_MORE_ATTRIBUTE_FOUND;
            break;
        case STUN_RESULT_NO_ATTRIBUTE_FOUND:
            retStatus = STATUS_STUN_ATTRIBUTE_NOT_FOUND;
            break;
        default:
            retStatus = STATUS_STUN_UNKNOWN_ERROR;
            break;
    }

    return retStatus;
}
