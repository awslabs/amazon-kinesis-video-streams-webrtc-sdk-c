# ESP-TLS Implementation for KVS WebRTC

## Overview

This implementation provides ESP-TLS based TLS session management for the Amazon Kinesis Video Streams WebRTC SDK on ESP platforms. It replaces the direct mbedTLS implementation with ESP-IDF's high-level ESP-TLS abstraction.

## Key Benefits

### ✅ **No Certificate File Dependencies**
- Uses ESP certificate bundle (`esp_crt_bundle_attach`) for CA verification
- Eliminates need for `/spiffs/certs/cacert.pem` or `KVS_CA_CERT_PATH`
- No file system dependencies for certificate management

### ✅ **Platform Optimized**
- Leverages ESP-IDF's optimized TLS implementation
- Better memory management for constrained environments
- Automatic certificate bundle updates with ESP-IDF releases

### ✅ **Simplified Configuration**
- Single Kconfig option to enable/disable
- Works out-of-the-box without certificate setup
- Maintains compatibility with existing SDK interface

## Configuration

### Enable ESP-TLS Implementation

In `menuconfig`:
```
Component config → KVS WebRTC Configuration → Use ESP-TLS for KVS TLS connections
```

Or in `sdkconfig`:
```
CONFIG_USE_ESP_TLS_FOR_KVS=y
```

### Disable (Use Direct mbedTLS)

Set in `sdkconfig`:
```
CONFIG_USE_ESP_TLS_FOR_KVS=n
```

## Usage

The implementation is transparent to application code. When `CONFIG_USE_ESP_TLS_FOR_KVS=y`, TURN over TLS connections will automatically use ESP-TLS with certificate bundle verification.

### Example: TURN over TLS Configuration

```c
// Application code remains unchanged
RtcConfiguration config;
// ... configure ICE servers with TURNS URLs
// TLS verification happens automatically with ESP certificate bundle
```

## When TLS is Used

TLS sessions are created when:
1. **TURN over TCP/TLS**: Ice server uses `turns:` protocol with TCP transport
2. **Secure TURN connections**: `iceServer.isSecure = TRUE` and `protocol = TCP`

Examples of URLs that trigger TLS:
- `turns:stun.kinesisvideo.us-east-1.amazonaws.com:443?transport=tcp`
- `turns:192.168.1.100:443?transport=tcp`

## Technical Details

### Implementation Files
- `src/Tls_esp.c` - ESP-TLS implementation
- `src/Tls_mbedtls.c` - Original mbedTLS implementation (used when ESP-TLS disabled)

### Interface Compatibility
The ESP-TLS implementation maintains full API compatibility:
- `createTlsSession()`
- `tlsSessionStartWithHostname()`
- `tlsSessionProcessPacket()`
- `tlsSessionPutApplicationData()`
- `tlsSessionShutdown()`

### Certificate Verification
- **With hostname**: Strict certificate verification
- **Without hostname**: Relaxed verification for IP-based connections
- **Certificate bundle**: Uses ESP-IDF's curated CA certificate bundle

### Dependencies
When `CONFIG_USE_ESP_TLS_FOR_KVS=y`:
- Requires `esp-tls` component
- Automatically includes `esp_crt_bundle`
- No additional certificate files needed

## Migration Guide

### From File-Based Certificates

**Before** (required certificate files):
```bash
# Required certificate file setup
echo "-----BEGIN CERTIFICATE-----" > /spiffs/certs/cacert.pem
# ... certificate content
```

**After** (no files needed):
```c
// Certificate verification happens automatically
// No certificate file management required
```

### Troubleshooting

#### Certificate Verification Failures
1. Ensure `CONFIG_MBEDTLS_CERTIFICATE_BUNDLE=y`
2. Check that `esp_crt_bundle_attach` is available
3. Verify TURN server uses valid certificates

#### Compilation Issues
1. Ensure ESP-IDF version supports `esp-tls` component
2. Check that `CONFIG_USE_ESP_TLS_FOR_KVS=y` is set
3. Verify `esp-tls` is in component dependencies

#### Runtime Issues
1. Check ESP-IDF logs for TLS handshake details
2. Verify TURN server supports TLS 1.2+
3. Ensure sufficient heap memory for TLS operations

## Performance Considerations

### Memory Usage
- ESP-TLS: ~8KB heap per TLS session
- Certificate bundle: ~90KB flash (shared across all connections)
- Total: Lower memory footprint than file-based certificates

### Connection Time
- First connection: ~2-3 seconds (includes handshake)
- Subsequent connections: ~1-2 seconds
- No file I/O delays compared to certificate file reading

## Compatibility

### Supported ESP-IDF Versions
- ESP-IDF v4.4+
- ESP-IDF v5.x (recommended)

### Supported Platforms
- ESP32, ESP32-S2, ESP32-S3, ESP32-C3, ESP32-C6
- Any ESP platform with ESP-TLS support

### Limitations
- Server mode TLS not implemented (not needed for TURN client use case)
- No custom certificate injection (uses ESP certificate bundle)

## Architecture

```
┌─────────────────┐    ┌──────────────────┐    ┌─────────────────┐
│   Application   │    │   KVS WebRTC     │    │   ESP-TLS       │
│                 │    │   SDK            │    │                 │
├─────────────────┤    ├──────────────────┤    ├─────────────────┤
│ TURN Config     │───▶│ createTlsSession │───▶│ esp_tls_init    │
│ Ice Servers     │    │                  │    │                 │
│                 │    │ tlsSessionStart  │───▶│ Certificate     │
│                 │    │                  │    │ Bundle Verify   │
└─────────────────┘    └──────────────────┘    └─────────────────┘
```

## Integration with ICE Server Bridge

In **split mode** configurations, ESP-TLS works seamlessly with the ICE Server Bridge:

1. **Signaling Device** (ESP32-C6): Uses ESP-TLS for HTTPS connections to AWS KVS signaling
2. **ICE Server Transfer**: Bridge forwards TURN server configurations to streaming device
3. **Streaming Device** (ESP32-P4): Uses ESP-TLS for TURN over TCP/TLS connections

**Key Benefits**:
- **Consistent Certificate Handling**: Both devices use ESP certificate bundle
- **No Certificate File Sync**: No need to sync certificate files between devices
- **Secure TURN Connections**: Enables `turns:` protocol over TCP with TLS

📖 **For more details on ICE server bridging, see [ICE_SERVER_BRIDGE_README.md](../../../examples/ICE_SERVER_BRIDGE_README.md)**

## Future Enhancements

- [ ] Support for custom certificate bundles
- [ ] Server mode TLS for P2P scenarios
- [ ] Certificate pinning options
- [ ] Performance optimizations for frequent connections
