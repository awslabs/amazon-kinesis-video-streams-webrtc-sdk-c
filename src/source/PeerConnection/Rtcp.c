#define LOG_CLASS "RtcRtcp"

#include "../Include_i.h"

STATUS onRtcpPacket(PKvsPeerConnection pKvsPeerConnection, PBYTE pBuff, INT32 pBuffLen)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pKvsPeerConnection != NULL && pBuff != NULL, STATUS_NULL_ARG);


CleanUp:

    return retStatus;
}
