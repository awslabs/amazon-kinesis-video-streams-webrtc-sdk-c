#include "Include_i.h"

CHAR ALPHA_NUM[] = "0123456789abcdefghijklmnopqrstuvwxyz";

/**
 * Converts an uint64 to string. This implementation is due to the fact that not all platforms support itoa type of functionality
 */
STATUS ulltostr(UINT64 value, PCHAR pStr, UINT32 size, UINT32 base, PUINT32 pSize)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 remainder;
    UINT32 i;
    UINT32 curSize = 0;
    CHAR ch;

    CHK(pStr != NULL, STATUS_NULL_ARG);

    // Should have at least 2 bytes - including null terminator
    CHK(size >= 2, STATUS_BUFFER_TOO_SMALL);

    // Check the base
    CHK(base >= 2 && base <= MAX_STRING_CONVERSION_BASE, STATUS_INVALID_BASE);

    // Quick special case check for 0
    if (value == 0) {
        pStr[0] = '0';
        pStr[1] = '\0';

        if (pSize != NULL) {
            *pSize = 1;
        }

        // Return SUCCESS
        CHK(FALSE, STATUS_SUCCESS);
    }

    while (value != 0) {
        remainder = (UINT32) (value % base);
        value = value / base;

        // Need space for the NULL terminator
        if (curSize >= size - 1) {
            return STATUS_BUFFER_TOO_SMALL;
        }

        pStr[curSize] = ALPHA_NUM[remainder];
        curSize++;
    }

    // Swap the chars
    for (i = 0; i < curSize / 2; i++) {
        ch = pStr[i];
        pStr[i] = pStr[curSize - i - 1];
        pStr[curSize - i - 1] = ch;
    }

    // Set the string terminator
    pStr[curSize] = '\0';

    // Set the last character pointer
    if (pSize != NULL) {
        *pSize = curSize;
    }

CleanUp:

    return retStatus;
}

STATUS ultostr(UINT32 value, PCHAR pStr, UINT32 size, UINT32 base, PUINT32 pSize)
{
    return ulltostr(value, pStr, size, base, pSize);
}

STATUS strtoint(PCHAR pStart, PCHAR pEnd, UINT32 base, PUINT64 pRet, PBOOL pSign)
{
    STATUS retStatus = STATUS_SUCCESS;
    PCHAR pCur = pStart;
    BOOL seenChars = FALSE;
    BOOL positive = TRUE;
    UINT64 result = 0;
    UINT64 digit;
    CHAR curChar;

    // Simple check for NULL
    CHK(pCur != NULL && pRet != NULL && pSign != NULL, STATUS_NULL_ARG);

    // Check for start and end pointers if end is specified
    CHK(pEnd == NULL || pEnd >= pCur, STATUS_INVALID_ARG);

    // Check the base
    CHK(base >= 2 && base <= MAX_STRING_CONVERSION_BASE, STATUS_INVALID_BASE);

    // Check the sign
    switch (*pCur) {
        case '-':
            positive = FALSE;
            // Deliberate fall-through
        case '+':
            pCur++;
        default:
            break;
    }

    while (pCur != pEnd && *pCur != '\0') {
        curChar = *pCur;
        if (curChar >= '0' && curChar <= '9') {
            digit = (UINT64) (curChar - '0');
        } else if (curChar >= 'a' && curChar <= 'z') {
            digit = (UINT64) (curChar - 'a') + 10;
        } else if (curChar >= 'A' && curChar <= 'Z') {
            digit = (UINT64) (curChar - 'A') + 10;
        } else {
            CHK(FALSE, STATUS_INVALID_DIGIT);
        }

        // Set as processed
        seenChars = TRUE;

        // Check against the base
        CHK(digit < base, STATUS_INVALID_BASE);

        // Safe operation which results in
        // result = result * base + digit;
        CHK_STATUS(unsignedSafeMultiplyAdd(result, base, digit, &result));

        pCur++;
    }

    CHK(seenChars, STATUS_EMPTY_STRING);

    if (!positive) {
        result = (UINT64) ((INT64) result * -1);
    }

    *pRet = result;
    *pSign = positive;

CleanUp:

    return retStatus;
}

STATUS strtoi64(PCHAR pStart, PCHAR pEnd, UINT32 base, PINT64 pRet)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT64 result;
    BOOL sign;

    CHK(pRet != NULL, STATUS_NULL_ARG);

    // Convert to UINT64
    CHK_STATUS(strtoint(pStart, pEnd, base, &result, &sign));

    // Check for the overflow
    if (sign) {
        CHK((INT64) result >= 0, STATUS_INT_OVERFLOW);
    } else {
        CHK((INT64) result <= 0, STATUS_INT_OVERFLOW);
    }

    *pRet = (INT64) result;

CleanUp:

    return retStatus;
}

STATUS strtoui64(PCHAR pStart, PCHAR pEnd, UINT32 base, PUINT64 pRet)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT64 result;
    BOOL sign;

    CHK(pRet != NULL, STATUS_NULL_ARG);

    // Convert to UINT64
    CHK_STATUS(strtoint(pStart, pEnd, base, &result, &sign));

    // Check for the overflow
    CHK(sign, STATUS_INVALID_DIGIT);

    *pRet = result;

CleanUp:

    return retStatus;
}

STATUS strtoi32(PCHAR pStart, PCHAR pEnd, UINT32 base, PINT32 pRet)
{
    STATUS retStatus = STATUS_SUCCESS;
    INT64 result;
    CHK(pRet != NULL, STATUS_NULL_ARG);

    // Convert to INT64
    CHK_STATUS(strtoi64(pStart, pEnd, base, &result));

    // Check for the overflow
    CHK(result >= (INT64) MIN_INT32 && result <= (INT64) MAX_INT32, STATUS_INT_OVERFLOW);

    *pRet = (INT32) result;

CleanUp:

    return retStatus;
}

STATUS strtoui32(PCHAR pStart, PCHAR pEnd, UINT32 base, PUINT32 pRet)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT64 result;
    CHK(pRet != NULL, STATUS_NULL_ARG);

    // Convert to UINT64
    CHK_STATUS(strtoui64(pStart, pEnd, base, &result));

    // Check for the overflow
    CHK(result <= (UINT64) MAX_UINT32, STATUS_INT_OVERFLOW);

    *pRet = (UINT32) result;

CleanUp:

    return retStatus;
}

STATUS unsignedSafeMultiplyAdd(UINT64 multiplicand, UINT64 multiplier, UINT64 addend, PUINT64 pResult)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT64 multiplicandHi, multiplicandLo, multiplierLo, multiplierHi, intermediate, result;

    CHK(pResult != NULL, STATUS_NULL_ARG);

    // Set the result to 0 to start with
    *pResult = 0;

    // Quick special case handling
    if (multiplicand == 0 || multiplier == 0) {
        // Early successful return - just return the addend
        *pResult = addend;

        CHK(FALSE, STATUS_SUCCESS);
    }

    // Perform the multiplication first
    // multiplicand * multiplier == (multiplicandHi + multiplicandLo) * (multiplierHi + multiplierLo)
    // which evaluates to
    // multiplicandHi * multiplierHi +
    // multiplicandHi * multiplierLo + multiplicandLo * multiplierHi +
    // multiplicandLo * multiplierLo
    multiplicandLo = multiplicand & 0x00000000ffffffff;
    multiplicandHi = (multiplicand & 0xffffffff00000000) >> (UINT64) 32;
    multiplierLo = multiplier & 0x00000000ffffffff;
    multiplierHi = (multiplier & 0xffffffff00000000) >> (UINT64) 32;

    // If both high parts are non-0 then we do have an overflow
    if (multiplicandHi != 0 && multiplierHi != 0) {
        CHK_STATUS(STATUS_INT_OVERFLOW);
    }

    // Intermediate result shouldn't overflow
    // intermediate = multiplicandHi * multiplierLo + multiplicandLo * multiplierHi;
    // as we have multiplicandHi or multiplierHi being 0
    intermediate = multiplicandHi * multiplierLo + multiplicandLo * multiplierHi;

    // Check if we overflowed the 32 bit
    if (intermediate > 0x00000000ffffffff) {
        CHK_STATUS(STATUS_INT_OVERFLOW);
    }

    // The resulting multiplication is
    // result = intermediate << 32 + multiplicandLo * multiplierLo
    // after which we need to add the addend
    intermediate <<= 32;

    result = intermediate + multiplicandLo * multiplierLo;

    if (result < intermediate) {
        CHK_STATUS(STATUS_INT_OVERFLOW);
    }

    // Finally, add the addend
    intermediate = result;
    result += addend;

    if (result < intermediate) {
        CHK_STATUS(STATUS_INT_OVERFLOW);
    }

    *pResult = result;

CleanUp:

    return retStatus;
}

PCHAR strnchr(PCHAR pStr, UINT32 strLen, CHAR ch)
{
    if (pStr == NULL || strLen == 0) {
        return NULL;
    }

    UINT32 i = 0;

    while (*pStr != ch) {
        if (*pStr++ == '\0' || i++ == strLen - 1) {
            return NULL;
        }
    }

    return pStr;
}

STATUS ltrimstr(PCHAR pStr, UINT32 strLen, PCHAR* ppStart)
{
    STATUS retStatus = STATUS_SUCCESS;
    PCHAR pStart = pStr;
    UINT32 i = 0;

    CHK(pStr != NULL && ppStart != NULL, STATUS_NULL_ARG);

    // Quick check if we need to do anything
    CHK(pStr[0] != '\0', retStatus);

    // See if we need ignore the length
    if (strLen == 0) {
        strLen = MAX_UINT32;
    }

    // Iterate strLen characters (not accounting for the NULL terminator) or until the NULL terminator is reached
    while (i++ < strLen && *pStart != '\0') {
        // Whitespace check
        CHK(IS_WHITE_SPACE(*pStart), retStatus);
        pStart++;
    }

CleanUp:

    if (STATUS_SUCCEEDED(retStatus)) {
        *ppStart = pStart;
    }

    return retStatus;
}

STATUS rtrimstr(PCHAR pStr, UINT32 strLen, PCHAR* ppEnd)
{
    STATUS retStatus = STATUS_SUCCESS;
    PCHAR pEnd = pStr;
    UINT32 i = 0;

    CHK(pStr != NULL && ppEnd != NULL, STATUS_NULL_ARG);

    // Quick check if we need to do anything
    CHK(pStr[0] != '\0', retStatus);

    // See if we need to calculate the length
    if (strLen == 0) {
        strLen = (UINT32) STRLEN(pStr);
    }

    // Start from the end
    pEnd = pStr + strLen;

    // Iterate strLen times from the back (not accounting for the NULL terminator).
    while (i++ < strLen && pStr < pEnd) {
        // Whitespace check
        CHK(IS_WHITE_SPACE(*(pEnd - 1)), retStatus);
        pEnd--;
    }

CleanUp:

    if (STATUS_SUCCEEDED(retStatus)) {
        *ppEnd = pEnd;
    }

    return retStatus;
}

STATUS trimstrall(PCHAR pStr, UINT32 strLen, PCHAR* ppStart, PCHAR* ppEnd)
{
    STATUS retStatus = STATUS_SUCCESS;

    // Left trim first
    CHK_STATUS(ltrimstr(pStr, strLen, ppStart));

    // Calculate the new length
    if (strLen != 0) {
        strLen -= (UINT32) (*ppStart - pStr);
        if (strLen == 0) {
            // This is the case where we have no more string left and we can't call the rtrimstr API
            // as it will interpret the strLen of 0 as a signal to calculate the length
            CHK(ppEnd != NULL, STATUS_NULL_ARG);
            *ppEnd = *ppStart;
            CHK(FALSE, retStatus);
        }
    }

    // Right trim next
    CHK_STATUS(rtrimstr(*ppStart, strLen, ppEnd));

CleanUp:

    return retStatus;
}

STATUS tolowerstr(PCHAR pStr, UINT32 strLen, PCHAR pConverted)
{
    return tolowerupperstr(pStr, strLen, FALSE, pConverted);
}

STATUS toupperstr(PCHAR pStr, UINT32 strLen, PCHAR pConverted)
{
    return tolowerupperstr(pStr, strLen, TRUE, pConverted);
}

STATUS tolowerupperstr(PCHAR pStr, UINT32 strLen, BOOL toUpper, PCHAR pConverted)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 i = 0;
    PCHAR pSrc = pStr, pDst = pConverted;

    CHK(pStr != NULL && pConverted != NULL, STATUS_NULL_ARG);

    // Quick check if we need to do anything
    CHK(pStr[0] != '\0', retStatus);

    // See if we need ignore the length
    if (strLen == 0) {
        strLen = MAX_UINT32;
    }

    // Iterate strLen characters (not accounting for the NULL terminator) or until the NULL terminator is reached
    while (i++ < strLen && *pSrc != '\0') {
        *pDst = (CHAR) (toUpper ? TOUPPER(*pSrc) : TOLOWER(*pSrc));

        pSrc++;
        pDst++;
    }

CleanUp:

    // Null terminate the destination string
    if (pDst != NULL && i < strLen) {
        *pDst = '\0';
    }

    return retStatus;
}

// This is a string search using Rabin–Karp algorithm.
// Running time complexity:
//   * Average: Θ(n + m)
//   * Worst  : Θ((n−m)m)
// Memory complexity: O(1)
PCHAR defaultStrnstr(PCHAR haystack, PCHAR needle, SIZE_T len)
{
    UINT32 prime = 16777619;
    UINT32 windowHash = 0;
    UINT32 needleHash = 0;
    UINT32 i;
    UINT32 square = prime, power = 1;
    UINT32 haystackSize, needleSize;

    if (needle == NULL) {
        return haystack;
    }

    if (haystack == NULL) {
        return NULL;
    }

    haystackSize = (UINT32) STRNLEN(haystack, len);
    needleSize = (UINT32) STRLEN(needle);
    if (needleSize > haystackSize) {
        return NULL;
    }

    for (i = 0; i < needleSize; i++) {
        windowHash = windowHash * prime + (UINT32) haystack[i];
        needleHash = needleHash * prime + (UINT32) needle[i];
    }

    if (windowHash == needleHash && STRNCMP(haystack, needle, needleSize) == 0) {
        return haystack;
    }

    // Precompute the largest power in O(log n).
    //
    // This algorithm takes an advantage for the fact that a number that's multiplied
    // by itself will double the power.
    //
    // For example:
    //    a ^ 4 = (a ^ 2) * (a ^ 2) = a * a * a * a
    //
    //
    // This largest power then can be used to efficiently slide the hash window.
    //
    // For example:
    //    windowString = "abc"
    //    ASCII table:
    //        a = 97
    //        b = 98
    //        c = 99
    //
    //    windowHash = 97 * prime ^ 2 + 98 * prime ^ 1 + 99
    //
    //    After this calculation, power = prime ^ 2
    //
    // The idea is to not repeat recalculating prime ^ 2 everytime we slide
    // the window.
    for (i = needleSize - 1; i > 0; i /= 2) {
        // When it's odd, bring back to even again by storing the extra square
        // in power
        if (i % 2 != 0) {
            power *= square;
        }
        square *= square;
    }

    for (i = needleSize; i < haystackSize;) {
        // slide the window hash, remove oldest char and add a new char
        windowHash = windowHash - (((UINT32) haystack[i - needleSize]) * power);
        windowHash = windowHash * prime + (UINT32) haystack[i];

        i++;
        // make sure that the hash is not collided
        if (windowHash == needleHash && STRNCMP(haystack + i - needleSize, needle, needleSize) == 0) {
            return haystack + i - needleSize;
        }
    }

    return NULL;
}
