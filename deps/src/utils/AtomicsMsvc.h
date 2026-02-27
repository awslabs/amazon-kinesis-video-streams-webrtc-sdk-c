#ifndef __UTILS_ATOMICS_MSVC__
#define __UTILS_ATOMICS_MSVC__

#ifdef __cplusplus
extern "C" {
#endif

#include "Include_i.h"

#if !(defined(_M_IX86) || defined(_M_X64))
#error Atomics are not currently supported for non-x86 MSVC platforms
#endif

#ifdef _M_IX86
#define INTERLOCKED_OP(x) _Interlocked##x
#else
#define INTERLOCKED_OP(x) _Interlocked##x##64
#endif

static inline SIZE_T defaultAtomicLoad(volatile SIZE_T* pAtomic)
{
    SIZE_T atomic;
    _ReadWriteBarrier();
    atomic = *pAtomic;
    _ReadWriteBarrier();

    return atomic;
}

static inline VOID defaultAtomicStore(volatile SIZE_T* pAtomic, SIZE_T var)
{
    _ReadWriteBarrier();
    *pAtomic = var;
    _ReadWriteBarrier();
}

static inline SIZE_T defaultAtomicExchange(volatile SIZE_T* pAtomic, SIZE_T var)
{
    return INTERLOCKED_OP(Exchange)(pAtomic, var);
}

static inline BOOL defaultAtomicCompareExchange(volatile SIZE_T* pAtomic, SIZE_T* pExpected, SIZE_T desired)
{
    SIZE_T oldval = INTERLOCKED_OP(CompareExchange)(pAtomic, desired, *pExpected);
    BOOL successful = (oldval == *pExpected);
    *pExpected = oldval;

    return successful;
}

static inline SIZE_T defaultAtomicIncrement(volatile SIZE_T* pAtomic)
{
    return INTERLOCKED_OP(Increment)(pAtomic) - 1;
}

static inline SIZE_T defaultAtomicDecrement(volatile SIZE_T* pAtomic)
{
    return INTERLOCKED_OP(Decrement)(pAtomic) + 1;
}

static inline SIZE_T defaultAtomicAdd(volatile SIZE_T* pAtomic, SIZE_T var)
{
    return INTERLOCKED_OP(ExchangeAdd)(pAtomic, var);
}

static inline SIZE_T defaultAtomicSubtract(volatile SIZE_T* pAtomic, SIZE_T var)
{
    return INTERLOCKED_OP(ExchangeAdd)(pAtomic, -(SSIZE_T) var);
}

static inline SIZE_T defaultAtomicAnd(volatile SIZE_T* pAtomic, SIZE_T var)
{
    return INTERLOCKED_OP(And)(pAtomic, var);
}

static inline SIZE_T defaultAtomicOr(volatile SIZE_T* pAtomic, SIZE_T var)
{
    return INTERLOCKED_OP(Or)(pAtomic, var);
}

static inline SIZE_T defaultAtomicXor(volatile SIZE_T* pAtomic, SIZE_T var)
{
    return INTERLOCKED_OP(Xor)(pAtomic, var);
}

#ifdef __cplusplus
}
#endif
#endif /* __UTILS_ATOMICS_MSVC__ */
