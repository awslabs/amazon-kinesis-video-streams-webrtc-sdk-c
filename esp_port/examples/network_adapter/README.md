# Network Adapter

This example is used to provide network connectivity to the main module on the dev board.
e.g., this example when flashed on ESP32-C6 of the `ESP32-P4 Function_EV_Board` provides the Wi-Fi connectivity to `ESP32-P4`.
- When using `webrtc_classic` example on `ESP32-P4`, this firmware must be flashed on `ESP32-C6`.

## Setup and Configuration

### Step 1: Configure Target Device
```bash
cd examples/network_adapter

# Configure for C6 chip
idf.py set-target esp32c6
```

### Step 2: Build & Flash

**Important for ESP32-C6:**
- ESP32-C6 does not have an onboard UART port. You will need to use ESP-Prog or any other JTAG.
- Use the following Pin configuration:

| ESP32-C6 (J2/Prog-C6) | ESP-Prog |
|-----------------------|----------|
| IO0                   | IO9      |
| TX0                   | TXD0     |
| RX0                   | RXD0     |
| EN                    | EN       |
| GND                   | GND      |

```bash
# Build for C6
idf.py build

# Flash to C6 (usually second USB port)
idf.py -p /dev/ttyUSB1 flash monitor
```
