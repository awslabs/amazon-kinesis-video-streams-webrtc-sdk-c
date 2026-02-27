# Plan: Make WebRTC SDK Self-Contained (No Signaling)

## Context

The WebRTC SDK (`amazon-kinesis-video-streams-webrtc-sdk-c`) currently depends on two external AWS repos:
- **PIC** (`amazon-kinesis-video-streams-pic`) — provides utility functions (data structures, threading, logging, timers, etc.) and a state machine framework
- **producer-c** (`amazon-kinesis-video-streams-producer-c`) — provides AWS credential types, signaling support

The goal is to vendor the necessary source files from PIC and producer-c into the WebRTC SDK so it **can** build self-contained with only **libsrtp**, **jsmn** (already vendored), and **openssl** as external dependencies. Signaling is assumed disabled in self-contained mode.

The existing `BUILD_DEPENDENCIES` path (download + build PIC/producer-c) is preserved as the default. A new `ENABLE_SELF_CONTAINED` option activates the vendored code instead.

---

## Current Dependency Map (signaling OFF)

**Link-time:** `kvsWebrtcClient` → `kvspicUtils`, `kvspicState`, openssl, libsrtp2, pthreads

**Header-time (include chain):**
```
WebRTC Include.h
  → PIC client/Include.h
    → PIC common/CommonDefs.h, PlatformUtils.h
    → PIC utils/Include.h
    → PIC mkvgen/Include.h → utils/Include.h
    → PIC view/Include.h → heap/Include.h → utils/Include.h
    → PIC heap/Include.h
    → PIC state/Include.h → mkvgen/Include.h
  → producer-c common/Include.h → PIC client/Include.h, jsmn.h
```

---

## Step 1: Create `deps/` directory and copy PIC headers

Create `deps/include/com/amazonaws/kinesis/video/` with the following headers copied from PIC:

| Target path (under `deps/include/`) | Source (from PIC) |
|---|---|
| `com/amazonaws/kinesis/video/common/CommonDefs.h` | `pic/src/common/include/.../CommonDefs.h` |
| `com/amazonaws/kinesis/video/common/PlatformUtils.h` | `pic/src/common/include/.../PlatformUtils.h` |
| `com/amazonaws/kinesis/video/common/dlfcn_win_stub.h` | `pic/src/common/include/.../dlfcn_win_stub.h` |
| `com/amazonaws/kinesis/video/utils/Include.h` | `pic/src/utils/include/.../Include.h` |
| `com/amazonaws/kinesis/video/state/Include.h` | `pic/src/state/include/.../Include.h` |
| `com/amazonaws/kinesis/video/mkvgen/Include.h` | `pic/src/mkvgen/include/.../Include.h` |
| `com/amazonaws/kinesis/video/heap/Include.h` | `pic/src/heap/include/.../Include.h` |
| `com/amazonaws/kinesis/video/view/Include.h` | `pic/src/view/include/.../Include.h` |
| `com/amazonaws/kinesis/video/client/Include.h` | `pic/src/client/include/.../Include.h` |

## Step 2: Copy producer-c headers

| Target path | Source (from producer-c) |
|---|---|
| `deps/include/com/amazonaws/kinesis/video/common/Include.h` | `producer-c/src/include/.../common/Include.h` |
| `deps/include/com/amazonaws/kinesis/video/common/jsmn.h` | `producer-c/src/include/.../common/jsmn.h` |

**Note:** PIC and producer-c both have files under `common/`. PIC has `CommonDefs.h` and `PlatformUtils.h`; producer-c has `Include.h` and `jsmn.h`. No conflicts.

## Step 3: Copy PIC source files

**Utils sources** → `deps/src/utils/`:
- `Include_i.h` (internal header)
- `AtomicsGnu.h`, `AtomicsGnuOld.h`, `AtomicsMsvc.h` (internal atomics headers)
- 18 `.c` files (of 32 total — only what's actually used):

  **Core library (16):**
  `Allocators.c`, `Atomics.c`, `Crc32.c`, `CustomAssert.c`, `DoubleLinkedList.c`, `Endianness.c`, `HashTable.c`, `Hex.c`, `Logger.c`, `Mutex.c`, `SingleLinkedList.c`, `StackQueue.c`, `String.c`, `Thread.c`, `Time.c`, `TimerQueue.c`

  **Tests only (2):**
  `InstrumentedAllocators.c`, `FileIo.c`

  **NOT needed (14 — only used by signaling or unused):**
  ~~Base64, BitField, BitReader, Directory, DynamicLibrary, Environment, ExponentialBackoffRetryStrategy, FileLogger, Math, Semaphore, Tags, Threadpool, ThreadsafeBlockingQueue, Version~~

Source: `pic/src/utils/src/`

**State sources** → `deps/src/state/`:
- `Include_i.h` (internal header)
- `State.c`

Source: `pic/src/state/src/`

## Step 4: Producer-c source files

The test fixture (`tst/WebRTCClientTestFixture.cpp`) calls `createStaticCredentialProvider`/`freeStaticCredentialProvider` under `#ifdef ENABLE_SIGNALING`. Since signaling is OFF in self-contained mode, these aren't called, but the types (`PAwsCredentialProvider`, `AwsCredentials`) are referenced unguarded. The **headers** from Step 2 provide the type definitions — no producer-c source files needed.

## Step 5: Update `CMakeLists.txt` — add `ENABLE_SELF_CONTAINED` option

### 5a. New option
```cmake
option(ENABLE_SELF_CONTAINED "Use vendored PIC/producer-c sources instead of external dependencies" OFF)
```

When `ENABLE_SELF_CONTAINED=ON`:
- `ENABLE_SIGNALING` is forced OFF
- `BUILD_DEPENDENCIES` skips kvspic/kvsCommonLws/websockets
- Vendored PIC sources are compiled instead

### 5b. Conditional: self-contained path
```cmake
if(ENABLE_SELF_CONTAINED)
  # Force signaling off
  set(ENABLE_SIGNALING OFF CACHE BOOL "" FORCE)

  # Build vendored PIC utils + state as a static library
  file(GLOB PIC_UTILS_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/deps/src/utils/*.c")
  set(PIC_STATE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/deps/src/state/State.c")

  add_library(kvspic_vendored STATIC ${PIC_UTILS_SOURCES} ${PIC_STATE_SOURCES})
  target_include_directories(kvspic_vendored
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/deps/include)
  target_link_libraries(kvspic_vendored
    PRIVATE ${CMAKE_THREAD_LIBS_INIT} ${CMAKE_DL_LIBS})

  # Alias so kvsWebrtcClient can link uniformly
  add_library(kvspicUtils ALIAS kvspic_vendored)
  add_library(kvspicState ALIAS kvspic_vendored)

  # Add vendored headers to include path
  include_directories(${CMAKE_CURRENT_SOURCE_DIR}/deps/include)
endif()
```

### 5c. Existing path unchanged
When `ENABLE_SELF_CONTAINED=OFF` (default), the existing `BUILD_DEPENDENCIES` logic runs as before — builds kvspic externally, finds kvspicUtils/kvspicState, optionally builds signaling.

### 5d. kvsWebrtcClient linkage (no changes needed)
The existing `target_link_libraries(kvsWebrtcClient PRIVATE kvspicUtils kvspicState ...)` works for both paths:
- Default: links to externally-built kvspicUtils/kvspicState libraries
- Self-contained: links to `kvspic_vendored` via the ALIAS targets

### 5e. Skip external PIC build when self-contained
In the `BUILD_DEPENDENCIES` block, wrap the kvspic/kvsCommonLws builds:
```cmake
if(NOT ENABLE_SELF_CONTAINED)
  if(ENABLE_SIGNALING)
    build_dependency(kvsCommonLws ${BUILD_ARGS})
  else()
    build_dependency(kvspic ${BUILD_ARGS})
  endif()
endif()
```

## Step 6: Update `tst/CMakeLists.txt`

When self-contained, replace `kvspicUtils` with `kvspic_vendored`:
```cmake
if(ENABLE_SELF_CONTAINED)
  set(KVSPIC_TEST_LIB kvspic_vendored)
else()
  set(KVSPIC_TEST_LIB kvspicUtils)
endif()

target_link_libraries(webrtc_client_test
    kvsWebrtcClient
    ${EXTRA_DEPS}
    ${OPENSSL_CRYPTO_LIBRARY}
    ${KVSPIC_TEST_LIB}
    ${GTEST_LIBNAME}
    ${AWS_SDK_TEST_LIBS})
```

## Step 7: Verify signaling-guarded test code compiles

The test fixture (`tst/WebRTCClientTestFixture.h`) has:
- `PAwsCredentialProvider mTestCredentialProvider;` (line 305, unguarded) — OK, type comes from copied header
- `StaticCredentialProvider` typedef (line 35-38, unguarded) — OK, types come from copied header
- `MAX_REGION_NAME_LEN` (line 317, unguarded) — OK, comes from producer-c common/Include.h
- `ACCESS_KEY_ENV_VAR`, `SECRET_KEY_ENV_VAR`, etc. — OK, come from producer-c common/Include.h
- `createStaticCredentialProvider()`/`freeStaticCredentialProvider()` calls (guarded by `#ifdef ENABLE_SIGNALING`) — NOT compiled, only declared in header

No producer-c source code compilation needed.

---

## Build Modes Summary

| Mode | Command | Dependencies |
|---|---|---|
| **Default** (existing) | `cmake -DENABLE_SIGNALING=OFF ..` | External kvspic + openssl + libsrtp2 |
| **Default + signaling** | `cmake -DENABLE_SIGNALING=ON ..` | External kvspic + producer-c + libwebsockets + openssl + libsrtp2 |
| **Self-contained** | `cmake -DENABLE_SELF_CONTAINED=ON ..` | openssl + libsrtp2 only (signaling forced OFF) |

---

## Files Modified

| File | Action |
|---|---|
| `CMakeLists.txt` | Add `ENABLE_SELF_CONTAINED` option + conditional vendored PIC path |
| `tst/CMakeLists.txt` | Conditional link target for self-contained mode |
| `deps/include/**` | NEW — copied PIC + producer-c headers (~11 files) |
| `deps/src/utils/**` | NEW — copied PIC utils sources (18 .c + 4 .h = 22 files) |
| `deps/src/state/**` | NEW — copied PIC state sources (2 files) |

Total new files: ~35 files (all copied from PIC/producer-c, no new code written)

---

## Verification

1. **Self-contained build:** `cmake -DENABLE_SELF_CONTAINED=ON -DUSE_OPENSSL=ON -DBUILD_TEST=ON .. && make`
2. **Default build still works:** `cmake -DENABLE_SIGNALING=OFF -DUSE_OPENSSL=ON .. && make`
3. **Tests:** `./webrtc_client_test` — all non-signaling tests pass in both modes
4. **Self-contained link check:** Verify no references to external kvspicUtils, kvspicState, kvsCommonLws
