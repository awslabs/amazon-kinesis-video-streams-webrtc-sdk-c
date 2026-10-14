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
    BOOL shuttingDown;
    MUTEX threadpoolContextLock;
    volatile SIZE_T outstandingTaskCount;
} ThreadPoolContext, *PThreadPoolContext;

PThreadPoolContext getReceiveThreadContextInstance();
PUBLIC_API STATUS createThreadPoolContext();
PUBLIC_API STATUS getThreadPoolContext(PThreadPoolContext);
PUBLIC_API STATUS threadpoolContextPush(startRoutine, PVOID);
PUBLIC_API STATUS destroyThreadPoolContext();
PUBLIC_API STATUS createReceiveThreadPoolContext();
PUBLIC_API STATUS receiveThreadpoolContextPush(startRoutine, PVOID);
PUBLIC_API STATUS destroyReceiveThreadPoolContext();

#ifdef __cplusplus
}
#endif
#endif /* __KINESIS_VIDEO_WEBRTC_CLIENT_THREADPOOLCONTEXT__ */
