# libsrtp2 Component

This component is an ESP-IDF port of the Cisco libSRTP library (version 2.x), which provides implementations of the Secure Real-time Transport Protocol (SRTP) and the Secure Real-time Transport Control Protocol (SRTCP).

## Directory Structure

```
libsrtp2/
├── include/            # Public header files
│   └── srtp2/         # SRTP header files for external use
├── libsrtp/           # Original libSRTP source code
│   ├── crypto/        # Cryptographic implementations
│   ├── include/       # Internal headers
│   └── srtp/         # SRTP implementation
├── patches/           # ESP-IDF specific patches
├── port/             # ESP-IDF port-specific files
│   └── config.h      # Port-specific configuration
└── CMakeLists.txt    # Component build configuration
```

## Configuration

The component is configured to use mbedTLS for cryptographic operations. Key configuration options are set in `port/config.h`:

- `SRTP_CRYPTO_MBEDTLS`: Enables mbedTLS crypto backend
- `GCM`: Enables AES-GCM mode support
- `CPU_CISC`: Optimizations for CISC architectures
- Various feature macros for standard C library functions

## Usage

To use this component in your ESP-IDF project:

1. Add it to your project's requirements in `CMakeLists.txt`:
   ```cmake
   idf_component_register(
       ...
       REQUIRES "libsrtp2"
   )
   ```

2. Include the SRTP header in your code:
   ```c
   #include <srtp2/srtp.h>
   ```

3. Initialize and use SRTP:
   ```c
   srtp_t session;
   srtp_init();
   // Configure and use SRTP...
   srtp_shutdown();
   ```

## Dependencies

- mbedTLS (required for cryptographic operations)

## Notes

- The component uses a patched version of libSRTP for ESP-IDF compatibility
- All cryptographic operations are performed using mbedTLS
- The component is configured for optimal performance on ESP32 platforms
- Debug logging can be enabled through ESP-IDF's logging system
