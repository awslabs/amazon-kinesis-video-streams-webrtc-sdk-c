/*
 * Copyright 2021 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *  http://aws.amazon.com/apache2.0
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */
#ifndef __AWS_KVS_WEBRTC_ENDIANNESS_INCLUDE__
#define __AWS_KVS_WEBRTC_ENDIANNESS_INCLUDE__

#ifdef __cplusplus
extern "C" {
#endif

#include "platform_esp32.h"

//
// Macros to swap endinanness
//
// #define LOW_BYTE(x)   ((BYTE)(x))
// #define HIGH_BYTE(x)  ((BYTE)(((INT16)(x) >> 8) & 0xFF))
// #define LOW_INT16(x)  ((INT16)(x))
// #define HIGH_INT16(x) ((INT16)(((INT32)(x) >> 16) & 0xFFFF))
// #define LOW_INT32(x)  ((INT32)(x))
// #define HIGH_INT32(x) ((INT32)(((INT64)(x) >> 32) & 0xFFFFFFFF))

// #define MAKE_INT16(a, b) ((INT16)(((UINT8)((UINT16)(a) &0xff)) | ((UINT16)((UINT8)((UINT16)(b) &0xff))) << 8))
// #define MAKE_INT32(a, b) ((INT32)(((UINT16)((UINT32)(a) &0xffff)) | ((UINT32)((UINT16)((UINT32)(b) &0xffff))) << 16))
// #define MAKE_INT64(a, b) ((INT64)(((UINT32)((UINT64)(a) &0xffffffff)) | ((UINT64)((UINT32)((UINT64)(b) &0xffffffff))) << 32))

#define SWAP_INT16(x) MAKE_INT16(HIGH_BYTE(x), LOW_BYTE(x))

#define SWAP_INT32(x) MAKE_INT32(SWAP_INT16(HIGH_INT16(x)), SWAP_INT16(LOW_INT16(x)))

#define SWAP_INT64(x) MAKE_INT64(SWAP_INT32(HIGH_INT32(x)), SWAP_INT32(LOW_INT32(x)))

#if 0
/**
 * Endianness functionality
 */
INLINE INT16 getInt16Swap(INT16);
INLINE INT16 getInt16NoSwap(INT16);
INLINE INT32 getInt32Swap(INT32);
INLINE INT32 getInt32NoSwap(INT32);
INLINE INT64 getInt64Swap(INT64);
INLINE INT64 getInt64NoSwap(INT64);

INLINE VOID putInt16Swap(PINT16, INT16);
INLINE VOID putInt16NoSwap(PINT16, INT16);
INLINE VOID putInt32Swap(PINT32, INT32);
INLINE VOID putInt32NoSwap(PINT32, INT32);
INLINE VOID putInt64Swap(PINT64, INT64);
INLINE VOID putInt64NoSwap(PINT64, INT64);

/**
 * Unaligned access functionality
 */
INLINE INT16 getUnalignedInt16Be(PVOID);
INLINE INT16 getUnalignedInt16Le(PVOID);
INLINE INT32 getUnalignedInt32Be(PVOID);
INLINE INT32 getUnalignedInt32Le(PVOID);
INLINE INT64 getUnalignedInt64Be(PVOID);
INLINE INT64 getUnalignedInt64Le(PVOID);

INLINE VOID putUnalignedInt16Be(PVOID, INT16);
INLINE VOID putUnalignedInt16Le(PVOID, INT16);
INLINE VOID putUnalignedInt32Be(PVOID, INT32);
INLINE VOID putUnalignedInt32Le(PVOID, INT32);
INLINE VOID putUnalignedInt64Be(PVOID, INT64);
INLINE VOID putUnalignedInt64Le(PVOID, INT64);
#endif

////////////////////////////////////////////////////
// Endianness functionality
////////////////////////////////////////////////////
typedef INT16 (*getInt16Func)(INT16);
typedef INT32 (*getInt32Func)(INT32);
typedef INT64 (*getInt64Func)(INT64);
typedef VOID (*putInt16Func)(PINT16, INT16);
typedef VOID (*putInt32Func)(PINT32, INT32);
typedef VOID (*putInt64Func)(PINT64, INT64);

extern getInt16Func getInt16;
extern getInt32Func getInt32;
extern getInt64Func getInt64;
extern putInt16Func putInt16;
extern putInt32Func putInt32;
extern putInt64Func putInt64;

BOOL isBigEndian();
VOID initializeEndianness();

////////////////////////////////////////////////////
// Unaligned access functionality
////////////////////////////////////////////////////
typedef INT16 (*getUnalignedInt16Func)(PVOID);
typedef INT32 (*getUnalignedInt32Func)(PVOID);
typedef INT64 (*getUnalignedInt64Func)(PVOID);

typedef VOID (*putUnalignedInt16Func)(PVOID, INT16);
typedef VOID (*putUnalignedInt32Func)(PVOID, INT32);
typedef VOID (*putUnalignedInt64Func)(PVOID, INT64);

extern getUnalignedInt16Func getUnalignedInt16;
extern getUnalignedInt32Func getUnalignedInt32;
extern getUnalignedInt64Func getUnalignedInt64;

extern putUnalignedInt16Func putUnalignedInt16;
extern putUnalignedInt32Func putUnalignedInt32;
extern putUnalignedInt64Func putUnalignedInt64;

// These are the specific Big-endian variants needed for most of the formats
extern getUnalignedInt16Func getUnalignedInt16BigEndian;
extern getUnalignedInt32Func getUnalignedInt32BigEndian;
extern getUnalignedInt64Func getUnalignedInt64BigEndian;

extern putUnalignedInt16Func putUnalignedInt16BigEndian;
extern putUnalignedInt32Func putUnalignedInt32BigEndian;
extern putUnalignedInt64Func putUnalignedInt64BigEndian;

extern getUnalignedInt16Func getUnalignedInt16LittleEndian;
extern getUnalignedInt32Func getUnalignedInt32LittleEndian;
extern getUnalignedInt64Func getUnalignedInt64LittleEndian;

extern putUnalignedInt16Func putUnalignedInt16LittleEndian;
extern putUnalignedInt32Func putUnalignedInt32LittleEndian;
extern putUnalignedInt64Func putUnalignedInt64LittleEndian;

// Helper macro for unaligned
#define GET_UNALIGNED(ptr)                                                                                                                           \
    SIZEOF(*(ptr)) == 1       ? *(ptr)                                                                                                               \
        : SIZEOF(*(ptr)) == 2 ? getUnalignedInt16(ptr)                                                                                               \
        : SIZEOF(*(ptr)) == 4 ? getUnalignedInt32(ptr)                                                                                               \
        : SIZEOF(*(ptr)) == 8 ? getUnalignedInt64(ptr)                                                                                               \
                              : 0

#define GET_UNALIGNED_BIG_ENDIAN(ptr)                                                                                                                \
    SIZEOF(*(ptr)) == 1       ? *(ptr)                                                                                                               \
        : SIZEOF(*(ptr)) == 2 ? getUnalignedInt16BigEndian(ptr)                                                                                      \
        : SIZEOF(*(ptr)) == 4 ? getUnalignedInt32BigEndian(ptr)                                                                                      \
        : SIZEOF(*(ptr)) == 8 ? getUnalignedInt64BigEndian(ptr)                                                                                      \
                              : 0

#define PUT_UNALIGNED(ptr, val)                                                                                                                      \
    do {                                                                                                                                             \
        PVOID __pVoid = (ptr);                                                                                                                       \
        switch (SIZEOF(*(ptr))) {                                                                                                                    \
            case 1:                                                                                                                                  \
                *(PINT8) __pVoid = (INT8) (val);                                                                                                     \
                break;                                                                                                                               \
            case 2:                                                                                                                                  \
                putUnalignedInt16(__pVoid, (INT16) (val));                                                                                           \
                break;                                                                                                                               \
            case 4:                                                                                                                                  \
                putUnalignedInt32(__pVoid, (INT32) (val));                                                                                           \
                break;                                                                                                                               \
            case 8:                                                                                                                                  \
                putUnalignedInt64(__pVoid, (INT64) (val));                                                                                           \
                break;                                                                                                                               \
            default:                                                                                                                                 \
                CHECK_EXT(FALSE, "Bad alignment size.");                                                                                             \
                break;                                                                                                                               \
        }                                                                                                                                            \
    } while (0);

#define PUT_UNALIGNED_BIG_ENDIAN(ptr, val)                                                                                                           \
    do {                                                                                                                                             \
        PVOID __pVoid = (ptr);                                                                                                                       \
        switch (SIZEOF(*(ptr))) {                                                                                                                    \
            case 1:                                                                                                                                  \
                *(PINT8) __pVoid = (INT8) (val);                                                                                                     \
                break;                                                                                                                               \
            case 2:                                                                                                                                  \
                putUnalignedInt16BigEndian(__pVoid, (INT16) (val));                                                                                  \
                break;                                                                                                                               \
            case 4:                                                                                                                                  \
                putUnalignedInt32BigEndian(__pVoid, (INT32) (val));                                                                                  \
                break;                                                                                                                               \
            case 8:                                                                                                                                  \
                putUnalignedInt64BigEndian(__pVoid, (INT64) (val));                                                                                  \
                break;                                                                                                                               \
            default:                                                                                                                                 \
                CHECK_EXT(FALSE, "Bad alignment size.");                                                                                             \
                break;                                                                                                                               \
        }                                                                                                                                            \
    } while (0);

#ifdef __cplusplus
}
#endif
#endif /* __AWS_KVS_WEBRTC_ENDIANNESS_INCLUDE__ */
