# esp_wifi_remote

[![Component Registry](https://components.espressif.com/components/espressif/esp_wifi_remote/badge.svg)](https://components.espressif.com/components/espressif/esp_wifi_remote)

The `esp_wifi_remote` component is designed to extend WiFi functionality to ESP chipsets that lack native WiFi support. By simply adding a dependency to this component from your project, you gain access to WiFi capabilities via the WiFi-remote menuconfig and standard `esp_wifi` interface.

Moreover, `esp_wifi_remote` can be utilized on ESP chipsets that do support native WiFi, providing an additional WiFi interface through the `esp_wifi_remote` API.

To employ this component, a slave device -- capable of WiFi connectivity -- must be connected to your target device in a specified manner, as defined by the transport layer of [`esp_hosted`](https://github.com/espressif/esp-hosted).

Functionally, `esp_wifi_remote` wraps the public API of `esp_wifi`, offering a set of function call namespaces prefixed with esp_wifi_remote. These calls are translated into Remote Procedure Calls (RPC) to another target device (referred to as the "slave" device), which then executes the appropriate `esp_wifi` APIs.

Notably, `esp_wifi_remote` heavily relies on a specific version of the `esp_wifi` component. Consequently, the majority of its headers, sources, and configuration files are pre-generated based on the actual version of `esp_wifi`.

It's important to highlight that `esp_wifi_remote` does not directly implement the RPC calls; rather, it relies on dependencies for this functionality. Presently, only esp_hosted is supported to provide the RPC functionality required by esp_wifi_remote.


## Dependencies on `esp_wifi`

Public API needs to correspond exactly to the `esp_wifi` API. Some of the internal types depend on the actual wifi target, as well as some default configuration values. Therefore it's easier to maintain consistency between this component and the exact version of `esp_wifi` automatically in CI:

* We extract function prototypes from `esp_wifi.h` and use them to generate `esp_wifi_remote` function declarations.
* We process the local `esp_wifi_types_native.h` and replace `CONFIG_IDF_TARGET` to `CONFIG_SLAVE_IDF_TARGET` and `CONFIG_SOC_WIFI_...` to `CONFIG_SLAVE_....`
* Similarly we process `esp_wifi`'s Kconfig, so the dependencies are on the slave target and slave SOC capabilities.

Please check the [README.md](./scripts/README.md) for more details on the generation step and testing consistency.
