/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#if defined(LINUX_BUILD) || defined(KVS_PLAT_LINUX_UNIX)
// Linux compatibility implementation

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

// Base64 encoding/decoding tables
static const char base64_enc_map[64] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J',
    'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T',
    'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd',
    'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
    'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x',
    'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7',
    '8', '9', '+', '/'
};

static const unsigned char base64_dec_map[128] = {
    127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
    127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
    127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
    127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
    127, 127, 127,  62, 127, 127, 127,  63,  52,  53,
     54,  55,  56,  57,  58,  59,  60,  61, 127, 127,
    127,  64, 127, 127, 127,   0,   1,   2,   3,   4,
      5,   6,   7,   8,   9,  10,  11,  12,  13,  14,
     15,  16,  17,  18,  19,  20,  21,  22,  23,  24,
     25, 127, 127, 127, 127, 127, 127,  26,  27,  28,
     29,  30,  31,  32,  33,  34,  35,  36,  37,  38,
     39,  40,  41,  42,  43,  44,  45,  46,  47,  48,
     49,  50,  51, 127, 127, 127, 127, 127
};

/**
 * @brief Base64 encode a buffer
 *
 * @param dst destination buffer
 * @param dlen size of the destination buffer
 * @param olen pointer to output length variable
 * @param src source buffer
 * @param slen amount of data to be encoded
 * @return 0 on success, non-zero on error
 */
static inline int base64_encode(unsigned char *dst, size_t dlen, size_t *olen,
                       const unsigned char *src, size_t slen)
{
    size_t i, n;
    int C1, C2, C3;
    unsigned char *p;

    if (src == NULL || olen == NULL) {
        return -1;
    }

    *olen = 4 * ((slen + 2) / 3); /* 3-byte blocks to 4-byte */

    if (dlen < *olen) {
        *olen = dlen;
        return -1;
    }

    if (dst == NULL || slen == 0) {
        return 0;
    }

    n = slen / 3;
    n = (slen % 3 != 0) ? n + 1 : n;

    p = dst;
    for (i = 0; i < slen; i += 3) {
        C1 = src[i];
        C2 = ((i + 1) < slen) ? src[i + 1] : 0;
        C3 = ((i + 2) < slen) ? src[i + 2] : 0;

        *p++ = base64_enc_map[(C1 >> 2) & 0x3F];
        *p++ = base64_enc_map[((C1 & 3) << 4) | ((C2 >> 4) & 0x0F)];
        *p++ = ((i + 1) < slen) ? base64_enc_map[((C2 & 0x0F) << 2) | ((C3 >> 6) & 0x03)] : '=';
        *p++ = ((i + 2) < slen) ? base64_enc_map[C3 & 0x3F] : '=';
    }

    *olen = p - dst;
    return 0;
}

/**
 * @brief Base64 decode a buffer
 *
 * @param dst destination buffer
 * @param dlen size of the destination buffer
 * @param olen pointer to output length variable
 * @param src source buffer
 * @param slen amount of data to be decoded
 * @return 0 on success, non-zero on error
 */
static inline int base64_decode(unsigned char *dst, size_t dlen, size_t *olen,
                       const unsigned char *src, size_t slen)
{
    size_t i, n;
    size_t j, x;
    unsigned char *p;

    if (src == NULL || olen == NULL) {
        return -1;
    }

    /* First pass: check for validity and get output length */
    for (i = n = j = 0; i < slen; i++) {
        /* Skip spaces before checking for EOL */
        x = 0;
        while (i < slen && src[i] == ' ') {
            ++i;
            ++x;
        }

        /* Spaces at end of buffer are OK */
        if (i == slen)
            break;

        if ((slen - i) >= 2 && src[i] == '\r' && src[i + 1] == '\n')
            continue;

        if (src[i] == '\n')
            continue;

        /* Space inside a line is an error */
        if (x != 0)
            return -1;

        if (src[i] == '=' && ++j > 2)
            return -1;

        if (src[i] > 127 || base64_dec_map[src[i]] == 127)
            return -1;

        if (base64_dec_map[src[i]] < 64 && j != 0)
            return -1;

        n++;
    }

    if (n == 0) {
        *olen = 0;
        return 0;
    }

    /* The following expression is to calculate the following formula without
     * risk of integer overflow in n:
     *     n = ( ( n * 6 ) + 7 ) >> 3;
     */
    n = (6 * (n >> 3)) + ((6 * (n & 0x7) + 7) >> 3);
    n -= j;

    if (dst == NULL || dlen < n) {
        *olen = n;
        return -1;
    }

    /* Second pass: decode */
    for (j = 3, n = x = 0, p = dst; i > 0; i--, src++) {
        if (*src == '\r' || *src == '\n' || *src == ' ')
            continue;

        j -= (*src == '=');
        x = (x << 6) | (base64_dec_map[*src] & 0x3F);

        if (++n == 4) {
            n = 0;
            if (j > 0) *p++ = (unsigned char)(x >> 16);
            if (j > 1) *p++ = (unsigned char)(x >> 8);
            if (j > 2) *p++ = (unsigned char)(x);
        }
    }

    *olen = p - dst;
    return 0;
}

// Compatibility layer for mbedtls-style interface
#define mbedtls_base64_encode base64_encode
#define mbedtls_base64_decode base64_decode

#else
// Use actual mbedtls for ESP builds
#include "mbedtls/base64.h"
#endif
