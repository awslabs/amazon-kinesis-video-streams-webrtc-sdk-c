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

typedef struct {
    PThreadpool pThreadpool;
    BOOL isInitialized;
    MUTEX threadpoolContextLock;
} ThreadPoolContext, *PThreadPoolContext;

PUBLIC_API STATUS createThreadPoolContext();
PUBLIC_API STATUS getThreadPoolContext(PThreadPoolContext);
PUBLIC_API STATUS threadpoolContextPush(startRoutine, PVOID);
PUBLIC_API STATUS destroyThreadPoolContext();

#ifdef __cplusplus
}
#endif
#endif /* __KINESIS_VIDEO_WEBRTC_CLIENT_THREADPOOLCONTEXT__ */
