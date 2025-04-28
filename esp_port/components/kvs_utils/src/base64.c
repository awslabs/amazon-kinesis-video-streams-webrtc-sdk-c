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

// #include "base64.h"
#include "common_defs.h"
#include "error.h"

/**
 * Base64 encoding alphabet
 */
CHAR BASE64_ENCODE_ALPHA[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/**
 * Base64 decoding alphabet - an array of 256 values corresponding to the encoded base64 indexes
 * maps A -> 0, B -> 1, etc..
 */
BYTE BASE64_DECODE_ALPHA[256] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  // 10
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  // 20
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  // 30
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  // 40
    0,  0,  0,  62, 0,  0,  0,  63, 52, 53, // 50
    54, 55, 56, 57, 58, 59, 60, 61, 0,  0,  // 60
    0,  0,  0,  0,  0,  0,  1,  2,  3,  4,  // 70
    5,  6,  7,  8,  9,  10, 11, 12, 13, 14, // 80
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, // 90
    25, 0,  0,  0,  0,  0,  0,  26, 27, 28, // 100
    29, 30, 31, 32, 33, 34, 35, 36, 37, 38, // 110
    39, 40, 41, 42, 43, 44, 45, 46, 47, 48, // 120
    49, 50, 51, 0,  0,  0,  0,  0,  0,  0,  // 130
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  // 140
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  // 150
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  // 160
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  // 170
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  // 180
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  // 190
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  // 200
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  // 210
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  // 220
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  // 230
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  // 240
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  // 250
    0,  0,  0,  0,  0,  0,
};

/**
 * Padding values for mod3 indicating how many '=' to append
 */
BYTE BASE64_ENCODE_PADDING[3] = {0, 2, 1};

/**
 * Padding values for mod4 indicating how many '=' has been padded. NOTE: value for 1 is invalid = 0xff
 */
BYTE BASE64_DECODE_PADDING[4] = {0, 0xff, 2, 1};

/**
 * Base64 encodes the data. Calling the function with NULL pOutputData will calculate the output buffer size only
 * The function will NULL-terminate the returned encoded string and count the size including the terminator.
 */
STATUS base64Encode(PVOID pInputData, UINT32 inputLength, PCHAR pOutputData, PUINT32 pOutputLength)
{
    UINT32 mod3;
    UINT32 outputLength;
    UINT32 padding;
    UINT32 i;
    BYTE b0, b1, b2;
    PBYTE pInput = (PBYTE) pInputData;
    PCHAR pOutput = pOutputData;

    if (pInputData == NULL || pOutputLength == NULL) {
        return STATUS_NULL_ARG;
    }

    if (inputLength == 0) {
        return STATUS_INVALID_ARG_LEN;
    }

    // Calculate the needed buffer size
    mod3 = inputLength % 3;
    padding = BASE64_ENCODE_PADDING[mod3];

    // Include the NULL terminator in the length calculation
    outputLength = 4 * (inputLength + padding) / 3 + 1;

    if (pOutputData == NULL) {
        // Early return - just needed the size
        *pOutputLength = outputLength;
        return STATUS_SUCCESS;
    }

    // Check against the buffer size that's provided
    if (*pOutputLength < outputLength) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    // Need to have at least a triade to process in the loop
    if (inputLength >= 3) {
        for (i = 0; i <= inputLength - 3; i += 3) {
            b0 = *pInput++;
            b1 = *pInput++;
            b2 = *pInput++;

            *pOutput++ = BASE64_ENCODE_ALPHA[b0 >> 2];
            *pOutput++ = BASE64_ENCODE_ALPHA[((0x03 & b0) << 4) + (b1 >> 4)];
            *pOutput++ = BASE64_ENCODE_ALPHA[((0x0f & b1) << 2) + (b2 >> 6)];
            *pOutput++ = BASE64_ENCODE_ALPHA[0x3f & b2];
        }
    }

    // Process the padding
    if (padding == 1) {
        *pOutput++ = BASE64_ENCODE_ALPHA[*pInput >> 2];
        *pOutput++ = BASE64_ENCODE_ALPHA[((0x03 & *pInput) << 4) + (*(pInput + 1) >> 4)];
        *pOutput++ = BASE64_ENCODE_ALPHA[(0x0f & *(pInput + 1)) << 2];
        *pOutput++ = '=';
    } else if (padding == 2) {
        *pOutput++ = BASE64_ENCODE_ALPHA[*pInput >> 2];
        *pOutput++ = BASE64_ENCODE_ALPHA[(0x03 & *pInput) << 4];
        *pOutput++ = '=';
        *pOutput++ = '=';
    }

    // Add the null terminator
    *pOutput = '\0';

    // Set the correct size
    *pOutputLength = outputLength;

    return STATUS_SUCCESS;
}

/**
 * Decodes Base64 data. Calling the function with NULL output buffer will result in just the buffer size calculation.
 * NOTE: pInputData should be NULL terminated
 * IMPLEMENTATION: We will ignore the '=' padding by removing those and will calculate the passing based on the string length
 */
STATUS base64Decode(PCHAR pInputData, UINT32 inputLength, PBYTE pOutputData, PUINT32 pOutputLength)
{
    UINT32 outputLength;
    UINT32 i;
    BYTE b0, b1, b2, b3;
    UINT32 padding = 0;
    PBYTE pInput = (PBYTE) pInputData;
    PBYTE pOutput = pOutputData;

    if (pInput == NULL || pOutputLength == NULL) {
        return STATUS_NULL_ARG;
    }

    inputLength = (inputLength != 0) ? inputLength : (UINT32) STRLEN(pInputData);
    // Check the size - should have more than 2 chars
    if (inputLength < 2) {
        return STATUS_INVALID_ARG_LEN;
    }

    // Check the padding
    if (pInput[inputLength - 1] == '=') {
        inputLength--;
    }

    // Check for the second padding
    if (pInput[inputLength - 1] == '=') {
        inputLength--;
    }

    // Calculate the padding
    padding = BASE64_DECODE_PADDING[inputLength % 4];

    // Mod4 can't be 1 which means the padding can never be 0xff
    if (padding == 0xff) {
        return STATUS_INVALID_BASE64_ENCODE;
    }

    // Calculate the output length
    outputLength = 3 * inputLength / 4;

    // Check if we only need to calculate the buffer size
    if (pOutputData == NULL) {
        // Early return - just calculate the size
        *pOutputLength = outputLength;
        return STATUS_SUCCESS;
    }

    // Check against the buffer size that's been supplied
    if (*pOutputLength < outputLength) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    // Proceed with the decoding - we should have at least a quad to process in the loop
    if (inputLength >= 4) {
        for (i = 0; i <= inputLength - 4; i += 4) {
            b0 = BASE64_DECODE_ALPHA[*pInput++];
            b1 = BASE64_DECODE_ALPHA[*pInput++];
            b2 = BASE64_DECODE_ALPHA[*pInput++];
            b3 = BASE64_DECODE_ALPHA[*pInput++];

            *pOutput++ = (b0 << 2) | (b1 >> 4);
            *pOutput++ = (b1 << 4) | (b2 >> 2);
            *pOutput++ = (b2 << 6) | b3;
        }
    }

    // Process the padding
    if (padding == 1) {
        b0 = BASE64_DECODE_ALPHA[*pInput++];
        b1 = BASE64_DECODE_ALPHA[*pInput++];
        b2 = BASE64_DECODE_ALPHA[*pInput++];

        *pOutput++ = (b0 << 2) | (b1 >> 4);
        *pOutput++ = (b1 << 4) | (b2 >> 2);
    } else if (padding == 2) {
        b0 = BASE64_DECODE_ALPHA[*pInput++];
        b1 = BASE64_DECODE_ALPHA[*pInput++];

        *pOutput++ = (b0 << 2) | (b1 >> 4);
    }

    // Set the correct size
    *pOutputLength = outputLength;

    return STATUS_SUCCESS;
}
