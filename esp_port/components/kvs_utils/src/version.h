/*******************************************
Auth internal include file
*******************************************/
#ifndef __AWS_KVS_WEBRTC_VERSION_INCLUDE__
#define __AWS_KVS_WEBRTC_VERSION_INCLUDE__

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "error.h"

// Max string length for platform name
#define MAX_PLATFORM_NAME_STRING_LEN 128

// Max string length for the OS version
#define MAX_OS_VERSION_STRING_LEN 128

// Max string length for the compiler info
#define MAX_COMPILER_INFO_STRING_LEN 128

/**
 * IMPORTANT!!! This is the current version of the SDK which needs to be maintained
 */
#define AWS_SDK_KVS_PRODUCER_VERSION_STRING (PCHAR) "3.0.0"

/**
 * Default user agent string
 */
#define USER_AGENT_NAME (PCHAR) "AWS-SDK-KVS"

//
// Version functions
//
typedef UINT32 (*getPlatformName)(PCHAR, UINT32);
typedef UINT32 (*getOsVersion)(PCHAR, UINT32);
typedef UINT32 (*getCompilerInfo)(PCHAR, UINT32);

//
// Version information
//
extern getPlatformName globalGetPlatformName;
extern getOsVersion globalGetOsVersion;
extern getCompilerInfo globalGetCompilerInfo;

//
// Version macros
//
#define GET_PLATFORM_NAME globalGetPlatformName
#define GET_OS_VERSION    globalGetOsVersion
#define GET_COMPILER_INFO globalGetCompilerInfo

////////////////////////////////////////////////////
// Function definitions
////////////////////////////////////////////////////
/**
 * Creates a user agent string
 *
 * @param - PCHAR - IN - User agent name
 * @param - PCHAR - IN - Custom user agent string
 * @param - UINT32 - IN - Length of the string
 * @param - PCHAR - OUT - Combined user agent string
 *
 * @return - STATUS code of the execution
 */
UINT32 getUserAgentString(PCHAR, PCHAR, UINT32, PCHAR);

#ifdef __cplusplus
}
#endif
#endif /* __AWS_KVS_WEBRTC_VERSION_INCLUDE__ */
