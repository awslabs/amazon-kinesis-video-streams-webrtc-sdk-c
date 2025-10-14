/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <inttypes.h>
#include <ctype.h>
#include <time.h>

/**
 * @brief Controls whether to use dynamic allocation for URLs and payloads
 * If CONFIG_PREFER_DYNAMIC_ALLOCS is enabled, enable dynamic URL allocation
 */
#if CONFIG_PREFER_DYNAMIC_ALLOCS
#define DYNAMIC_SIGNALING_PAYLOAD 1
#define USE_DYNAMIC_URL 1
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include "common.h"
#include "error.h"
#include "platform_esp32.h"
#include "platform_utils.h"
#include "version.h"

//
// Infinite time
//
#define INFINITE_TIME_VALUE MAX_UINT64

//
// Some standard definitions/macros
//
#ifndef SIZEOF
#define SIZEOF(x) (sizeof(x))
#endif

//
// Check for 32/64 bit
//
#ifndef CHECK_64_BIT
#define CHECK_64_BIT (SIZEOF(SIZE_T) == 8)
#endif

#ifndef UNUSED_PARAM
#define UNUSED_PARAM(expr)                                                                                                                           \
    do {                                                                                                                                             \
        (void) (expr);                                                                                                                               \
    } while (0)
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(array) (sizeof(array) / sizeof *(array))
#endif

#ifndef SAFE_MEMFREE
#define SAFE_MEMFREE(p)                                                                                                                              \
    do {                                                                                                                                             \
        if (p) {                                                                                                                                     \
            MEMFREE(p);                                                                                                                              \
            (p) = NULL;                                                                                                                              \
        }                                                                                                                                            \
    } while (0)
#endif

#ifndef SAFE_DELETE
#define SAFE_DELETE(p)                                                                                                                               \
    do {                                                                                                                                             \
        if (p) {                                                                                                                                     \
            delete (p);                                                                                                                              \
            (p) = NULL;                                                                                                                              \
        }                                                                                                                                            \
    } while (0)
#endif

#ifndef SAFE_DELETE_ARRAY
#define SAFE_DELETE_ARRAY(p)                                                                                                                         \
    do {                                                                                                                                             \
        if (p) {                                                                                                                                     \
            delete[] (p);                                                                                                                            \
            (p) = NULL;                                                                                                                              \
        }                                                                                                                                            \
    } while (0)
#endif

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

#ifndef ABS
#define ABS(a) ((a) > (0) ? (a) : (-a))
#endif

#ifndef IS_ALIGNED_TO
#define IS_ALIGNED_TO(m, n) ((m) % (n) == 0)
#endif

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>

#if !(defined _WIN32 || defined _WIN64)
#include <unistd.h>
#include <dirent.h>
#include <sys/time.h>
// #include <sys/utsname.h>
#endif

#if !defined(_MSC_VER) && !defined(__MINGW64__) && !defined(__MINGW32__) && !defined(__MACH__)
// NOTE!!! For some reason memalign is not included for Linux builds in stdlib.h
#include <malloc.h>
#endif

#if defined __WINDOWS_BUILD__

// Include the posix IO
#include <fcntl.h>
#include <io.h>

#include "dlfcn_win_stub.h"

// Definition of the helper stats check macros for Windows
#ifndef S_ISLNK
#define S_ISLNK(mode) FALSE
#endif

#ifndef S_ISDIR
#define S_ISDIR(mode) (((mode) & S_IFMT) == S_IFDIR)
#endif

#ifndef S_ISREG
#define S_ISREG(mode) (((mode) & S_IFMT) == S_IFREG)
#endif

// Definition of the mkdir for Windows with 1 param
#define GLOBAL_MKDIR(p1, p2) _mkdir(p1)

// Definition of the case insensitive string compare
#define GLOBAL_STRCMPI  _strcmpi
#define GLOBAL_STRNCMPI _strnicmp

#if defined(__MINGW64__) || defined(__MINGW32__)
#define GLOBAL_RMDIR rmdir
#define GLOBAL_STAT  stat

// Typedef stat structure
typedef struct stat STAT_STRUCT;
#else
// Definition of posix APIs for Windows
#define GLOBAL_RMDIR _rmdir
#define GLOBAL_STAT  _stat

// Typedef stat structure
typedef struct _stat STAT_STRUCT;
#endif

// Definition of the static initializers
#define GLOBAL_MUTEX_INIT           MUTEX_CREATE(FALSE)
#define GLOBAL_MUTEX_INIT_RECURSIVE MUTEX_CREATE(TRUE)
#define GLOBAL_CVAR_INIT            CVAR_CREATE()

#else

// Definition of the mkdir for non-Windows platforms with 2 params
#define GLOBAL_MKDIR(p1, p2) mkdir((p1), (p2))

// Definition of the case insensitive string compare
#define GLOBAL_STRCMPI       strcasecmp
#define GLOBAL_STRNCMPI      strncasecmp

// Definition of posix API for non-Windows platforms
#define GLOBAL_RMDIR         rmdir
#define GLOBAL_STAT          stat

// Typedef stat structure
typedef struct stat STAT_STRUCT;

// NOTE!!! Some of the libraries don't have a definition of PTHREAD_RECURSIVE_MUTEX_INITIALIZER
#ifndef PTHREAD_RECURSIVE_MUTEX_INITIALIZER

#ifndef PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP
#define PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP                                                                                                       \
    {                                                                                                                                                \
        {                                                                                                                                            \
            PTHREAD_MUTEX_RECURSIVE                                                                                                                  \
        }                                                                                                                                            \
    }
#endif

#define GLOBAL_MUTEX_INIT_RECURSIVE PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP
#else
#define GLOBAL_MUTEX_INIT_RECURSIVE PTHREAD_RECURSIVE_MUTEX_INITIALIZER
#endif

#define GLOBAL_MUTEX_INIT PTHREAD_MUTEX_INITIALIZER
#define GLOBAL_CVAR_INIT  PTHREAD_COND_INITIALIZER

#endif

//
// Allocator function definitions
//
typedef PVOID (*memAlloc)(SIZE_T size);
typedef PVOID (*memAlignAlloc)(SIZE_T size, SIZE_T alignment);
typedef PVOID (*memCalloc)(SIZE_T num, SIZE_T size);
typedef PVOID (*memRealloc)(PVOID ptr, SIZE_T size);
typedef VOID (*memFree)(PVOID ptr);

typedef BOOL (*memChk)(PVOID ptr, BYTE val, SIZE_T size);

//
// Default allocator functions
//
PVOID defaultMemAlloc(SIZE_T size);
PVOID defaultMemAlignAlloc(SIZE_T size, SIZE_T alignment);
PVOID defaultMemCalloc(SIZE_T num, SIZE_T size);
PVOID defaultMemRealloc(PVOID ptr, SIZE_T size);
VOID defaultMemFree(VOID* ptr);

//
// Global allocator function pointers
//
extern memAlloc globalMemAlloc;
extern memAlignAlloc globalMemAlignAlloc;
extern memCalloc globalMemCalloc;
extern memRealloc globalMemRealloc;
extern memFree globalMemFree;

extern memChk globalMemChk;

//
// Thread library function definitions
//
typedef TID (*getTId)();
typedef STATUS (*getTName)(TID, PCHAR, UINT32);

//
// Thread related functionality
//
extern getTId globalGetThreadId;
extern getTName globalGetThreadName;

//
// Time library function definitions
//
typedef UINT64 (*getTime)();

typedef struct tm* (*getTmTime)(const time_t*);

//
// Default time library functions
//
#define TIME_DIFF_UNIX_WINDOWS_TIME 116444736000000000ULL

PUBLIC_API UINT64 defaultGetTime();

//
// The C library function gmtime is not threadsafe, but we need a thread
// safe impl.  This provides that by wrapping the gmtime call around
// a global mutex specific for gmtime calls.  All instances of GMTIME
// can be safely replaced with the new GMTIME_THREAD_SAFE.
// On Windows gmtime is threadsafe so no impact there.
//
PUBLIC_API struct tm* defaultGetThreadSafeTmTime(const time_t*);

//
// Thread related functionality
//
extern getTime globalGetTime;
extern getTime globalGetRealTime;
extern getTmTime globalGetThreadSafeTmTime;

//
// Thread library function definitions
//
typedef PVOID (*startRoutine)(PVOID);
typedef MUTEX (*createMutex)(BOOL);
typedef VOID (*lockMutex)(MUTEX);
typedef VOID (*unlockMutex)(MUTEX);
typedef BOOL (*tryLockMutex)(MUTEX);
typedef VOID (*freeMutex)(MUTEX);
typedef STATUS (*createThread)(PTID, startRoutine, PVOID);
typedef UINT32 (*createThreadEx)(PTID, PCHAR, UINT32, BOOL, startRoutine, PVOID);
typedef UINT32 (*createThreadExExt)(PTID, PCHAR, UINT32, BOOL, startRoutine, PVOID);
typedef UINT32 (*createThreadExPri)(PTID, PCHAR, UINT32, BOOL, startRoutine, INT32, PVOID);
typedef STATUS (*joinThread)(TID, PVOID*);
typedef VOID (*threadSleep)(UINT64);
typedef VOID (*threadSleepUntil)(UINT64);
typedef STATUS (*cancelThread)(TID);
typedef STATUS (*detachThread)(TID);
typedef CVAR (*createConditionVariable)();
typedef STATUS (*signalConditionVariable)(CVAR);
typedef STATUS (*broadcastConditionVariable)(CVAR);
typedef STATUS (*waitConditionVariable)(CVAR, MUTEX, UINT64);
typedef VOID (*freeConditionVariable)(CVAR);

//
// Atomics functions
//
typedef SIZE_T (*atomicLoad)(volatile SIZE_T*);
typedef VOID (*atomicStore)(volatile SIZE_T*, SIZE_T);
typedef SIZE_T (*atomicExchange)(volatile SIZE_T*, SIZE_T);
typedef BOOL (*atomicCompareExchange)(volatile SIZE_T*, SIZE_T*, SIZE_T);
typedef SIZE_T (*atomicIncrement)(volatile SIZE_T*);
typedef SIZE_T (*atomicDecrement)(volatile SIZE_T*);
typedef SIZE_T (*atomicAdd)(volatile SIZE_T*, SIZE_T);
typedef SIZE_T (*atomicSubtract)(volatile SIZE_T*, SIZE_T);
typedef SIZE_T (*atomicAnd)(volatile SIZE_T*, SIZE_T);
typedef SIZE_T (*atomicOr)(volatile SIZE_T*, SIZE_T);
typedef SIZE_T (*atomicXor)(volatile SIZE_T*, SIZE_T);

//
// Atomics
//
extern PUBLIC_API atomicLoad globalAtomicLoad;
extern PUBLIC_API atomicStore globalAtomicStore;
extern PUBLIC_API atomicExchange globalAtomicExchange;
extern PUBLIC_API atomicCompareExchange globalAtomicCompareExchange;
extern PUBLIC_API atomicIncrement globalAtomicIncrement;
extern PUBLIC_API atomicDecrement globalAtomicDecrement;
extern PUBLIC_API atomicAdd globalAtomicAdd;
extern PUBLIC_API atomicSubtract globalAtomicSubtract;
extern PUBLIC_API atomicAnd globalAtomicAnd;
extern PUBLIC_API atomicOr globalAtomicOr;
extern PUBLIC_API atomicXor globalAtomicXor;

// Max string length for platform name
#define MAX_PLATFORM_NAME_STRING_LEN 128

// Max string length for the OS version
#define MAX_OS_VERSION_STRING_LEN 128

// Max string length for the compiler info
#define MAX_COMPILER_INFO_STRING_LEN 128

//
// Version macros
//
#define GET_PLATFORM_NAME globalGetPlatformName
#define GET_OS_VERSION    globalGetOsVersion
#define GET_COMPILER_INFO globalGetCompilerInfo

//
// Memory allocation and operations
//
#define MEMALLOC      globalMemAlloc
#define MEMALIGNALLOC globalMemAlignAlloc
#define MEMCALLOC     globalMemCalloc
#define MEMREALLOC    globalMemRealloc
#define MEMFREE       globalMemFree
#define MEMCMP        memcmp
#define MEMSET        memset
#ifndef MEMCPY
#define MEMCPY        memcpy
#endif
#ifndef MEMMOVE
#define MEMMOVE       memmove
#endif
#ifndef REALLOC
#define REALLOC       realloc
#endif

//
// Whether the buffer contains the same char
//
#define MEMCHK globalMemChk

//
// String operations
//
#define STRCAT     strcat
#define STRNCAT    strncat
#define STRCPY     strcpy
#define STRNCPY    strncpy
#define STRLEN     strlen
#define STRNLEN    strnlen
#define STRCHR     strchr
#define STRNCHR    strnchr
#define STRRCHR    strrchr
#define STRCMP     strcmp
#define STRCMPI    GLOBAL_STRCMPI
#define STRNCMPI   GLOBAL_STRNCMPI
#define STRNCMP    strncmp
#define PRINTF     printf
#define SPRINTF    sprintf
#define SNPRINTF   snprintf
#define SSCANF     sscanf
#define TRIMSTRALL trimstrall
#define LTRIMSTR   ltrimstr
#define RTRIMSTR   rtrimstr
#define STRSTR     strstr
#ifdef strnstr
#define STRNSTR strnstr
#else
#define STRNSTR defaultStrnstr
#endif
#define TOLOWER    tolower
#define TOUPPER    toupper
#define TOLOWERSTR tolowerstr
#define TOUPPERSTR toupperstr

#define MKTIME mktime

//
// Environment variables
//
#define GETENV getenv

//
// Empty string definition
//
#define EMPTY_STRING ((PCHAR) "")

//
// Check if string is empty
//
#define IS_EMPTY_STRING(str) ((str)[0] == '\0')

//
// Check if string is null or empty
//
#define IS_NULL_OR_EMPTY_STRING(str) (str == NULL || IS_EMPTY_STRING(str))

//
// Pseudo-random functionality
//
#ifndef SRAND
#define SRAND srand
#endif
#ifndef RAND
#define RAND rand
#endif

//
// CRT functionality
//
#define STRTOUL  strtoul
#define ULLTOSTR ulltostr
#define ULTOSTR  ultostr

//
// File operations
//
#ifndef FOPEN
#define FOPEN fopen
#endif
#ifndef FCLOSE
#define FCLOSE fclose
#endif
#ifndef FWRITE
#define FWRITE fwrite
#endif
#ifndef FPUTC
#define FPUTC fputc
#endif
#ifndef FREAD
#define FREAD fread
#endif
#ifndef FEOF
#define FEOF feof
#endif
#ifndef FSEEK
#if defined _WIN32 || defined _WIN64
#define FSEEK _fseeki64
#else
#define FSEEK fseek
#endif
#endif
#ifndef FTELL
#if defined _WIN32 || defined _WIN64
#define FTELL _ftelli64
#else
#define FTELL ftell
#endif
#endif
#ifndef FREMOVE
#define FREMOVE remove
#endif
#ifndef FUNLINK
#define FUNLINK unlink
#endif
#ifndef FREWIND
#define FREWIND rewind
#endif
#ifndef FRENAME
#define FRENAME rename
#endif
#ifndef FTMPFILE
#define FTMPFILE tmpfile
#endif
#ifndef FTMPNAME
#define FTMPNAME tmpnam
#endif
#ifndef FOPENDIR
#define FOPENDIR opendir
#endif
#ifndef FCLOSEDIR
#define FCLOSEDIR closedir
#endif
#ifndef FREADDIR
#define FREADDIR readdir
#endif
#ifndef FMKDIR
#define FMKDIR GLOBAL_MKDIR
#endif
#ifndef FRMDIR
#define FRMDIR GLOBAL_RMDIR
#endif
#ifndef FSTAT
#define FSTAT GLOBAL_STAT
#endif
#ifndef FSCANF
#define FSCANF fscanf
#endif

#define FPATHSEPARATOR     '/'
#define FPATHSEPARATOR_STR "/"

#include "kvs_string.h"

//
// String to integer conversion
//
#define STRTOUI32 strtoui32
#define STRTOI32  strtoi32
#define STRTOUI64 strtoui64
#define STRTOI64  strtoi64

//
// Thread functionality
//
#define GETTID   globalGetThreadId
#define GETTNAME globalGetThreadName

//
// Time functionality
//
#define GETTIME     globalGetTime
#define GETREALTIME globalGetRealTime
#define STRFTIME    strftime
#define GMTIME      gmtime

#if defined _WIN32 || defined _WIN64 || defined __CYGWIN__
#define GMTIME_THREAD_SAFE GMTIME
#else
#define GMTIME_THREAD_SAFE globalGetThreadSafeTmTime
#endif

//
// Thread and Mutex related functionality
//
extern createMutex globalCreateMutex;
extern lockMutex globalLockMutex;
extern unlockMutex globalUnlockMutex;
extern tryLockMutex globalTryLockMutex;
extern freeMutex globalFreeMutex;
extern createThread globalCreateThread;
extern joinThread globalJoinThread;
extern threadSleep globalThreadSleep;
extern threadSleepUntil globalThreadSleepUntil;
extern cancelThread globalCancelThread;
extern detachThread globalDetachThread;
extern createConditionVariable globalConditionVariableCreate;
extern signalConditionVariable globalConditionVariableSignal;
extern broadcastConditionVariable globalConditionVariableBroadcast;
extern waitConditionVariable globalConditionVariableWait;
extern freeConditionVariable globalConditionVariableFree;

//
// Mutex functionality
//
#define MUTEX_CREATE  globalCreateMutex
#define MUTEX_LOCK    globalLockMutex
#define MUTEX_UNLOCK  globalUnlockMutex
#define MUTEX_TRYLOCK globalTryLockMutex
#define MUTEX_FREE    globalFreeMutex

//
// Condition variable functionality
//
#define CVAR_CREATE    globalConditionVariableCreate
#define CVAR_SIGNAL    globalConditionVariableSignal
#define CVAR_BROADCAST globalConditionVariableBroadcast
#define CVAR_WAIT      globalConditionVariableWait
#define CVAR_FREE      globalConditionVariableFree

//
// Static initializers
//
#define MUTEX_INIT           GLOBAL_MUTEX_INIT
#define MUTEX_INIT_RECURSIVE GLOBAL_MUTEX_INIT_RECURSIVE
#define CVAR_INIT            GLOBAL_CVAR_INIT

//
// Basic Atomics functionality
//
#define ATOMIC_LOAD             globalAtomicLoad
#define ATOMIC_STORE            globalAtomicStore
#define ATOMIC_EXCHANGE         globalAtomicExchange
#define ATOMIC_COMPARE_EXCHANGE globalAtomicCompareExchange
#define ATOMIC_INCREMENT        globalAtomicIncrement
#define ATOMIC_DECREMENT        globalAtomicDecrement
#define ATOMIC_ADD              globalAtomicAdd
#define ATOMIC_SUBTRACT         globalAtomicSubtract
#define ATOMIC_AND              globalAtomicAnd
#define ATOMIC_OR               globalAtomicOr
#define ATOMIC_XOR              globalAtomicXor

//
// Helper atomics
//
#define ATOMIC_LOAD_BOOL             (BOOL) globalAtomicLoad
#define ATOMIC_STORE_BOOL(a, b)      ATOMIC_STORE((a), (SIZE_T) (b))
#define ATOMIC_EXCHANGE_BOOL         (BOOL) globalAtomicExchange
#define ATOMIC_COMPARE_EXCHANGE_BOOL (BOOL) globalAtomicCompareExchange
#define ATOMIC_AND_BOOL              (BOOL) globalAtomicAnd
#define ATOMIC_OR_BOOL               (BOOL) globalAtomicOr
#define ATOMIC_XOR_BOOL              (BOOL) globalAtomicXor

#ifndef SQRT
#include <math.h>
#define SQRT sqrt
#endif

//
// Calculate the byte offset of a field in a structure of type type.
//
#ifndef FIELD_OFFSET
#define FIELD_OFFSET(type, field) ((LONG) (LONG_PTR) & (((type*) 0)->field))
#endif

//
// Aligns an integer X value up or down to alignment A
//
#define ROUND_DOWN(X, A) ((X) & ~((A) -1))
#define ROUND_UP(X, A)   (((X) + (A) -1) & ~((A) -1))

//
// Macros to swap endinanness
//
#define LOW_BYTE(x)   ((BYTE) (x))
#define HIGH_BYTE(x)  ((BYTE) (((INT16) (x) >> 8) & 0xFF))
#define LOW_INT16(x)  ((INT16) (x))
#define HIGH_INT16(x) ((INT16) (((INT32) (x) >> 16) & 0xFFFF))
#define LOW_INT32(x)  ((INT32) (x))
#define HIGH_INT32(x) ((INT32) (((INT64) (x) >> 32) & 0xFFFFFFFF))

#define MAKE_INT16(a, b) ((INT16) (((UINT8) ((UINT16) (a) & 0xff)) | ((UINT16) ((UINT8) ((UINT16) (b) & 0xff))) << 8))
#define MAKE_INT32(a, b) ((INT32) (((UINT16) ((UINT32) (a) & 0xffff)) | ((UINT32) ((UINT16) ((UINT32) (b) & 0xffff))) << 16))
#define MAKE_INT64(a, b) ((INT64) (((UINT32) ((UINT64) (a) & 0xffffffff)) | ((UINT64) ((UINT32) ((UINT64) (b) & 0xffffffff))) << 32))

#define SWAP_INT16(x) MAKE_INT16(HIGH_BYTE(x), LOW_BYTE(x))

#define SWAP_INT32(x) MAKE_INT32(SWAP_INT16(HIGH_INT16(x)), SWAP_INT16(LOW_INT16(x)))

#define SWAP_INT64(x) MAKE_INT64(SWAP_INT32(HIGH_INT32(x)), SWAP_INT32(LOW_INT32(x)))

//
// Check if at most 1 bit is set
//
#define CHECK_POWER_2(x) !((x) & ((x) -1))

//
// Checks if only 1 bit is set
//
#define CHECK_SINGLE_BIT_SET(x) ((x) && CHECK_POWER_2(x))

//
// Handle definitions
//
#if !defined(HANDLE) && !defined(_WINNT_)
typedef UINT64 HANDLE;
#endif
#ifndef INVALID_PIC_HANDLE_VALUE
#define INVALID_PIC_HANDLE_VALUE ((UINT64) NULL)
#endif
#ifndef IS_VALID_HANDLE
#define IS_VALID_HANDLE(h) ((h) != INVALID_PIC_HANDLE_VALUE)
#endif
#ifndef POINTER_TO_HANDLE
#define POINTER_TO_HANDLE(h) ((UINT64) (h))
#endif
#ifndef HANDLE_TO_POINTER
#define HANDLE_TO_POINTER(h) ((PBYTE) (h))
#endif

////////////////////////////////////////////////////
// Conditional checks
////////////////////////////////////////////////////
#define CHK(condition, errRet)                                                                                                                       \
    do {                                                                                                                                             \
        if (!(condition)) {                                                                                                                          \
            retStatus = (errRet);                                                                                                                    \
            goto CleanUp;                                                                                                                            \
        }                                                                                                                                            \
    } while (FALSE)

#define CHK_ERR(condition, errRet, errorMessage, ...)                                                                                                \
    do {                                                                                                                                             \
        if (!(condition)) {                                                                                                                          \
            retStatus = (errRet);                                                                                                                    \
            DLOGE(errorMessage, ##__VA_ARGS__);                                                                                                      \
            goto CleanUp;                                                                                                                            \
        }                                                                                                                                            \
    } while (FALSE)

#define CHK_WARN(condition, errRet, errorMessage, ...)                                                                                               \
    do {                                                                                                                                             \
        if (!(condition)) {                                                                                                                          \
            retStatus = (errRet);                                                                                                                    \
            DLOGW(errorMessage, ##__VA_ARGS__);                                                                                                      \
            goto CleanUp;                                                                                                                            \
        }                                                                                                                                            \
    } while (FALSE)

#define CHK_STATUS_ERR(condition, errRet, errorMessage, ...)                                                                                         \
    do {                                                                                                                                             \
        STATUS __status = condition;                                                                                                                 \
        if (STATUS_FAILED(__status)) {                                                                                                               \
            retStatus = (__status);                                                                                                                  \
            DLOGE(errorMessage, ##__VA_ARGS__);                                                                                                      \
            goto CleanUp;                                                                                                                            \
        }                                                                                                                                            \
    } while (FALSE)

#define CHK_STATUS(condition)                                                                                                                        \
    do {                                                                                                                                             \
        STATUS __status = condition;                                                                                                                 \
        if (STATUS_FAILED(__status)) {                                                                                                               \
            retStatus = (__status);                                                                                                                  \
            goto CleanUp;                                                                                                                            \
        }                                                                                                                                            \
    } while (FALSE)

#define CHK_STATUS_CONTINUE(condition)                                                                                                               \
    do {                                                                                                                                             \
        STATUS __status = condition;                                                                                                                 \
        if (STATUS_FAILED(__status)) {                                                                                                               \
            retStatus = __status;                                                                                                                    \
        }                                                                                                                                            \
    } while (FALSE)

#define CHK_HANDLE(handle)                                                                                                                           \
    do {                                                                                                                                             \
        if (IS_VALID_HANDLE(handle)) {                                                                                                               \
            retStatus = (STATUS_INVALID_HANDLE_ERROR);                                                                                               \
            goto CleanUp;                                                                                                                            \
        }                                                                                                                                            \
    } while (FALSE)

#define CHK_LOG(condition, logMessage)                                                                                                               \
    do {                                                                                                                                             \
        STATUS __status = condition;                                                                                                                 \
        if (STATUS_FAILED(__status)) {                                                                                                               \
            DLOGS("%s Returned status code: 0x%08x", logMessage, __status);                                                                          \
        }                                                                                                                                            \
    } while (FALSE)

#define CHK_LOG_ERR(condition)                                                                                                                       \
    do {                                                                                                                                             \
        STATUS __status = condition;                                                                                                                 \
        if (STATUS_FAILED(__status)) {                                                                                                               \
            DLOGE("operation returned status code: 0x%08x", __status);                                                                               \
        }                                                                                                                                            \
    } while (FALSE)

// Checks if the data provided is in range [low, high]
#define CHECK_IN_RANGE(data, low, high) ((data) >= (low) && (data) <= (high))

////////////////////////////////////////////////////
// Callbacks definitions
////////////////////////////////////////////////////
/**
 * Gets the current time in 100ns from some timestamp.
 *
 * @param 1 UINT64 - Custom handle passed by the caller.
 *
 * @return Current time value in 100ns
 */
typedef UINT64 (*GetCurrentTimeFunc)(UINT64);

// Platform specific stubs
struct utsname {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
};

static inline int uname(struct utsname *buf) {
    if (buf != NULL) {
        strcpy(buf->sysname, "ESP32");
        strcpy(buf->nodename, "esp32");
        strcpy(buf->release, "1.0");
        strcpy(buf->version, "1.0");
        strcpy(buf->machine, "esp32");
        return 0;
    }
    return -1;
}

#ifndef LOG_CLASS
#define LOG_CLASS "KVS"
#endif

#include "time_port.h"
#include "mutex.h"
#include "single_linked_list.h"
#include "double_linked_list.h"
#include "stack_queue.h"
#include "directory.h"
#include "semaphore.h"

/**
 * @brief Request Header structure
 */
typedef struct __RequestHeader RequestHeader;
struct __RequestHeader {
    PCHAR pName;     //!< Request header name
    UINT32 nameLen;  //!< Header name length
    PCHAR pValue;    //!< Request header value
    UINT32 valueLen; //!< Header value length
};
typedef struct __RequestHeader* PRequestHeader;

/**
 * @brief Request info structure
 */
typedef struct __RequestInfo RequestInfo;
struct __RequestInfo {
    volatile ATOMIC_BOOL terminating;         //!< Indicating whether the request is being terminated
    HTTP_REQUEST_VERB verb;                   //!< HTTP verb
    PCHAR body;                               //!< Body of the request.
                                              //!< NOTE: In streaming mode the body will be NULL
                                              //!< NOTE: The body will follow the main struct
    UINT32 bodySize;                          //!< Size of the body in bytes
#if USE_DYNAMIC_URL
    PCHAR url;                                //!< The URL for the request
#else
    CHAR url[MAX_URI_CHAR_LEN + 1];           //!< The URL for the request
#endif
    CHAR certPath[MAX_PATH_LEN + 1];          //!< CA Certificate path to use - optional
    CHAR sslCertPath[MAX_PATH_LEN + 1];       //!< SSL Certificate file path to use - optional
    CHAR sslPrivateKeyPath[MAX_PATH_LEN + 1]; //!< SSL Certificate private key file path to use - optional
    SSL_CERTIFICATE_TYPE certType;            //!< One of the following types "DER", "PEM", "ENG"
    CHAR region[MAX_REGION_NAME_LEN + 1];     //!< Region
    UINT64 currentTime;                       //!< Current time when request was created
    UINT64 completionTimeout;                 //!< Call completion timeout
    UINT64 connectionTimeout;                 //!< Connection completion timeout
    UINT64 callAfter;                         //!< Call after time
    UINT64 lowSpeedLimit;                     //!< Low-speed limit
    UINT64 lowSpeedTimeLimit;                 //!< Low-time limit
    PAwsCredentials pAwsCredentials;          //!< AWS Credentials
    PSingleList pRequestHeaders;              //!< Request headers
};
typedef struct __RequestInfo* PRequestInfo;

/**
 * @brief Call Info structure
 */
typedef struct __CallInfo CallInfo;
struct __CallInfo {
    PRequestInfo pRequestInfo;                        //!< Original request info
    UINT32 httpStatus;                                //!< HTTP status code of the execution
    SERVICE_CALL_RESULT callResult;                   //!< Execution result
    CHAR errorBuffer[CALL_INFO_ERROR_BUFFER_LEN + 1]; //!< Error buffer for curl calls
    PStackQueue pResponseHeaders;                     //!< Response Headers list
    PRequestHeader pRequestId;                        //!< Request ID if specified
    PCHAR responseData;                               //!< Buffer to write the data to - will be allocated. Buffer is freed by a caller.
    UINT32 responseDataLen;                           //!< Response data size
};
typedef struct __CallInfo* PCallInfo;

#include "request_info.h"
#include "thread.h"

#ifdef __cplusplus
}
#endif
