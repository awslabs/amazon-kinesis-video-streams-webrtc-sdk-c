//
// Created by Sampath Kumar, Divya on 2/16/24.
//

#ifndef KVS_SDK_ABSTRACTION_H
#define KVS_SDK_ABSTRACTION_H

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <com/amazonaws/kinesis/video/webrtcclient/Include.h>

typedef struct {
    CHAR channelName[256];
} AppCtx, *PAppCtx;


STATUS initializeLibrary(PAppCtx);
STATUS deinitializeLibrary();

#ifdef __cplusplus
}
#endif
#endif //KVS_SDK_ABSTRACTION_H
