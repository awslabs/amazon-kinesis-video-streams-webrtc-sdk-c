# Flash Multiple Partitions If MD5 Mismatch Example

## Overview

This component uses esp-serial-flasher

The following steps are performed in order to re-program the target's memory:

1. UART1 through which the new binary will be transferred is initialized.
2. The host puts the target device into boot mode and tries to connect by calling `esp_loader_connect()`.
3. The binary file is opened, its MD5 and size is acquired, as it has to be known before flashing.
4. Then `esp_loader_flash_start()` is called to enter the flashing mode and erase the amount of memory to be flashed.
5. `esp_loader_flash_write()` function is called repeatedly until the whole binary image is transferred, but only if the MD5 hash does not match the existing partition.

## Hardware Required

- Two development boards with Espressif SoCs (e.g., ESP32-DevKitC, ESP-WROVER-KIT, etc.).
- One or two USB cables for power supply and programming.
- Jumper cables to connect host to target according to table below.

## Hardware Connection

This example uses the **UART interface**. For detailed interface information and general hardware considerations, see the [Hardware Connections Guide](../../docs/hardware-connections.md#uartserial-interface).

**ESP32-P4-Function-EV-Board Pin Assignment:**

Default Configuration:

| ESP32 (host) | Espressif SoC (target) |
| :----------: | :--------------------: |
|     IO20     |          BOOT          |
|     IO22     |          RX0           |
|     IO21     |          TX0           |

## Prepare Target Firmware

Place the required target firmware binaries in the `target-firmware/` directory. You can use your own binaries, build them from the esp-idf examples, or build them from the source in the `test/target-example-src` directory.

**Required binaries:**

- `bootloader.bin` - ESP bootloader binary
- `partition-table.bin` - Partition table configuration
- `app.bin` - Main application binary

## Build and Flash

To run the example, type the following command:

```CMake
idf.py -p PORT flash monitor
```

(To exit the serial monitor, type `Ctrl-]`.)

See the [Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/index.html) for full steps to configure and use ESP-IDF to build projects.
