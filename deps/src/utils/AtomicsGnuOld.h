#ifndef __UTILS_ATOMICS_GNU_OLD__
#define __UTILS_ATOMICS_GNU_OLD__

#ifdef __cplusplus
extern "C" {
#endif

#include "Include_i.h"

#if defined(__GNUC__)
#if (__GNUC__ < 4)
#error GCC versions before 4.1.2 are not supported
#elif (defined(__arm__) || defined(__ia64__)) && (__GNUC__ == 4 && __GNUC_MINOR__ < 4)
/* See https://gcc.gnu.org/bugzilla/show_bug.cgi?id=36793 Itanium codegen */
/* https://bugs.launchpad.net/ubuntu/+source/gcc-4.4/+bug/491872 ARM codegen*/
/* https://gcc.gnu.org/bugzilla/show_bug.cgi?id=42263 ARM codegen */
#error GCC versions before 4.4.0 are not supported on ARM or Itanium
#elif (defined(__x86_64__) || defined(__i386__)) && (__GNUC__ == 4 && (__GNUC_MINOR__ < 1 || (__GNUC_MINOR__ == 1 && __GNUC_PATCHLEVEL__ < 2)))
/* 4.1.2 is the first gcc version with 100% working atomic intrinsics on Intel */
#error GCC versions before 4.1.2 are not supported on x86/x64
#endif
#endif

static inline VOID atomicFullBarrier()
{
    __sync_synchronize();
    __asm__ __volatile__("" : : : "memory");
}

static inline SIZE_T defaultAtomicLoad(volatile SIZE_T* pAtomic)
{
    SIZE_T atomic;

    atomicFullBarrier();
    atomic = *pAtomic;
    atomicFullBarrier();

    return atomic;
}

static inline VOID defaultAtomicStore(volatile SIZE_T* pAtomic, SIZE_T var)
{
    atomicFullBarrier();
    *pAtomic = var;
    atomicFullBarrier();
}

static inline SIZE_T defaultAtomicExchange(volatile SIZE_T* pAtomic, SIZE_T var)
{
    /*
     * GCC 4.6 and before have only __sync_lock_test_and_set as an exchange operation,
     * which may not support arbitrary values on all architectures. We simply emulate
     * with a CAS instead.
     */

    SIZE_T oldval;
    do {
        oldval = *pAtomic;
    } while (!__sync_bool_compare_and_swap(pAtomic, oldval, var));

    /* __sync_bool_compare_and_swap implies a full barrier */

    return oldval;
}

static inline BOOL defaultAtomicCompareExchange(volatile SIZE_T* pAtomic, SIZE_T* pExpected, SIZE_T desired)
{
    BOOL result = __sync_bool_compare_and_swap(pAtomic, *pExpected, desired);
    if (!result) {
        *pExpected = *pAtomic;
    }

    return result;
}

static inline SIZE_T defaultAtomicIncrement(volatile SIZE_T* pAtomic)
{
    return __sync_fetch_and_add(pAtomic, 1);
}

static inline SIZE_T defaultAtomicDecrement(volatile SIZE_T* pAtomic)
{
    return __sync_fetch_and_sub(pAtomic, 1);
}

static inline SIZE_T defaultAtomicAdd(volatile SIZE_T* pAtomic, SIZE_T var)
{
    return __sync_fetch_and_add(pAtomic, var);
}

static inline SIZE_T defaultAtomicSubtract(volatile SIZE_T* pAtomic, SIZE_T var)
{
    return __sync_fetch_and_sub(pAtomic, var);
}

static inline SIZE_T defaultAtomicAnd(volatile SIZE_T* pAtomic, SIZE_T var)
{
    return __sync_fetch_and_and(pAtomic, var);
}

static inline SIZE_T defaultAtomicOr(volatile SIZE_T* pAtomic, SIZE_T var)
{
    return __sync_fetch_and_or(pAtomic, var);
}

static inline SIZE_T defaultAtomicXor(volatile SIZE_T* pAtomic, SIZE_T var)
{
    return __sync_fetch_and_xor(pAtomic, var);
}

#ifdef __cplusplus
}
#endif
#endif /* __UTILS_ATOMICS_GNU_OLD__ */
