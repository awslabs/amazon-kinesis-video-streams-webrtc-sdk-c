# Amazon Kinesis Video Streams WebRTC SDK C - ESP-IDF Guide

This guide provides specific instructions for building and running the Amazon Kinesis Video Streams WebRTC SDK C on ESP32 devices using the ESP-IDF framework.

## Prerequisites

- ESP-IDF v5.4 or later installed
- ESP32 development board with 4MB PSRAM (mandatory requirement)
- AWS Account and credentials
- Basic familiarity with ESP-IDF development

## Supported Hardware

The SDK has been tested on the following boards:

1. ESP32 (with PSRAM)
2. ESP32-S3
3. ESP32-P4

### Important Hardware Requirements

- **PSRAM is mandatory**: Internal memory alone is not sufficient to run the WebRTC stack
- All development boards must have PSRAM capability
- Choose boards with adequate PSRAM for optimal performance

### Board-Specific Requirements

#### ESP32-P4 Setup
The ESP32-P4 requires additional components and setup:

1. Required Components (automatically included in P4 builds):
   - esp-hosted
   - esp-remote

2. Network Adapter Setup:
   - Flash the network_adapter example on the ESP32-C6 (part of ESP32-P4 Function EV board)
   - The network_adapter example can be found in the ESP32-C6 SDK

Example build command for ESP32-P4:
```bash
idf.py set-target esp32p4
idf.py build
```

## Build Instructions

### 1. Setting up the Build Environment

First, make sure you have the ESP-IDF environment properly set up:

```bash
. $IDF_PATH/export.sh  # On Linux/macOS
%IDF_PATH%\export.bat  # On Windows
```

### 2. Project Configuration

The SDK requires several configurations to be set in the ESP-IDF project:

1. Run menuconfig:
```bash
idf.py menuconfig
```

2. Important configurations to set:
   - Component config → mbedTLS
     - Enable MBEDTLS_DYNAMIC_BUFFER
     - Set MBEDTLS_SSL_IN_CONTENT_LEN to 16384
     - Set MBEDTLS_SSL_OUT_CONTENT_LEN to 16384
   - Component config → ESP HTTPS server
     - Increase Max HTTP Request Header Length to 8192
   - Component config → FreeRTOS
     - Increase FreeRTOS timer task stack size to 4096

### 3. Memory Considerations

PSRAM is mandatory for running the WebRTC stack as ESP32's internal RAM is insufficient. Configure your setup properly:

- Enable SPIRAM support in menuconfig (required)
  - Component config → ESP32/ESP32-S3/ESP32-P4 Specific → Support for external SPI RAM
  - Set "SPI RAM config" appropriate for your board
  - Enable "Run memory test on SPI RAM initialization"
  - Set "SPI RAM access method" based on your needs (recommended: "Make RAM allocatable using heap_caps_malloc()")

- Recommended PSRAM settings in menuconfig:
  - Set "Size of per-pointer metadata in bytes" to 8
  - Enable "Allow external memory as an argument to malloc()"

- Memory allocation strategy:
  - Use PSRAM for large buffers and media frames
  - Reserve internal RAM for critical system operations
  - Monitor memory usage during development

Example of proper PSRAM initialization:
```c
#include "esp_psram.h"

void app_main(void) {
    // Initialize PSRAM
    if (!esp_psram_is_initialized()) {
        ESP_LOGE(TAG, "PSRAM initialization failed! WebRTC stack requires PSRAM.");
        return;
    }

    size_t psram_size = esp_psram_get_size();
    ESP_LOGI(TAG, "PSRAM size: %d bytes", psram_size);

    // Continue with application initialization
    // ...
}
```

### 4. Certificate Storage

For embedded devices, certificates need to be stored in non-volatile storage. The example implementation uses SPIFFS, but NVS can also be used as an alternative:

1. SPIFFS Storage (Default Implementation):
   ```c
   // Certificate paths in SPIFFS
   setenv("AWS_IOT_CORE_CERT", "/spiffs/certs/certificate.pem", 1);
   setenv("AWS_IOT_CORE_PRIVATE_KEY", "/spiffs/certs/private.key", 1);
   setenv("AWS_KVS_CACERT_PATH", "/spiffs/certs/", 1);
   ```

   To use SPIFFS:
   - Create a `certs` directory in your project's SPIFFS partition
   - Copy your certificates to this directory during flash image creation
   - Ensure SPIFFS is mounted before accessing certificates:
   ```c
   esp_vfs_spiffs_conf_t conf = {
       .base_path = "/spiffs",
       .partition_label = NULL,
       .max_files = 5,
       .format_if_mount_failed = true
   };
   esp_vfs_spiffs_register(&conf);
   ```

2. NVS Storage (Alternative):
   ```c
   #include "nvs_flash.h"
   #include "nvs.h"

   void store_certificates(void) {
       nvs_handle_t nvs_handle;
       nvs_open("storage", NVS_READWRITE, &nvs_handle);
       nvs_set_str(nvs_handle, "cert", certificate_data);
       nvs_set_str(nvs_handle, "key", private_key_data);
       nvs_commit(nvs_handle);
       nvs_close(nvs_handle);
   }
   ```

   Example of loading from NVS:
   ```c
   void load_certificates(char* cert_buffer, size_t max_size) {
       nvs_handle_t nvs_handle;
       size_t required_size;
       nvs_open("storage", NVS_READONLY, &nvs_handle);
       nvs_get_str(nvs_handle, "cert", NULL, &required_size);
       if (required_size <= max_size) {
           nvs_get_str(nvs_handle, "cert", cert_buffer, &required_size);
       }
       nvs_close(nvs_handle);
   }
   ```

## IoT Core Setup

For IoT Core setup and credentials, refer to the [Setup IoT section in main README](./README.md#setup-iot). However, for ESP32 devices:

1. Store certificates in NVS or secure element
2. Initialize certificates during boot
3. Implement secure boot if required

Example of loading certificates from NVS:
```c
void load_certificates(char* cert_buffer, size_t max_size) {
    nvs_handle_t nvs_handle;
    size_t required_size;
    nvs_open("storage", NVS_READONLY, &nvs_handle);
    nvs_get_str(nvs_handle, "cert", NULL, &required_size);
    if (required_size <= max_size) {
        nvs_get_str(nvs_handle, "cert", cert_buffer, &required_size);
    }
    nvs_close(nvs_handle);
}
```

## Network Configuration

ESP32 requires proper WiFi/Ethernet setup before WebRTC can work:

```c
void wifi_init_sta(void) {
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_WIFI_SSID,
            .password = CONFIG_WIFI_PASSWORD,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}
```

## Memory Optimization

For ESP32 devices, consider these additional memory optimization techniques:

1. Use PSRAM for large buffers:
```c
#ifdef CONFIG_SPIRAM_SUPPORT
    // Allocate in PSRAM
    void* large_buffer = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
#else
    // Regular allocation
    void* large_buffer = malloc(size);
#endif
```

2. Adjust WebRTC buffer sizes:
```c
// Reduce buffer sizes for ESP32
configuration.kvsRtcConfiguration.maximumTransmissionUnit = 1200;
configuration.kvsRtcConfiguration.maximumNumberOfBuffers = 12;
```

## Debugging

ESP32-specific debugging tips:

1. Enable Core Dumps:
```bash
idf.py menuconfig
# Enable saving core dumps
# Component config → ESP System Settings → Core dump destination → UART
```

2. Monitor memory usage:
```c
#include "esp_heap_caps.h"

void print_memory_info() {
    printf("Free DRAM: %d bytes\n", heap_caps_get_free_size(MALLOC_CAP_8BIT));
    printf("Free IRAM: %d bytes\n", heap_caps_get_free_size(MALLOC_CAP_32BIT));
    #ifdef CONFIG_SPIRAM_SUPPORT
    printf("Free PSRAM: %d bytes\n", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    #endif
}
```

## Known Limitations

1. Memory Constraints
   - ESP32 has limited RAM (520KB)
   - Consider using ESP32-WROVER with PSRAM for larger applications

2. CPU Performance
   - WebRTC operations may be CPU intensive
   - Monitor CPU usage and optimize where possible

3. Network Stability
   - WiFi connection stability is crucial
   - Implement proper reconnection handling

## Additional Resources

- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/)
- [Main KVS WebRTC SDK Documentation](./README.md)
- [ESP32 Hardware Reference](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/hw-reference/index.html)

## License

This library is licensed under the Apache 2.0 License.
