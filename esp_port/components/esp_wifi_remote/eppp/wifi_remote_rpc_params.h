/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

struct esp_wifi_remote_config {
    wifi_interface_t interface;
    wifi_config_t conf;
};

struct esp_wifi_remote_mac_t {
    esp_err_t err;
    uint8_t mac[6];
};

struct esp_wifi_remote_eppp_ip_event {
    int32_t id;
    esp_netif_ip_info_t wifi_ip;
    esp_netif_ip_info_t ppp_ip;
    esp_netif_dns_info_t dns;
};
