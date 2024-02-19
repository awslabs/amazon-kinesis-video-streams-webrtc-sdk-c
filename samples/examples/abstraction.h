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
    PCHAR channelName;
} AppCtx, *PAppCtx;


STATUS initializeLibrary();
STATUS deinitializeLibrary();

#ifdef __cplusplus
}
#endif
#endif //KVS_SDK_ABSTRACTION_H
