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

#include "common_defs.h"
#include "error.h"
#include "endianness.h"
//
// Global var defining runtime endianness
//
BOOL g_BigEndian;

//
// Functions dealing with endianness and swapping
//
INT16 getInt16Swap(INT16 x)
{
    return SWAP_INT16(x);
}

INT16 getInt16NoSwap(INT16 x)
{
    return x;
}

INT32 getInt32Swap(INT32 x)
{
    return SWAP_INT32(x);
}

INT32 getInt32NoSwap(INT32 x)
{
    return x;
}

INT64 getInt64Swap(INT64 x)
{
    return SWAP_INT64(x);
}

INT64 getInt64NoSwap(INT64 x)
{
    return x;
}

VOID putInt16Swap(PINT16 px, INT16 x)
{
    *px = SWAP_INT16(x);
}

VOID putInt16NoSwap(PINT16 px, INT16 x)
{
    *px = x;
}

VOID putInt32Swap(PINT32 px, INT32 x)
{
    *px = SWAP_INT32(x);
}

VOID putInt32NoSwap(PINT32 px, INT32 x)
{
    *px = x;
}

VOID putInt64Swap(PINT64 px, INT64 x)
{
    *px = SWAP_INT64(x);
}

VOID putInt64NoSwap(PINT64 px, INT64 x)
{
    *px = x;
}

//
// Default initialization of the global functions
//
getInt16Func getInt16 = getInt16NoSwap;
getInt32Func getInt32 = getInt32NoSwap;
getInt64Func getInt64 = getInt64NoSwap;

putInt16Func putInt16 = putInt16NoSwap;
putInt32Func putInt32 = putInt32NoSwap;
putInt64Func putInt64 = putInt64NoSwap;

//
// Functions dealing with aligned access
//
INT16 getUnalignedInt16Le(PVOID pVal)
{
    PBYTE pSrc = (PBYTE) pVal;
    return MAKE_INT16(*pSrc, *(pSrc + 1));
}

INT16 getUnalignedInt16Be(PVOID pVal)
{
    PBYTE pSrc = (PBYTE) pVal;
    return MAKE_INT16(*(pSrc + 1), *pSrc);
}

INT32 getUnalignedInt32Le(PVOID pVal)
{
    PBYTE pSrc = (PBYTE) pVal;
    return MAKE_INT32(MAKE_INT16(*pSrc, *(pSrc + 1)), MAKE_INT16(*(pSrc + 2), *(pSrc + 3)));
}

INT32 getUnalignedInt32Be(PVOID pVal)
{
    PBYTE pSrc = (PBYTE) pVal;
    return MAKE_INT32(MAKE_INT16(*(pSrc + 3), *(pSrc + 2)), MAKE_INT16(*(pSrc + 1), *pSrc));
}

INT64 getUnalignedInt64Le(PVOID pVal)
{
    PBYTE pSrc = (PBYTE) pVal;
    return MAKE_INT64(MAKE_INT32(MAKE_INT16(*pSrc, *(pSrc + 1)), MAKE_INT16(*(pSrc + 2), *(pSrc + 3))),
                      MAKE_INT32(MAKE_INT16(*(pSrc + 4), *(pSrc + 5)), MAKE_INT16(*(pSrc + 6), *(pSrc + 7))));
}

INT64 getUnalignedInt64Be(PVOID pVal)
{
    PBYTE pSrc = (PBYTE) pVal;
    return MAKE_INT64(MAKE_INT32(MAKE_INT16(*(pSrc + 7), *(pSrc + 6)), MAKE_INT16(*(pSrc + 5), *(pSrc + 4))),
                      MAKE_INT32(MAKE_INT16(*(pSrc + 3), *(pSrc + 2)), MAKE_INT16(*(pSrc + 1), *pSrc)));
}

VOID putUnalignedInt16Le(PVOID pVal, INT16 val)
{
    PBYTE pDst = (PBYTE) pVal;
    pDst[0] = (INT8) val;
    pDst[1] = (INT8)(val >> 8);
}

VOID putUnalignedInt16Be(PVOID pVal, INT16 val)
{
    PBYTE pDst = (PBYTE) pVal;
    pDst[1] = (INT8) val;
    pDst[0] = (INT8)(val >> 8);
}

VOID putUnalignedInt32Le(PVOID pVal, INT32 val)
{
    PBYTE pDst = (PBYTE) pVal;
    pDst[0] = (INT8) val;
    pDst[1] = (INT8)(val >> 8);
    pDst[2] = (INT8)(val >> 16);
    pDst[3] = (INT8)(val >> 24);
}

VOID putUnalignedInt32Be(PVOID pVal, INT32 val)
{
    PBYTE pDst = (PBYTE) pVal;
    pDst[3] = (INT8) val;
    pDst[2] = (INT8)(val >> 8);
    pDst[1] = (INT8)(val >> 16);
    pDst[0] = (INT8)(val >> 24);
}

VOID putUnalignedInt64Le(PVOID pVal, INT64 val)
{
    PBYTE pDst = (PBYTE) pVal;
    pDst[0] = (INT8) val;
    pDst[1] = (INT8)(val >> 8);
    pDst[2] = (INT8)(val >> 16);
    pDst[3] = (INT8)(val >> 24);
    pDst[4] = (INT8)(val >> 32);
    pDst[5] = (INT8)(val >> 40);
    pDst[6] = (INT8)(val >> 48);
    pDst[7] = (INT8)(val >> 56);
}

VOID putUnalignedInt64Be(PVOID pVal, INT64 val)
{
    PBYTE pDst = (PBYTE) pVal;
    pDst[7] = (INT8) val;
    pDst[6] = (INT8)(val >> 8);
    pDst[5] = (INT8)(val >> 16);
    pDst[4] = (INT8)(val >> 24);
    pDst[3] = (INT8)(val >> 32);
    pDst[2] = (INT8)(val >> 40);
    pDst[1] = (INT8)(val >> 48);
    pDst[0] = (INT8)(val >> 56);
}

//
// Default initialization of the global functions
//
getUnalignedInt16Func getUnalignedInt16 = getUnalignedInt16Be;
getUnalignedInt32Func getUnalignedInt32 = getUnalignedInt32Be;
getUnalignedInt64Func getUnalignedInt64 = getUnalignedInt64Be;

putUnalignedInt16Func putUnalignedInt16 = putUnalignedInt16Be;
putUnalignedInt32Func putUnalignedInt32 = putUnalignedInt32Be;
putUnalignedInt64Func putUnalignedInt64 = putUnalignedInt64Be;

// The Big-endian variants
getUnalignedInt16Func getUnalignedInt16BigEndian = getUnalignedInt16Be;
getUnalignedInt32Func getUnalignedInt32BigEndian = getUnalignedInt32Be;
getUnalignedInt64Func getUnalignedInt64BigEndian = getUnalignedInt64Be;

putUnalignedInt16Func putUnalignedInt16BigEndian = putUnalignedInt16Be;
putUnalignedInt32Func putUnalignedInt32BigEndian = putUnalignedInt32Be;
putUnalignedInt64Func putUnalignedInt64BigEndian = putUnalignedInt64Be;

getUnalignedInt16Func getUnalignedInt16LittleEndian = getUnalignedInt16Le;
getUnalignedInt32Func getUnalignedInt32LittleEndian = getUnalignedInt32Le;
getUnalignedInt64Func getUnalignedInt64LittleEndian = getUnalignedInt64Le;

putUnalignedInt16Func putUnalignedInt16LittleEndian = putUnalignedInt16Le;
putUnalignedInt32Func putUnalignedInt32LittleEndian = putUnalignedInt32Le;
putUnalignedInt64Func putUnalignedInt64LittleEndian = putUnalignedInt64Le;

//////////////////////////////////////////////////////////
// Public functions
//////////////////////////////////////////////////////////

//
// Checking run-time endianness.
// Other methods checking for byte placement might fail due to compiler optimization
//
BOOL isBigEndian()
{
    union {
        BYTE c[4];
        INT32 i;
    } u;

    u.i = 0x01020304;

    return (0x01 == u.c[0]);
}

VOID initializeEndianness()
{
    if (isBigEndian()) {
        // Big-endian
        g_BigEndian = TRUE;

        getInt16 = getInt16NoSwap;
        getInt32 = getInt32NoSwap;
        getInt64 = getInt64NoSwap;

        putInt16 = putInt16NoSwap;
        putInt32 = putInt32NoSwap;
        putInt64 = putInt64NoSwap;

        // Set the unaligned access functions
        getUnalignedInt16 = getUnalignedInt16Be;
        getUnalignedInt32 = getUnalignedInt32Be;
        getUnalignedInt64 = getUnalignedInt64Be;

        putUnalignedInt16 = putUnalignedInt16Be;
        putUnalignedInt32 = putUnalignedInt32Be;
        putUnalignedInt64 = putUnalignedInt64Be;
    } else {
        // Little-endian
        g_BigEndian = FALSE;

        getInt16 = getInt16Swap;
        getInt32 = getInt32Swap;
        getInt64 = getInt64Swap;

        putInt16 = putInt16Swap;
        putInt32 = putInt32Swap;
        putInt64 = putInt64Swap;

        // Set the unaligned access functions
        getUnalignedInt16 = getUnalignedInt16Le;
        getUnalignedInt32 = getUnalignedInt32Le;
        getUnalignedInt64 = getUnalignedInt64Le;

        putUnalignedInt16 = putUnalignedInt16Le;
        putUnalignedInt32 = putUnalignedInt32Le;
        putUnalignedInt64 = putUnalignedInt64Le;
    }
}
