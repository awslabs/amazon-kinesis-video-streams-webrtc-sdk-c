# About Proto Files


## Protobuf Submodule

[protobuf-c](https://github.com/protobuf-c/protobuf-c) is open source code used as submodule in ESP-Hosted-FG in directory `../protobuf-c/`
If this directory is empty, please run
```sh
$ cd esp-hosted
$ git submodule update --init --recursive
```

## Files

- `esp_hosted_rpc.proto`
  - This is Ready-To-Use protobuf file which has messages for Request / Response / Events to communicate between Host and ESP
  - User can add his own message field in `.proto` file and generate respective C files using 'protoc'

- `esp_hosted_rpc.pb-c.c` & `esp_hosted_rpc.pb-c.h`
  - Ready-To-Use Source Generated files using `esp_hosted_rpc.proto`
  - These files also cached which was generated with current `esp_hosted_rpc.proto` file for easy use (No need to generate again)
  - If any addition or modifications `esp_hosted_rpc.proto` done, these files need to be re-generated


## Generate esp_hosted_rpc.pb-c.c & esp_hosted_rpc.pb-c.h

If you want to add or modify existing set of RPC procedures supported, you need to modify `esp_hosted_rpc.proto` as needed and build it to generate new set of `esp_hosted_rpc.pb-c.c` & `esp_hosted_rpc.pb-c.h`.
For this, third party software for protobuf C compiler is needed to be installed
- Debian/Ubuntu
  - sudo apt install protobuf-c-compiler
- Mac OS
  - brew install protobuf
- Windows
  - check https://github.com/protobuf-c/protobuf-c

`protoc-c` command should be available once installed.

This software might only be needed on development environment, Once esp_hosted_rpc.pb-c.c & esp_hosted_rpc.pb-c.h files are generated, could also be uninstalled.

##### Steps to generate
```sh
$ cd <path/to/esp_hosted_fg>/common/proto
$ protoc-c esp_hosted_rpc.proto --c_out=.
```

## Add new RPC message
To send an new RPC request/response
<TBD>
1. Add C function in `host/host_common/commands.c`
2. Create python binding in `host/linux/host_control/python_support/commands_map_py_to_c.py` and its python function in `host/linux/host_control/python_support/commands_lib.py`.
3. Add ESP side C function in `esp/esp_driver/network_adapter/main/slave_commands.c`, respective to python function, to handle added message field.

User can test added functionality using `host/linux/host_control/python_support/test.py`.
