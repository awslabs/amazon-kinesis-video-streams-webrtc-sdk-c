// Test-only RNG for the KVS_TEST_MBEDTLS4_NO_ENTROPY build: MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG makes the
// application supply PSA's randomness, which a device would take from its hardware RNG.
#if defined(KVS_TEST_MBEDTLS4_NO_ENTROPY)
#include <psa/crypto.h>
#include <sys/random.h>
#include <errno.h>

extern "C" psa_status_t mbedtls_psa_external_get_random(mbedtls_psa_external_random_context_t* context, uint8_t* output, size_t outputSize,
                                                        size_t* pOutputLength)
{
    size_t done = 0;
    ssize_t n;

    (void) context;
    while (done < outputSize) {
        n = getrandom(output + done, outputSize - done, 0);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return PSA_ERROR_INSUFFICIENT_ENTROPY;
        }
        done += (size_t) n;
    }
    *pOutputLength = outputSize;
    return PSA_SUCCESS;
}
#endif
