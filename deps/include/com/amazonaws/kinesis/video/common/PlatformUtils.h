/**
 * Platform specific definitions for logging, assert, etc..
 */
#ifndef __PLATFORM_UTILS_DEFINES_H__
#define __PLATFORM_UTILS_DEFINES_H__

#ifdef __cplusplus
extern "C" {
#endif

#pragma once

// Tag for the logging
#ifndef LOG_CLASS
#define LOG_CLASS "platform-utils"
#endif

#ifdef DETECTED_GIT_HASH
#define GIT_HASH SDK_VERSION
#else
#define GIT_HASH EMPTY_STRING
#endif

// Log print function definition
typedef VOID (*logPrintFunc)(UINT32, const PCHAR, const PCHAR, ...);

//
// Default logger function
//
PUBLIC_API VOID defaultLogPrint(UINT32 level, const PCHAR tag, const PCHAR fmt, ...);

extern logPrintFunc globalCustomLogPrintFn;

#ifdef ANDROID_BUILD
// Compiling with NDK
#include <android/log.h>
#define __LOG(p1, p2, p3, ...)    __android_log_print(p1, p2, p3, ##__VA_ARGS__)
#define __ASSERT(p1, p2, p3, ...) __android_log_assert(p1, p2, p3, ##__VA_ARGS__) // This is unaffected by release vs debug build unlike C assert().
#else
// Compiling under non-NDK
#include <stddef.h>
#include <stdlib.h>
#include <assert.h>

#ifdef DEBUG_BUILD
#define __ASSERT(p1, p2, p3, ...) assert(p1)
#else
PUBLIC_API VOID customAssert(INT64 condition, const CHAR* fileName, INT64 lineNumber, const CHAR* functionName);
#define __ASSERT(p1, p2, p3, ...) customAssert((p1), __FILE__, __LINE__, __FUNCTION__)
#endif

#define __LOG globalCustomLogPrintFn
#endif // ANDROID_BUILD

#define LOG_LEVEL_VERBOSE 1
#define LOG_LEVEL_DEBUG   2
#define LOG_LEVEL_INFO    3
#define LOG_LEVEL_WARN    4
#define LOG_LEVEL_ERROR   5
#define LOG_LEVEL_FATAL   6
#define LOG_LEVEL_SILENT  7

#define DEFAULT_LOG_LEVEL LOG_LEVEL_WARN

// Adding this after LOG_LEVEL_SILENT to ensure we do not break backward compat
#define LOG_LEVEL_PROFILE 8

#define LOG_LEVEL_VERBOSE_STR (const PCHAR) "VERBOSE"
#define LOG_LEVEL_DEBUG_STR   (const PCHAR) "DEBUG"
#define LOG_LEVEL_INFO_STR    (const PCHAR) "INFO"
#define LOG_LEVEL_WARN_STR    (const PCHAR) "WARN"
#define LOG_LEVEL_ERROR_STR   (const PCHAR) "ERROR"
#define LOG_LEVEL_FATAL_STR   (const PCHAR) "FATAL"
#define LOG_LEVEL_SILENT_STR  (const PCHAR) "SILENT"
#define LOG_LEVEL_PROFILE_STR (const PCHAR) "PROFILE"

// LOG_LEVEL_VERBOSE_STR string length
#define MAX_LOG_LEVEL_STRLEN 7

// Extra logging macros
#ifndef DLOGE
#define DLOGE(fmt, ...) __LOG(LOG_LEVEL_ERROR, (const PCHAR) LOG_CLASS, (const PCHAR) "%s(): " fmt, __FUNCTION__, ##__VA_ARGS__)
#endif
#ifndef DLOGW
#define DLOGW(fmt, ...) __LOG(LOG_LEVEL_WARN, (const PCHAR) LOG_CLASS, (const PCHAR) "%s(): " fmt, __FUNCTION__, ##__VA_ARGS__)
#endif
#ifndef DLOGI
#define DLOGI(fmt, ...) __LOG(LOG_LEVEL_INFO, (const PCHAR) LOG_CLASS, (const PCHAR) "%s(): " fmt, __FUNCTION__, ##__VA_ARGS__)
#endif
#ifndef DLOGD
#define DLOGD(fmt, ...) __LOG(LOG_LEVEL_DEBUG, (const PCHAR) LOG_CLASS, (const PCHAR) "%s(): " fmt, __FUNCTION__, ##__VA_ARGS__)
#endif
#ifndef DLOGV
#define DLOGV(fmt, ...) __LOG(LOG_LEVEL_VERBOSE, (const PCHAR) LOG_CLASS, (const PCHAR) "%s(): " fmt, __FUNCTION__, ##__VA_ARGS__)
#endif
#ifndef DLOGP
#define DLOGP(fmt, ...) __LOG(LOG_LEVEL_PROFILE, (const PCHAR) LOG_CLASS, (const PCHAR) "%s(): " fmt, __FUNCTION__, ##__VA_ARGS__)
#endif

#ifndef ENTER
#define ENTER() DLOGV("Enter")
#endif
#ifndef LEAVE
#define LEAVE() DLOGV("Leave")
#endif
#ifndef ENTERS
#define ENTERS() DLOGS("Enter")
#endif
#ifndef LEAVES
#define LEAVES() DLOGS("Leave")
#endif

// Optionally log very verbose data (>200 msgs/second) about the streaming process
#ifndef DLOGS
#ifdef LOG_STREAMING
#define DLOGS DLOGV
#else
#define DLOGS(...)                                                                                                                                   \
    do {                                                                                                                                             \
    } while (0)
#endif
#endif

// Assertions
#ifndef CONDITION
#ifdef __GNUC__
#define CONDITION(cond) (__builtin_expect((cond) != 0, 0))
#else
#define CONDITION(cond) ((cond) == TRUE)
#endif
#endif
#ifndef LOG_ALWAYS_FATAL_IF
#define LOG_ALWAYS_FATAL_IF(cond, ...) ((CONDITION(cond)) ? ((void) __ASSERT(FALSE, (const PCHAR) LOG_CLASS, ##__VA_ARGS__)) : (void) 0)
#endif

#ifndef LOG_ALWAYS_FATAL
#define LOG_ALWAYS_FATAL(...) (((void) __ASSERT(FALSE, (const PCHAR) LOG_CLASS, ##__VA_ARGS__)))
#endif

#ifndef SANITIZED_FILE
#define SANITIZED_FILE (STRRCHR(__FILE__, '/') ? STRRCHR(__FILE__, '/') + 1 : __FILE__)
#endif
#ifndef CRASH
#define CRASH(fmt, ...) LOG_ALWAYS_FATAL("%s::%s: " fmt, (const PCHAR) LOG_CLASS, __FUNCTION__, ##__VA_ARGS__)
#endif
#ifndef CHECK
#define CHECK(x) LOG_ALWAYS_FATAL_IF(!(x), "%s::%s: ASSERTION FAILED at %s:%d: " #x, (const PCHAR) LOG_CLASS, __FUNCTION__, SANITIZED_FILE, __LINE__)
#endif
#ifndef CHECK_EXT
#define CHECK_EXT(x, fmt, ...)                                                                                                                       \
    LOG_ALWAYS_FATAL_IF(!(x), "%s::%s: ASSERTION FAILED at %s:%d: " fmt, (const PCHAR) LOG_CLASS, __FUNCTION__, SANITIZED_FILE, __LINE__,            \
                        ##__VA_ARGS__)
#endif

#ifndef LOG_GIT_HASH
#define LOG_GIT_HASH() DLOGI("SDK version: %s", GIT_HASH)
#endif

#ifdef __cplusplus
}
#endif

#endif // __PLATFORM_UTILS_DEFINES_H__
