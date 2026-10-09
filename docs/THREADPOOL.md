# Thread Pool (`ENABLE_KVS_THREADPOOL`)

When built with `-DENABLE_KVS_THREADPOOL=ON`, the SDK replaces per-message
thread creation with a process-wide thread pool. This reduces thread churn
but introduces shared-resource constraints that must be sized for the
deployment.

## What runs on the pool

| Task | Queued by | Duration |
|---|---|---|
| STUN DNS pre-resolution | `initKvsWebRtc` (once at startup) | Seconds (DNS) |
| Signaling message dispatch | `receiveLwsMessage` (per inbound message) | Application-dependent (`messageReceivedFn`) |
| DTLS handshake (OpenSSL only) | `onIceConnectionStateChange` (per peer connection) | Seconds (crypto + network RTT) |

## Defaults

| Parameter | Default | Environment variable |
|---|---|---|
| Minimum threads | 3 | `AWS_KVS_WEBRTC_THREADPOOL_MIN_THREADS` |
| Maximum threads | 10 | `AWS_KVS_WEBRTC_THREADPOOL_MAX_THREADS` |

The queue is **unbounded**: when all threads are busy, new tasks wait
indefinitely rather than failing.

## Sizing guidance

Set `AWS_KVS_WEBRTC_THREADPOOL_MAX_THREADS` to at least:

```
max_concurrent_dtls_handshakes + signaling_headroom + 1 (STUN resolver)
```

For a master that handles N simultaneous viewers connecting at once:
- Each viewer triggers one DTLS handshake (OpenSSL) that blocks a thread
  for several seconds.
- Each inbound signaling message (offer, ICE candidate) dispatches to the
  pool. If `messageReceivedFn` blocks (e.g., waiting for a peer connection
  to be created), it holds a thread.
- The STUN resolver task runs once at startup and again every 2 hours.

**Example**: a master expecting up to 5 concurrent viewer connections should
set `AWS_KVS_WEBRTC_THREADPOOL_MAX_THREADS` to at least 8 (5 DTLS + 2
signaling + 1 STUN).

Each thread allocates `KVS_STACK_SIZE` bytes of stack memory (default:
platform-dependent, typically 64 KB–1 MB).

## Deadlock risk

Because the queue is unbounded and all task types share the pool, a
`messageReceivedFn` callback that blocks waiting for a later signaling
message can deadlock the pool if all threads are occupied. Keep
`messageReceivedFn` non-blocking: queue work to your own application
threads if it requires waiting.

## Shutdown behavior

`deinitKvsWebRtc` → `destroyThreadPoolContext` → `threadpoolFree`:
- Queued but not-yet-started tasks are **dropped** (their memory is leaked
  if the caller allocated wrapper structs — e.g., `SignalingMessageWrapper`).
- Tasks that are already running are **not waited on** by `threadpoolFree`.
  The DTLS handshake task is protected by the DTLS session refcount
  (`objRefCount`), so `freeDtlsSession` spin-waits for it. Other tasks
  (signaling wrappers) may outlive `deinitKvsWebRtc`.

**Recommended shutdown order**:
1. Free every peer connection (`freePeerConnection`).
2. Free the signaling client (`freeSignalingClient`).
3. Call `deinitKvsWebRtc`.

Do not call `peerConnectionGetMetrics` after `deinitKvsWebRtc`.
