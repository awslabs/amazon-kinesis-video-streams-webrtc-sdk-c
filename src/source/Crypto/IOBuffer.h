#ifndef __KINESIS_VIDEO_WEBRTC_CLIENT_CRYPTO_IOBUFFER__
#define __KINESIS_VIDEO_WEBRTC_CLIENT_CRYPTO_IOBUFFER__

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct __IOBuffer IOBuffer, *PIOBuffer;
struct __IOBuffer {
    UINT32 off, len, cap;
    PBYTE raw;
};

STATUS createIOBuffer(UINT32, PIOBuffer*);
STATUS freeIOBuffer(PIOBuffer*);

STATUS ioBufferReset(PIOBuffer);
STATUS ioBufferWrite(PIOBuffer, PBYTE, UINT32);
STATUS ioBufferRead(PIOBuffer, PBYTE, UINT32, PUINT32);

#ifdef __cplusplus
}
#endif
#endif //__KINESIS_VIDEO_WEBRTC_CLIENT_CRYPTO_IOBUFFER__
