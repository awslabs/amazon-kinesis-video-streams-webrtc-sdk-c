/*******************************************
Main internal include file
*******************************************/
#ifndef __KINESIS_VIDEO_WEBRTC_CLIENT_THREADPOOLCONTEXT__
#define __KINESIS_VIDEO_WEBRTC_CLIENT_THREADPOOLCONTEXT__

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

////////////////////////////////////////////////////
// Project include files
////////////////////////////////////////////////////

PUBLIC_API PThreadpool getThreadpoolInstance();
PUBLIC_API STATUS webRtcCreateThreadPool();
PUBLIC_API STATUS webRtcThreadPoolPush(startRoutine, PVOID);
PUBLIC_API STATUS webRtcDestroyThreadPool();

#ifdef __cplusplus
}
#endif
#endif /* __KINESIS_VIDEO_WEBRTC_CLIENT_THREADPOOLCONTEXT__ */
