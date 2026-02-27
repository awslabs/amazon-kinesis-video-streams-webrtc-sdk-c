#ifndef __UTILS_ATOMICS__
#define __UTILS_ATOMICS__

#ifdef __cplusplus
extern "C" {
#endif

#include "Include_i.h"

#if defined(__GNUC__) || defined(__clang__)
#if defined(__ATOMIC_RELAXED)
#include "AtomicsGnu.h"
#else
#include "AtomicsGnuOld.h"
#endif /* __ATOMIC_RELAXED */
#elif defined(_MSC_VER)
#include "AtomicsMsvc.h"
#else
#error No atomics implementation for your compiler is available
#endif

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
