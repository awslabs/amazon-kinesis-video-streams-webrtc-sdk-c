#include "Include_i.h"

/**
 * Hex encoding upper-case alphabet
 */
CHAR HEX_ENCODE_ALPHA_UPPER[] = "0123456789ABCDEF";

/**
 * Hex encoding lower-case alphabet
 */
CHAR HEX_ENCODE_ALPHA_LOWER[] = "0123456789abcdef";

/**
 * Hex decoding alphabet - an array of 256 values corresponding to the encoded hexindexes
 * maps 0 -> 0x00, 1 -> 0x01, ..., A -> 0x0a, B -> 0xb, ..., a -> 0xa, b -> 0xb, ...
 */
BYTE HEX_DECODE_ALPHA[256] = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // 10
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // 20
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // 30
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // 40
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x01, // 50
    0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0xff, 0xff, // 60
    0xff, 0xff, 0xff, 0xff, 0xff, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, // 70
    0x0f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // 80
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // 90
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x0a, 0x0b, 0x0c, // 100
    0x0d, 0x0e, 0x0f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // 110
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // 120
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // 130
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // 140
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // 150
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // 160
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // 170
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // 180
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // 190
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // 200
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // 210
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // 220
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // 230
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // 240
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // 250
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
};

/**
 * Hex encodes the data. Calling the function with NULL pOutputData will calculate the output buffer size only
 * The function will NULL-terminate the returned encoded string and count the size including the terminator.
 */
PUBLIC_API STATUS hexEncode(PVOID pInputData, UINT32 inputLength, PCHAR pOutputData, PUINT32 pOutputLength)
{
    return hexEncodeCase(pInputData, inputLength, pOutputData, pOutputLength, TRUE);
}

/**
 * Hex encodes the data. Calling the function with NULL pOutputData will calculate the output buffer size only
 * The function will NULL-terminate the returned encoded string and count the size including the terminator.
 *
 * This API takes in a boolean indicating whether the upper case or lower case to be generated
 */
PUBLIC_API STATUS hexEncodeCase(PVOID pInputData, UINT32 inputLength, PCHAR pOutputData, PUINT32 pOutputLength, BOOL upperCase)
{
    UINT32 outputLength;
    UINT32 i;
    BYTE input;
    PBYTE pInput = (PBYTE) pInputData;
    PCHAR pOutput = pOutputData;
    PCHAR pAlpha = upperCase ? HEX_ENCODE_ALPHA_UPPER : HEX_ENCODE_ALPHA_LOWER;

    if (pInputData == NULL || pOutputLength == NULL) {
        return STATUS_NULL_ARG;
    }

    if (inputLength == 0) {
        return STATUS_INVALID_ARG_LEN;
    }

    // Calculate the needed buffer size
    // Include the NULL terminator in the length calculation
    outputLength = 2 * inputLength + 1;

    if (pOutputData == NULL) {
        // Early return - just needed the size
        *pOutputLength = outputLength;
        return STATUS_SUCCESS;
    }

    // Check against the buffer size that's provided
    if (*pOutputLength < outputLength) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    for (i = 0; i < inputLength; i++) {
        input = *pInput++;
        *pOutput++ = pAlpha[input >> 4];
        *pOutput++ = pAlpha[input & 0x0f];
    }

    // Add the null terminator
    *pOutput = '\0';

    // Set the correct size
    *pOutputLength = outputLength;

    return STATUS_SUCCESS;
}

/**
 * Decodes hex data. Calling the function with NULL output buffer will result in just the buffer size calculation.
 * NOTE: pInputData should be NULL terminated
 * IMPLEMENTATION: We will ignore the last character if the number is odd. Will return an error on non-hex chars.
 * Will process the upper and lower case.
 */
PUBLIC_API STATUS hexDecode(PCHAR pInputData, UINT32 inputLength, PBYTE pOutputData, PUINT32 pOutputLength)
{
    UINT32 outputLength;
    UINT32 i;
    UINT8 hiNibble, loNibble;
    PBYTE pInput = (PBYTE) pInputData;
    PBYTE pOutput = pOutputData;

    if (pInput == NULL || pOutputLength == NULL) {
        return STATUS_NULL_ARG;
    }

    // Calculate the length if none is specified
    if (inputLength == 0) {
        inputLength = (UINT32) STRLEN(pInputData);
    }

    // Check the size - should have more than 1 chars
    if (inputLength <= 1) {
        return STATUS_INVALID_ARG_LEN;
    }

    // Calculate the output length - input length divided by 2
    outputLength = inputLength >> 1;

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

    // Ensure inputLength is even
    inputLength = inputLength & 0xfffffffe;

    // Proceed with the decoding - we should have at least a quad to process in the loop
    for (i = 0; i < inputLength; i += 2) {
        hiNibble = HEX_DECODE_ALPHA[*pInput++];
        loNibble = HEX_DECODE_ALPHA[*pInput++];
        if (hiNibble > 0x0f || loNibble > 0x0f) {
            return STATUS_INVALID_ARG;
        }

        *pOutput++ = (hiNibble << 4) | loNibble;
    }

    // Set the correct size
    *pOutputLength = outputLength;

    return STATUS_SUCCESS;
}
