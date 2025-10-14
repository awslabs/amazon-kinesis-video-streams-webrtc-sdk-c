# WiFi remote EPPP RPC server

This is a standalone application serving as the slave device for `esp_wifi_remote` users (with `eppp` RPC).

## Overview

You need to configure and connect a slave device to the `esp_wifi_remote` host and run this application. Please fallow carefully these guidelines on HW connection and configuration of the slave device, based on the host device.

## HW connection

We currently support only `UART` transport you just need to connect Rx, Tx and GND and configure these fields accordingly:
* CONFIG_ESP_WIFI_REMOTE_EPPP_UART_TX_PIN
* CONFIG_ESP_WIFI_REMOTE_EPPP_UART_RX_PIN

## SW configuration

You will have to install server side certificates and keys, as well as the CA which should verify the client side.
Please configure these options:
* CONFIG_ESP_WIFI_REMOTE_EPPP_CLIENT_CA
* CONFIG_ESP_WIFI_REMOTE_EPPP_SERVER_CRT
* CONFIG_ESP_WIFI_REMOTE_EPPP_SERVER_KEY
