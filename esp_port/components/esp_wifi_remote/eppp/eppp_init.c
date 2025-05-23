/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "esp_log.h"
#include "esp_wifi.h"
#include "eppp_link.h"

__attribute__((weak)) esp_netif_t *wifi_remote_eppp_init(eppp_type_t role)
{
    uint32_t our_ip = role == EPPP_SERVER ? EPPP_DEFAULT_SERVER_IP() : EPPP_DEFAULT_CLIENT_IP();
    uint32_t their_ip = role == EPPP_SERVER ? EPPP_DEFAULT_CLIENT_IP() : EPPP_DEFAULT_SERVER_IP();
    eppp_config_t config = EPPP_DEFAULT_CONFIG(our_ip, their_ip);
    // We currently support only UART transport
    config.transport = EPPP_TRANSPORT_UART;
    config.uart.tx_io = CONFIG_ESP_WIFI_REMOTE_EPPP_UART_TX_PIN;
    config.uart.rx_io = CONFIG_ESP_WIFI_REMOTE_EPPP_UART_RX_PIN;
    config.ppp.netif_description = CONFIG_ESP_WIFI_REMOTE_EPPP_NETIF_DESCRIPTION;
    config.ppp.netif_prio = CONFIG_ESP_WIFI_REMOTE_EPPP_NETIF_PRIORITY;
    return eppp_open(role, &config, portMAX_DELAY);
}
