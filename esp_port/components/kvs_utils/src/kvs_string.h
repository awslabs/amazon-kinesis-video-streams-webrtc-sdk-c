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
#ifndef __AWS_KVS_WEBRTC_STRING_INCLUDE__
#define __AWS_KVS_WEBRTC_STRING_INCLUDE__

#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>
#include "error.h"
#include "common_defs.h"

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
#define TRIMSTRALL trimstrall
#define LTRIMSTR   ltrimstr
#define RTRIMSTR   rtrimstr
#define STRSTR     strstr
#ifdef strnstr
#define STRNSTR strnstr
#else
#define STRNSTR defaultStrnstr
#endif

//
// CRT functionality
//
#define STRTOUL  strtoul
#define ULLTOSTR ulltostr
#define ULTOSTR  ultostr

//
// String to integer conversion
//
#define STRTOUI32 strtoui32
#define STRTOI32  strtoi32
#define STRTOUI64 strtoui64
#define STRTOI64  strtoi64
//
// Empty string definition
//
#define EMPTY_STRING ((PCHAR) "")

//
// Check if string is empty
//
#define IS_EMPTY_STRING(str) ((str)[0] == '\0')

#define MAX_STRING_CONVERSION_BASE 36
// Check for whitespace
#define IS_WHITE_SPACE(ch) (((ch) == ' ') || ((ch) == '\t') || ((ch) == '\r') || ((ch) == '\n') || ((ch) == '\v') || ((ch) == '\f'))
/**
 * Internal String operations
 */
UINT32 strtoint(PCHAR, PCHAR, UINT32, PUINT64, PBOOL);
/**
 * Integer to string conversion routines
 */
UINT32 ulltostr(UINT64, PCHAR, UINT32, UINT32, PUINT32);
UINT32 ultostr(UINT32, PCHAR, UINT32, UINT32, PUINT32);

/**
 * String to integer conversion routines. NOTE: The base is in [2-36]
 *
 * @param 1 - IN - Input string to process
 * @param 2 - IN/OPT - Pointer to the end of the string. If NULL, the NULL terminator would be used
 * @param 3 - IN - Base of the number (10 - for decimal)
 * @param 4 - OUT - The resulting value
 */
UINT32 strtoui32(PCHAR, PCHAR, UINT32, PUINT32);
UINT32 strtoui64(PCHAR, PCHAR, UINT32, PUINT64);
UINT32 strtoi32(PCHAR, PCHAR, UINT32, PINT32);
UINT32 strtoi64(PCHAR, PCHAR, UINT32, PINT64);

/**
 * Safe variant of strchr
 *
 * @param 1 - IN - Input string to process
 * @param 2 - IN/OPT - String length. 0 if NULL terminated and the length is calculated.
 * @param 3 - IN - The character to look for
 *
 * @return - Pointer to the first occurrence or NULL
 */
PCHAR strnchr(PCHAR, UINT32, CHAR);

/**
 * Safe variant of strstr. This is a default implementation for strnstr when not available.
 *
 * @param 1 - IN - Input string to process
 * @param 3 - IN - The string to look for
 * @param 2 - IN - String length.
 *
 * @return - Pointer to the first occurrence or NULL
 */
PCHAR defaultStrnstr(PCHAR, PCHAR, SIZE_T);

/**
 * Left and right trim of the whitespace
 *
 * @param 1 - IN - Input string to process
 * @param 2 - IN/OPT - String length. 0 if NULL terminated and the length is calculated.
 * @param 3 and 4 - OUT - The pointer to the trimmed start and/or end
 *
 * @return UINT32 of the operation
 */
UINT32 ltrimstr(PCHAR, UINT32, PCHAR*);
UINT32 rtrimstr(PCHAR, UINT32, PCHAR*);
UINT32 trimstrall(PCHAR, UINT32, PCHAR*, PCHAR*);

/**
 * To lower and to upper string conversion
 *
 * @param 1 - IN - Input string to convert
 * @param 2 - IN - String length. 0 if NULL terminated and the length is calculated.
 * @param 3 - OUT - The pointer to the converted string - can be pointing to the same location. Size should be enough
 *
 * @return UINT32 of the operation
 */
UINT32 tolowerstr(PCHAR, UINT32, PCHAR);
UINT32 toupperstr(PCHAR, UINT32, PCHAR);

/**
 * To lower/upper string conversion internal function
 *
 * @param 1 - IN - Input string to convert
 * @param 2 - IN - String length. 0 if NULL terminated and the length is calculated.
 * @param 3 - INT - Whether to upper (TRUE) or to lower (FALSE)
 * @param 4 - OUT - The pointer to the converted string - can be pointing to the same location. Size should be enough
 *
 * @return UINT32 of the operation
 */
UINT32 tolowerupperstr(PCHAR, UINT32, BOOL, PCHAR);

/**
 * Internal safe multiplication
 */
UINT32 unsignedSafeMultiplyAdd(UINT64, UINT64, UINT64, PUINT64);

#ifdef __cplusplus
}
#endif
#endif /* __AWS_KVS_WEBRTC_STRING_INCLUDE__ */
