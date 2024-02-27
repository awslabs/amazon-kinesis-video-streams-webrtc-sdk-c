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
#include "sampleSignaling.h"

typedef enum {
    MASTER,
    VIEWER
} CONFIG_TYPE;

typedef struct {
    SignalingCtx signalingCtx;
    CONFIG_TYPE configType;
    UINT32 logLevel;
} AppCtx, *PAppCtx;


STATUS initializeLibrary(PAppCtx);
STATUS deinitializeLibrary();
STATUS initializeAppCtx(PAppCtx, PCHAR, PCHAR);

#ifdef __cplusplus
}
#endif
#endif //KVS_SDK_ABSTRACTION_H
