//
// Common definition file
//

#ifndef __COMMON_DEFINITIONS__
#define __COMMON_DEFINITIONS__

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

////////////////////////////////////////////////////
// Project defines
////////////////////////////////////////////////////
#if defined _WIN32 || defined __CYGWIN__
#if defined __GNUC__ || defined __GNUG__
#define INLINE     inline
#define LIB_EXPORT __attribute__((dllexport))
#define LIB_IMPORT __attribute__((dllimport))
#define LIB_INTERNAL
#define ALIGN_4 __attribute__((aligned(4)))
#define ALIGN_8 __attribute__((aligned(8)))
#define PACKED  __attribute__((__packed__))
#define DISCARDABLE
#else
#define INLINE     _inline
#define LIB_EXPORT __declspec(dllexport)
#define LIB_IMPORT __declspec(dllimport)
#define LIB_INTERNAL
#define ALIGN_4 __declspec(align(4))
#define ALIGN_8 __declspec(align(8))
#define PACKED
#define DISCARDABLE __declspec(selectany)
#endif
#else
#if __GNUC__ >= 4
#define INLINE       inline
#define LIB_EXPORT   __attribute__((visibility("default")))
#define LIB_IMPORT   __attribute__((visibility("default")))
#define LIB_INTERNAL __attribute__((visibility("hidden")))
#define ALIGN_4      __attribute__((aligned(4)))
#define ALIGN_8      __attribute__((aligned(8)))
#define PACKED       __attribute__((__packed__))
#define DISCARDABLE
#else
#define LIB_EXPORT
#define LIB_IMPORT
#define LIB_INTERNAL
#define ALIGN_4
#define ALIGN_8
#define DISCARDABLE
#define PACKED
#endif
#endif

#define PUBLIC_API  LIB_EXPORT
#define PRIVATE_API LIB_INTERNAL

//
// 64/32 bit check on Windows platforms
//
#if defined _WIN32 || defined _WIN64
#if defined _WIN64
#define SIZE_64
#define __LLP64__ // win64 uses LLP64 data model
#else
#define SIZE_32
#endif
#endif

//
// 64/32 bit check on GCC
//
#if defined __GNUC__ || defined __GNUG__
#if defined __x86_64__ || defined __ppc64__ || defined __aarch64__
#define SIZE_64
#if defined __APPLE__
#define __LLP64__
#else
#ifndef __LP64__
#define __LP64__ // Linux uses LP64 data model
#endif
#endif
#else
#define SIZE_32
#endif
#endif

#if defined _WIN32
#if defined _MSC_VER
#include <Windows.h>
#include <direct.h>
#elif defined(__MINGW64__) || defined(__MINGW32__)
#define WINVER       0x0A00
#define _WIN32_WINNT 0x0A00
#include <windows.h>
#else
#error "Unknown Windows platform!"
#endif
#define __WINDOWS_BUILD__
#ifndef _WINNT_
typedef char CHAR;
typedef short WCHAR;
typedef unsigned __int8 UINT8;
typedef __int8 INT8;
typedef unsigned __int16 UINT16;
typedef __int16 INT16;
typedef unsigned __int32 UINT32;
typedef __int32 INT32;
typedef unsigned __int64 UINT64;
typedef __int64 INT64;
typedef double DOUBLE;
typedef long double LDOUBLE;
typedef float FLOAT;
#endif

typedef double DOUBLE;
typedef long double LDOUBLE;

#elif defined(__GNUC__)

#include <stdint.h>

typedef char CHAR;
typedef short WCHAR;
typedef uint8_t UINT8;
typedef int8_t INT8;
typedef uint16_t UINT16;
typedef int16_t INT16;
typedef uint32_t UINT32;
typedef int32_t INT32;
typedef uint64_t UINT64;
typedef int64_t INT64;
typedef double DOUBLE;
typedef long double LDOUBLE;
typedef float FLOAT;

#else

typedef char CHAR;
typedef short WCHAR;
typedef unsigned char UINT8;
typedef char INT8;
typedef unsigned short UINT16;
typedef short INT16;
typedef unsigned long UINT32;
typedef long INT32;
typedef unsigned long long UINT64;
typedef long long INT64;
typedef double DOUBLE;
typedef long double LDOUBLE;
typedef float FLOAT;

#endif

#ifndef VOID
#define VOID void
#endif

#ifdef __APPLE__
#ifndef OBJC_BOOL_DEFINED
#include "TargetConditionals.h"
#if TARGET_IPHONE_SIMULATOR
// iOS Simulator
typedef bool BOOL;
#elif TARGET_OS_IPHONE
// iOS device
typedef signed char BOOL;
#elif TARGET_OS_MAC
// Other kinds of Mac OS
typedef INT32 BOOL;
#else
#error "Unknown Apple platform"
#endif
#endif
#else
typedef INT32 BOOL;
#endif

typedef UINT8 BYTE;
typedef VOID* PVOID;
typedef BYTE* PBYTE;
typedef BOOL* PBOOL;
typedef CHAR* PCHAR;
typedef WCHAR* PWCHAR;
typedef INT8* PINT8;
typedef UINT8* PUINT8;
typedef INT16* PINT16;
typedef UINT16* PUINT16;
typedef INT32* PINT32;
typedef UINT32* PUINT32;
typedef INT64* PINT64;
typedef UINT64* PUINT64;
typedef long LONG, *PLONG;
typedef unsigned long ULONG, *PULONG;
typedef DOUBLE* PDOUBLE;
typedef LDOUBLE* PLDOUBLE;
typedef FLOAT* PFLOAT;

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

// Thread Id
typedef UINT64 TID;
typedef TID* PTID;

#ifndef INVALID_TID_VALUE
#define INVALID_TID_VALUE ((UINT64) NULL)
#endif

#ifndef IS_VALID_TID_VALUE
#define IS_VALID_TID_VALUE(t) ((t) != INVALID_TID_VALUE)
#endif

// Mutex typedef
typedef UINT64 MUTEX;

#ifndef INVALID_MUTEX_VALUE
#define INVALID_MUTEX_VALUE ((UINT64) NULL)
#endif

#ifndef IS_VALID_MUTEX_VALUE
#define IS_VALID_MUTEX_VALUE(m) ((m) != INVALID_MUTEX_VALUE)
#endif

// Conditional variable
#if defined __WINDOWS_BUILD__
typedef PCONDITION_VARIABLE CVAR;
#else
#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif
#include <pthread.h>
#include <signal.h>
typedef pthread_cond_t* CVAR;
#endif

#ifndef INVALID_CVAR_VALUE
#define INVALID_CVAR_VALUE ((CVAR) NULL)
#endif

#ifndef IS_VALID_CVAR_VALUE
#define IS_VALID_CVAR_VALUE(c) ((c) != INVALID_CVAR_VALUE)
#endif

// Max thread name buffer length - similar to Linux platforms
#ifndef MAX_THREAD_NAME
#define MAX_THREAD_NAME 16
#endif

// Max mutex name
#ifndef MAX_MUTEX_NAME
#define MAX_MUTEX_NAME 32
#endif

// Content ID - 64 bit uint
typedef UINT64 CID;
typedef CID* PCID;

//
// int and long ptr definitions
//
#if defined SIZE_64
typedef INT64 INT_PTR, *PINT_PTR;
typedef UINT64 UINT_PTR, *PUINT_PTR;

typedef INT64 LONG_PTR, *PLONG_PTR;
typedef UINT64 ULONG_PTR, *PULONG_PTR;

#ifndef PRIu64
#ifdef __LLP64__
#define PRIu64 "llu"
#else // __LP64__
#define PRIu64 "lu"
#endif
#endif

#ifndef PRIx64
#ifdef __LLP64__
#define PRIx64 "llx"
#else // __LP64__
#define PRIx64 "lx"
#endif
#endif

#ifndef PRId64
#ifdef __LLP64__
#define PRId64 "lld"
#else // __LP64__
#define PRId64 "ld"
#endif
#endif

#elif defined SIZE_32
typedef INT32 INT_PTR, *PINT_PTR;
typedef UINT32 UINT_PTR, *PUINT_PTR;

#ifndef __int3264 // defined in Windows 'basetsd.h'
typedef INT64 LONG_PTR, *PLONG_PTR;
typedef UINT64 ULONG_PTR, *PULONG_PTR;
#endif

#ifndef PRIu64
#define PRIu64 "llu"
#endif

#ifndef PRIx64
#define PRIx64 "llx"
#endif

#ifndef PRId64
#define PRId64 "lld"
#endif

#else
#error "Environment not 32 or 64-bit."
#endif

//
// Define pointer width size types
//
#ifndef _SIZE_T_DEFINED_IN_COMMON
#if defined(__MINGW32__)
typedef ULONG_PTR SIZE_T, *PSIZE_T;
typedef LONG_PTR SSIZE_T, *PSSIZE_T;
#elif !(defined _WIN32 || defined _WIN64)
typedef size_t SIZE_T, *PSIZE_T;
typedef INT_PTR SSIZE_T, *PSSIZE_T;
#endif
#define _SIZE_T_DEFINED_IN_COMMON
#endif

//
// Stringification macro
//
#define STR_(s) #s
#define STR(s)  STR_(s)

//
// NULL definition
//
#ifndef NULL
#ifdef __cplusplus
#define NULL 0
#else /* __cplusplus */
#define NULL ((void*) 0)
#endif /* __cplusplus */
#endif /* NULL */

//
// Max and Min definitions
//
#define MAX_UINT8 ((UINT8) 0xff)
#define MAX_INT8  ((INT8) 0x7f)
#define MIN_INT8  ((INT8) 0x80)

#define MAX_UINT16 ((UINT16) 0xffff)
#define MAX_INT16  ((INT16) 0x7fff)
#define MIN_INT16  ((INT16) 0x8000)

#define MAX_UINT32 ((UINT32) 0xffffffff)
#define MAX_INT32  ((INT32) 0x7fffffff)
#define MIN_INT32  ((INT32) 0x80000000)

#define MAX_UINT64 ((UINT64) 0xffffffffffffffff)
#define MAX_INT64  ((INT64) 0x7fffffffffffffff)
#define MIN_INT64  ((INT64) 0x8000000000000000)

//
// NOTE: Timer precision is in 100ns intervals. This is used in heuristics and in time functionality
//
#define DEFAULT_TIME_UNIT_IN_NANOS         100
#define HUNDREDS_OF_NANOS_IN_A_MICROSECOND ((INT64) 10)
#define HUNDREDS_OF_NANOS_IN_A_MILLISECOND (HUNDREDS_OF_NANOS_IN_A_MICROSECOND * ((INT64) 1000))
#define HUNDREDS_OF_NANOS_IN_A_SECOND      (HUNDREDS_OF_NANOS_IN_A_MILLISECOND * ((INT64) 1000))
#define HUNDREDS_OF_NANOS_IN_A_MINUTE      (HUNDREDS_OF_NANOS_IN_A_SECOND * ((INT64) 60))
#define HUNDREDS_OF_NANOS_IN_AN_HOUR       (HUNDREDS_OF_NANOS_IN_A_MINUTE * ((INT64) 60))

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
            delete[](p);                                                                                                                             \
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

////////////////////////////////////////////////////
// Status definitions
////////////////////////////////////////////////////
#define STATUS UINT32

#define STATUS_SUCCESS ((STATUS) 0x00000000)

#define STATUS_FAILED(x)    (((STATUS) (x)) != STATUS_SUCCESS)
#define STATUS_SUCCEEDED(x) (!STATUS_FAILED(x))

#define STATUS_BASE                              0x00000000
#define STATUS_NULL_ARG                          STATUS_BASE + 0x00000001
#define STATUS_INVALID_ARG                       STATUS_BASE + 0x00000002
#define STATUS_INVALID_ARG_LEN                   STATUS_BASE + 0x00000003
#define STATUS_NOT_ENOUGH_MEMORY                 STATUS_BASE + 0x00000004
#define STATUS_BUFFER_TOO_SMALL                  STATUS_BASE + 0x00000005
#define STATUS_UNEXPECTED_EOF                    STATUS_BASE + 0x00000006
#define STATUS_FORMAT_ERROR                      STATUS_BASE + 0x00000007
#define STATUS_INVALID_HANDLE_ERROR              STATUS_BASE + 0x00000008
#define STATUS_OPEN_FILE_FAILED                  STATUS_BASE + 0x00000009
#define STATUS_READ_FILE_FAILED                  STATUS_BASE + 0x0000000a
#define STATUS_WRITE_TO_FILE_FAILED              STATUS_BASE + 0x0000000b
#define STATUS_INTERNAL_ERROR                    STATUS_BASE + 0x0000000c
#define STATUS_INVALID_OPERATION                 STATUS_BASE + 0x0000000d
#define STATUS_NOT_IMPLEMENTED                   STATUS_BASE + 0x0000000e
#define STATUS_OPERATION_TIMED_OUT               STATUS_BASE + 0x0000000f
#define STATUS_NOT_FOUND                         STATUS_BASE + 0x00000010
#define STATUS_CREATE_THREAD_FAILED              STATUS_BASE + 0x00000011
#define STATUS_THREAD_NOT_ENOUGH_RESOURCES       STATUS_BASE + 0x00000012
#define STATUS_THREAD_INVALID_ARG                STATUS_BASE + 0x00000013
#define STATUS_THREAD_PERMISSIONS                STATUS_BASE + 0x00000014
#define STATUS_THREAD_DEADLOCKED                 STATUS_BASE + 0x00000015
#define STATUS_THREAD_DOES_NOT_EXIST             STATUS_BASE + 0x00000016
#define STATUS_JOIN_THREAD_FAILED                STATUS_BASE + 0x00000017
#define STATUS_WAIT_FAILED                       STATUS_BASE + 0x00000018
#define STATUS_CANCEL_THREAD_FAILED              STATUS_BASE + 0x00000019
#define STATUS_THREAD_IS_NOT_JOINABLE            STATUS_BASE + 0x0000001a
#define STATUS_DETACH_THREAD_FAILED              STATUS_BASE + 0x0000001b
#define STATUS_THREAD_ATTR_INIT_FAILED           STATUS_BASE + 0x0000001c
#define STATUS_THREAD_ATTR_SET_STACK_SIZE_FAILED STATUS_BASE + 0x0000001d
#define STATUS_MEMORY_NOT_FREED                  STATUS_BASE + 0x0000001e
#define STATUS_INVALID_THREAD_PARAMS_VERSION     STATUS_BASE + 0x0000001f

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
#include <sys/utsname.h>
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
#define S_ISDIR(mode) (((mode) &S_IFMT) == S_IFDIR)
#endif

#ifndef S_ISREG
#define S_ISREG(mode) (((mode) &S_IFMT) == S_IFREG)
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

#include <dlfcn.h>

#if !defined(__MACH__)
#include <sys/prctl.h>
#endif

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
// Dynamic library loading function definitions
//
typedef PVOID (*dlOpen)(PCHAR filename, UINT32 flag);
typedef INT32 (*dlClose)(PVOID handle);
typedef PVOID (*dlSym)(PVOID handle, PCHAR symbol);
typedef PCHAR (*dlError)();

//
// Default dynamic library loading functions
//
PVOID defaultDlOpen(PCHAR filename, UINT32 flag);
INT32 defaultDlClose(PVOID handle);
PVOID defaultDlSym(PVOID handle, PCHAR symbol);
PCHAR defaultDlError();

//
// Global dynamic library loading function pointers
//
extern dlOpen globalDlOpen;
extern dlClose globalDlClose;
extern dlSym globalDlSym;
extern dlError globalDlError;

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

/**
 * Current version of the thread parameters structure
 */
#define THREAD_PARAMS_CURRENT_VERSION 0

typedef struct __ThreadParams ThreadParams;
struct __ThreadParams {
    // Version of the struct
    UINT32 version;

    // Stack size, in bytes. 0 = use defaults
    SIZE_T stackSize;
};
typedef struct __ThreadParams* PThreadParams;

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
typedef STATUS (*createThreadWithParams)(PTID, PThreadParams, startRoutine, PVOID);
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
// Version functions
//
typedef STATUS (*getPlatformName)(PCHAR, UINT32);
typedef STATUS (*getOsVersion)(PCHAR, UINT32);
typedef STATUS (*getCompilerInfo)(PCHAR, UINT32);

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
// Thread and Mutex related functionality
//
extern createMutex globalCreateMutex;
extern lockMutex globalLockMutex;
extern unlockMutex globalUnlockMutex;
extern tryLockMutex globalTryLockMutex;
extern freeMutex globalFreeMutex;
extern createThread globalCreateThread;
extern createThreadWithParams globalCreateThreadWithParams;
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
// Version information
//
extern getPlatformName globalGetPlatformName;
extern getOsVersion globalGetOsVersion;
extern getCompilerInfo globalGetCompilerInfo;

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
#define MEMCPY        memcpy
#define MEMSET        memset
#define MEMMOVE       memmove
#define REALLOC       realloc

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

#if defined _WIN32 || defined _WIN64 || defined __CYGWIN__
#define FPATHSEPARATOR     '\\'
#define FPATHSEPARATOR_STR "\\"
#else
#define FPATHSEPARATOR     '/'
#define FPATHSEPARATOR_STR "/"
#endif
//
// String to integer conversion
//
#define STRTOUI32 strtoui32
#define STRTOI32  strtoi32
#define STRTOUI64 strtoui64
#define STRTOI64  strtoi64

//
// Dynamic library loading routines
//
#define DLOPEN  globalDlOpen
#define DLCLOSE globalDlClose
#define DLSYM   globalDlSym
#define DLERROR globalDlError

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
// Thread functionality
//
// Wrappers around OS specific utilities for threads. Takes arguments as given.
#define THREAD_CREATE             globalCreateThread
#define THREAD_CREATE_WITH_PARAMS globalCreateThreadWithParams
#define THREAD_JOIN               globalJoinThread
#define THREAD_SLEEP              globalThreadSleep
#define THREAD_SLEEP_UNTIL        globalThreadSleepUntil
#define THREAD_CANCEL             globalCancelThread
#define THREAD_DETACH             globalDetachThread

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
typedef SIZE_T ATOMIC_BOOL;
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

#define MAKE_INT16(a, b) ((INT16) (((UINT8) ((UINT16) (a) &0xff)) | ((UINT16) ((UINT8) ((UINT16) (b) &0xff))) << 8))
#define MAKE_INT32(a, b) ((INT32) (((UINT16) ((UINT32) (a) &0xffff)) | ((UINT32) ((UINT16) ((UINT32) (b) &0xffff))) << 16))
#define MAKE_INT64(a, b) ((INT64) (((UINT32) ((UINT64) (a) &0xffffffff)) | ((UINT64) ((UINT32) ((UINT64) (b) &0xffffffff))) << 32))

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

#ifdef __cplusplus
}
#endif
#endif /* __COMMON_DEFINITIONS__ */
