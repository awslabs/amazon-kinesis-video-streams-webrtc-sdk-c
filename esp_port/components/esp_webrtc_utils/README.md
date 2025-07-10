# ESP WebRTC Utils

This component contains utility functions for WebRTC applications on ESP platforms. These utilities simplify common operations needed for WebRTC applications.

## Flash Wrapper

The flash wrapper provides a unified interface for accessing both file systems (SPIFFS) and Non-Volatile Storage (NVS):

- **Path Format**:
  - Regular files: `/spiffs/path/to/file.ext`
  - NVS items: `/nvs/<partition>/<namespace>/<key>`

- **Key Functions**:
  - `flash_wrapper_init()`: Initialize the flash wrapper
  - `flash_wrapper_read()`: Read data from files or NVS
  - `flash_wrapper_write()`: Write data to files or NVS
  - `flash_wrapper_exists()`: Check if a file or NVS key exists

- **Memory Usage**: All operations are offloaded to a dedicated task running in internal memory, allowing these APIs to be safely called from tasks running in external memory.

## ESP Work Queue

A task queue implementation for executing work items in a dedicated thread context:

- **Features**:
  - Execute tasks asynchronously in a dedicated FreeRTOS task
  - Configurable queue size, stack size, and task priority
  - Support for external RAM allocation

- **Key Functions**:
  - `esp_work_queue_init()`: Initialize the work queue with default configuration
  - `esp_work_queue_init_with_config()`: Initialize with custom configuration
  - `esp_work_queue_start()`: Start the work queue processing thread
  - `esp_work_queue_add_task()`: Queue a function for execution
  - `esp_work_queue_stop()`: Stop the work queue
  - `esp_work_queue_deinit()`: Clean up resources

## ESP WebRTC Time

Utilities for time synchronization, essential for WebRTC operations:

- **Features**:
  - SNTP time synchronization
  - Blocking and non-blocking time sync options

- **Key Functions**:
  - `esp_webrtc_time_sntp_time_sync_and_wait()`: Sync time and wait for completion
  - `esp_webrtc_time_sntp_time_sync_no_wait()`: Start time sync without waiting

## Message Utilities

Utilities for handling message buffers, especially for WebSocket communications:

- **Features**:
  - Dynamic buffer management
  - Support for fragmented message reassembly

- **Key Functions**:
  - `esp_webrtc_create_buffer_for_msg()`: Create a buffer for received messages
  - `esp_webrtc_append_msg_to_existing()`: Append message fragments to an existing buffer

## CLI Utilities

Command Line Interface utilities for configuration and debugging:

- **ESP CLI**:
  - `esp_cli_start()`: Initialize and start the ESP command line interface

- **WiFi CLI**:
  - `wifi_register_cli()`: Register WiFi-specific commands with the CLI system

## Usage Examples

### Work Queue Example

```c
// Define a work function
void my_work_function(void *data) {
    // Task execution code
    printf("Executing task with data: %s\n", (char*)data);
}

// Initialize and start the work queue
esp_work_queue_init();
esp_work_queue_start();

// Queue a task
esp_work_queue_add_task(my_work_function, "Task data");
```

### Time Synchronization Example

```c
// Synchronize time and wait for completion
esp_webrtc_time_sntp_time_sync_and_wait();

// Get current time
time_t now;
time(&now);
printf("Current time: %s", ctime(&now));
```

### Message Buffer Example

```c
// Create a message buffer
received_msg_t *msg_buffer = esp_webrtc_create_buffer_for_msg(1024);

// Append data fragments
uint8_t fragment1[] = "First part of message";
uint8_t fragment2[] = "Second part of message";

esp_webrtc_append_msg_to_existing(msg_buffer, fragment1, sizeof(fragment1) - 1, false);
esp_err_t result = esp_webrtc_append_msg_to_existing(msg_buffer, fragment2, sizeof(fragment2) - 1, true);

if (result == ESP_OK) {
    // Message is complete
    printf("Complete message: %s\n", msg_buffer->buf);
}
```

### Flash Wrapper Example

```c
// Initialize flash wrapper
flash_wrapper_init();

// Read from SPIFFS file system
uint8_t config_data[1024];
esp_err_t err = flash_wrapper_read("/spiffs/config.json", config_data, sizeof(config_data), 0);
if (err == ESP_OK) {
    // File read successfully from SPIFFS
    printf("Config data read successfully\n");
}

// Read from NVS (disguised as file I/O)
uint8_t cert_data[1024];
err = flash_wrapper_read("/nvs/fctry/rmaker_creds/client_cert", cert_data, sizeof(cert_data), 0);
if (err == ESP_OK) {
    // Certificate read successfully from NVS
    printf("Certificate data read successfully\n");
}

// Write to SPIFFS file system
const char *log_data = "Application log entry";
err = flash_wrapper_write("/spiffs/app_log.txt", log_data, strlen(log_data));
if (err == ESP_OK) {
    // Data written successfully to SPIFFS
    printf("Log data written successfully\n");
}

// Write to NVS (disguised as file I/O)
const uint8_t key_data[] = { 0x01, 0x02, 0x03, 0x04 };
err = flash_wrapper_write("/nvs/nvs/my_app/secret_key", key_data, sizeof(key_data));
if (err == ESP_OK) {
    // Data written successfully to NVS
    printf("Key data written to NVS successfully\n");
}

// Check if file exists (works for both SPIFFS and NVS)
bool exists = false;
flash_wrapper_exists("/spiffs/config.json", &exists);
flash_wrapper_exists("/nvs/fctry/rmaker_creds/client_cert", &exists);
```
