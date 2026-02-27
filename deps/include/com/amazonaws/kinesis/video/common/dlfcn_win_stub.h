/**
 * Stub declarations for dlfcn
 */

#ifndef __DASH_DLFCN_STUB_H__
#define __DASH_DLFCN_STUB_H__

#ifdef __cplusplus
extern "C" {
#endif

#define RTLD_LAZY 0
#define RTLD_NOW  0

#define RTLD_GLOBAL (1 << 1)
#define RTLD_LOCAL  (1 << 2)

#define RTLD_DEFAULT 0
#define RTLD_NEXT    0

PVOID dlopen(const PCHAR filename, UINT32 flag);
INT32 dlclose(PVOID handle);
PVOID dlsym(PVOID handle, const PCHAR symbol);
PCHAR dlerror(VOID);

#ifdef __cplusplus
}
#endif

#endif // __DASH_DLFCN_STUB_H__
