#ifndef __UTILS_ATOMICS__
#define __UTILS_ATOMICS__

#ifdef __cplusplus
extern "C" {
#endif

#include "platform_esp32.h"
#include "atomics.h"

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc11-extensions"
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

static inline SIZE_T defaultAtomicLoad(volatile SIZE_T* pAtomic)
{
    return __atomic_load_n(pAtomic, __ATOMIC_SEQ_CST);
}

static inline VOID defaultAtomicStore(volatile SIZE_T* pAtomic, SIZE_T var)
{
    __atomic_store_n(pAtomic, var, __ATOMIC_SEQ_CST);
}

static inline SIZE_T defaultAtomicExchange(volatile SIZE_T* pAtomic, SIZE_T var)
{
    return __atomic_exchange_n(pAtomic, var, __ATOMIC_SEQ_CST);
}

static inline BOOL defaultAtomicCompareExchange(volatile SIZE_T* pAtomic, SIZE_T* pExpected, SIZE_T desired)
{
    return __atomic_compare_exchange_n(pAtomic, pExpected, desired, FALSE, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

static inline SIZE_T defaultAtomicIncrement(volatile SIZE_T* pAtomic)
{
    return __atomic_fetch_add(pAtomic, 1, __ATOMIC_SEQ_CST);
}

static inline SIZE_T defaultAtomicDecrement(volatile SIZE_T* pAtomic)
{
    return __atomic_fetch_sub(pAtomic, 1, __ATOMIC_SEQ_CST);
}

static inline SIZE_T defaultAtomicAdd(volatile SIZE_T* pAtomic, SIZE_T var)
{
    return __atomic_fetch_add(pAtomic, var, __ATOMIC_SEQ_CST);
}

static inline SIZE_T defaultAtomicSubtract(volatile SIZE_T* pAtomic, SIZE_T var)
{
    return __atomic_fetch_sub(pAtomic, var, __ATOMIC_SEQ_CST);
}

static inline SIZE_T defaultAtomicAnd(volatile SIZE_T* pAtomic, SIZE_T var)
{
    return __atomic_fetch_and(pAtomic, var, __ATOMIC_SEQ_CST);
}

static inline SIZE_T defaultAtomicOr(volatile SIZE_T* pAtomic, SIZE_T var)
{
    return __atomic_fetch_or(pAtomic, var, __ATOMIC_SEQ_CST);
}

static inline SIZE_T defaultAtomicXor(volatile SIZE_T* pAtomic, SIZE_T var)
{
    return __atomic_fetch_xor(pAtomic, var, __ATOMIC_SEQ_CST);
}

PUBLIC_API atomicLoad globalAtomicLoad = defaultAtomicLoad;
PUBLIC_API atomicStore globalAtomicStore = defaultAtomicStore;
PUBLIC_API atomicExchange globalAtomicExchange = defaultAtomicExchange;
PUBLIC_API atomicCompareExchange globalAtomicCompareExchange = defaultAtomicCompareExchange;
PUBLIC_API atomicIncrement globalAtomicIncrement = defaultAtomicIncrement;
PUBLIC_API atomicDecrement globalAtomicDecrement = defaultAtomicDecrement;
PUBLIC_API atomicAdd globalAtomicAdd = defaultAtomicAdd;
PUBLIC_API atomicSubtract globalAtomicSubtract = defaultAtomicSubtract;
PUBLIC_API atomicAnd globalAtomicAnd = defaultAtomicAnd;
PUBLIC_API atomicOr globalAtomicOr = defaultAtomicOr;
PUBLIC_API atomicXor globalAtomicXor = defaultAtomicXor;

#ifdef __cplusplus
}
#endif
#endif /* __UTILS_ATOMICS__ */
