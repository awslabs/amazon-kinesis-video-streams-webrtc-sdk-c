# Changelog

## [0.2.3](https://github.com/espressif/esp-protocols/commits/wifi_remote-v0.2.3)

### Bug Fixes

- Fix server event/command race condtion using eventfd ([732b1d5](https://github.com/espressif/esp-protocols/commit/732b1d5))
- Lock server before marshalling events ([9e13870](https://github.com/espressif/esp-protocols/commit/9e13870))

## [0.2.2](https://github.com/espressif/esp-protocols/commits/wifi_remote-v0.2.2)

### Bug Fixes

- Added more netif options for eppp connection ([24ce867](https://github.com/espressif/esp-protocols/commit/24ce867))
- Do not restrict EPPP config to RSA keys only ([f05c765](https://github.com/espressif/esp-protocols/commit/f05c765), [#570](https://github.com/espressif/esp-protocols/issues/570))

## [0.2.1](https://github.com/espressif/esp-protocols/commits/wifi_remote-v0.2.1)

### Bug Fixes

- Added misc wifi API in eppp impl ([93256d1](https://github.com/espressif/esp-protocols/commit/93256d1))
- Updated eppp dependency not to use fixed version ([3a48c06](https://github.com/espressif/esp-protocols/commit/3a48c06))

## [0.2.0](https://github.com/espressif/esp-protocols/commits/wifi_remote-v0.2.0)

### Features

- Add support for simple eppp based RPC ([fd168d8](https://github.com/espressif/esp-protocols/commit/fd168d8))

### Bug Fixes

- Make services restartable, code cleanup ([6c82ce2](https://github.com/espressif/esp-protocols/commit/6c82ce2))
- Add examples to CI ([d2b7c55](https://github.com/espressif/esp-protocols/commit/d2b7c55))

## [0.1.12](https://github.com/espressif/esp-protocols/commits/wifi_remote-v0.1.12)

### Features

- Added generation step for wifi_remote based on IDF ([dfb00358](https://github.com/espressif/esp-protocols/commit/dfb00358))
- Move to esp-protocols ([edc3c2d](https://github.com/espressif/esp-protocols/commit/edc3c2d))
