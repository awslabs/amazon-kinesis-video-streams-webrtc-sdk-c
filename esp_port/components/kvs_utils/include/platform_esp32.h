/**
 * @file platform_esp32.h
 * @brief ESP32 platform-specific definitions for KVS WebRTC
 *
 * This file contains ESP32-specific type definitions, macros, and function declarations
 * required for the KVS WebRTC SDK to work on ESP32 platforms.
 *
 * @copyright Copyright (c) 2024
 */

#ifndef __PLATFORM_ESP32_H__
#define __PLATFORM_ESP32_H__

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <pthread.h>
#include <signal.h>

// ESP-IDF includes
#include "esp_system.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

// FIXME
#define KVS_CA_CERT_PATH "/spiffs/certs/cacert.pem"

// Define gai_strerror for lwip
#ifndef gai_strerror
#define gai_strerror(x) lwip_strerr(x)
#endif

// Platform specific defines
#define MAX_HOSTNAME_LENGTH                    128
#define MAX_PATH_LEN                           200
#define MAX_URI_LENGTH                         4000
#define IPV4_MAX_LENGTH                        16
#define IPV6_MAX_LENGTH                        128


// Type definitions
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

// Content ID - 64 bit uint
typedef UINT64 CID;
typedef CID* PCID;

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

#include <stdint.h>
#include <stddef.h>
#include <unistd.h>

typedef UINT8 BYTE;
typedef BYTE* PBYTE;
typedef INT32* PINT32;
typedef INT64* PINT64;
typedef UINT32* PUINT32;
typedef UINT64* PUINT64;
typedef INT16* PINT16;
typedef INT8* PINT8;
typedef UINT16* PUINT16;
typedef UINT8* PUINT8;
typedef INT32 BOOL;
typedef BOOL* PBOOL;
typedef CHAR* PCHAR;
typedef void VOID;
typedef VOID* PVOID;
typedef size_t SIZE_T;
typedef SIZE_T* PSIZE_T;
typedef ssize_t SSIZE_T;
typedef SSIZE_T* PSSIZE_T;

typedef unsigned long ULONG, *PULONG;

typedef FLOAT* PFLOAT;
typedef DOUBLE* PDOUBLE;
typedef LDOUBLE* PLDOUBLE;
typedef SIZE_T ATOMIC_BOOL;

////////////////////////////////////////////////////
// Project defines
////////////////////////////////////////////////////

#define LIB_EXPORT   __attribute__((visibility("default")))
#define LIB_IMPORT   __attribute__((visibility("default")))
#define LIB_INTERNAL __attribute__((visibility("hidden")))
#define ALIGN_4      __attribute__((aligned(4)))
#define ALIGN_8      __attribute__((aligned(8)))
#define PACKED       __attribute__((__packed__))
#define DISCARDABLE

#define PUBLIC_API  LIB_EXPORT
#define PRIVATE_API LIB_INTERNAL

#include <stdint.h>


#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
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

#endif /* __PLATFORM_ESP32_H__ */
