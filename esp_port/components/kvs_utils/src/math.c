
#include "common_defs.h"
#include "error.h"

STATUS computePower(UINT64 base, UINT64 exponent, PUINT64 result)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT64 res = 1;

    CHK(result != NULL, STATUS_NULL_ARG);

    // base and exponent values checks to prevent overflow
    CHK((base <= 5 && exponent <= 27) || (base <= 100 && exponent <= 9) || (base <= 1000 && exponent <= 6), STATUS_INVALID_ARG);

    // Anything power 0 is 1
    if (exponent == 0) {
        *result = 1;
        return retStatus;
    }

    // Zero power anything except 0 is 0
    if (base == 0) {
        *result = 0;
        return retStatus;
    }

    while (exponent != 0) {
        // if exponent is odd, multiply the result by base
        if (exponent & 1) {
            res *= base;
        }
        // divide exponent by 2
        exponent = exponent >> 1;
        // multiply base by itself
        base = base * base;
    }

    *result = res;

CleanUp:
    return retStatus;
}
