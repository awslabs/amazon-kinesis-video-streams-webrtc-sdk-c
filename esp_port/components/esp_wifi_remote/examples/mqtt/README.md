# MQTT application running on WiFi station

This is a simple mqtt demo, that connects to WiFi AP first. This application has a dependency to `esp_wifi_remote`, so that if it's build and executed on a chipset without WiFI capabilities it redirects all wifi calls the remote target.

## Overview

When running this example on a target that doesn't natively support WiFi, please make sure that the remote target (slave application) is connected to your chipset via the configured transport interface.

Connection to the slave device also depends on RPC library used. It is recommended to use [`esp_hosted`](https://github.com/espressif/esp-hosted). Alternatively you can use [`eppp_link`](https://components.espressif.com/components/espressif/eppp_link).

Please note, that `esp_hosted` as a component is currently WIP, so the `wifi_remote` defaults to `eppp`, for now.

## HW connection

We currently support only `UART` transport, so the connection is very simple. You only need to connect Rx, Tx and GND with the remote target.
You need to configure these fields according to your connection:
* CONFIG_ESP_WIFI_REMOTE_EPPP_UART_TX_PIN
* CONFIG_ESP_WIFI_REMOTE_EPPP_UART_RX_PIN

## SW configuration

The RPC mechanism between the host and the slave micro uses TLS with mutual authentication, so you would have to configure certificates and keys for both parties. This application -- host target -- is considered RPC client, so it needs client's certificate and key, as well as the CA certificate to validate the server (slave application).
If self-signed certificates are acceptable, you can use [generate_test_certs](../test_certs/generate_test_certs.sh) script to generate both the CA and the keys itself and convert them to the PEM format that's accepted by the EPPP RPC engine.
You will have to configure these options:
* CONFIG_ESP_WIFI_REMOTE_EPPP_SERVER_CA
* CONFIG_ESP_WIFI_REMOTE_EPPP_CLIENT_CRT
* CONFIG_ESP_WIFI_REMOTE_EPPP_CLIENT_KEY

## Setting up slave device

You need to set up the connection and configuration in a similar way on the slave part (connection pins + certificates and keys). Please refer to the [slave_application](../server/README.md) README for more information.
