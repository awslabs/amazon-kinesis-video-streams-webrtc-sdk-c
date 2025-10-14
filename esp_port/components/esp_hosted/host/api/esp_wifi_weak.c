// SPDX-License-Identifier: Apache-2.0
// Copyright 2024 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/**
 * Weak version of esp_wifi API.
 *
 * Used when WiFi-Remote does not provide required esp_wifi calls
 */

#include "esp_hosted_api.h"

#define WEAK __attribute__((weak))

WEAK esp_err_t esp_wifi_init(const wifi_init_config_t *config)
{
    return esp_wifi_remote_init(config);
}

WEAK esp_err_t esp_wifi_deinit(void)
{
    return esp_wifi_remote_deinit();
}

WEAK esp_err_t esp_wifi_set_mode(wifi_mode_t mode)
{
    return esp_wifi_remote_set_mode(mode);
}

WEAK esp_err_t esp_wifi_get_mode(wifi_mode_t *mode)
{
    return esp_wifi_remote_get_mode(mode);
}

WEAK esp_err_t esp_wifi_start(void)
{
    return esp_wifi_remote_start();
}

WEAK esp_err_t esp_wifi_stop(void)
{
    return esp_wifi_remote_stop();
}

WEAK esp_err_t esp_wifi_restore(void)
{
    return esp_wifi_remote_restore();
}

WEAK esp_err_t esp_wifi_connect(void)
{
    return esp_wifi_remote_connect();
}

WEAK esp_err_t esp_wifi_disconnect(void)
{
    return esp_wifi_remote_disconnect();
}

WEAK esp_err_t esp_wifi_clear_fast_connect(void)
{
    return esp_wifi_remote_clear_fast_connect();
}

WEAK esp_err_t esp_wifi_deauth_sta(uint16_t aid)
{
    return esp_wifi_remote_deauth_sta(aid);
}

WEAK esp_err_t esp_wifi_scan_start(const wifi_scan_config_t *config, bool block)
{
    return esp_wifi_remote_scan_start(config, block);
}

WEAK esp_err_t esp_wifi_scan_stop(void)
{
    return esp_wifi_remote_scan_stop();
}

WEAK esp_err_t esp_wifi_scan_get_ap_num(uint16_t *number)
{
    return esp_wifi_remote_scan_get_ap_num(number);
}

WEAK esp_err_t esp_wifi_scan_get_ap_records(uint16_t *number, wifi_ap_record_t *ap_records)
{
    return esp_wifi_remote_scan_get_ap_records(number, ap_records);
}

WEAK esp_err_t esp_wifi_clear_ap_list(void)
{
    return esp_wifi_remote_clear_ap_list();
}

WEAK esp_err_t esp_wifi_sta_get_ap_info(wifi_ap_record_t *ap_info)
{
    return esp_wifi_remote_sta_get_ap_info(ap_info);
}

WEAK esp_err_t esp_wifi_set_ps(wifi_ps_type_t type)
{
    return esp_wifi_remote_set_ps(type);
}

WEAK esp_err_t esp_wifi_get_ps(wifi_ps_type_t *type)
{
    return esp_wifi_remote_get_ps(type);
}

WEAK esp_err_t esp_wifi_set_protocol(wifi_interface_t ifx, uint8_t protocol_bitmap)
{
    return esp_wifi_remote_set_protocol(ifx, protocol_bitmap);
}

WEAK esp_err_t esp_wifi_get_protocol(wifi_interface_t ifx, uint8_t *protocol_bitmap)
{
    return esp_wifi_remote_get_protocol(ifx, protocol_bitmap);
}

WEAK esp_err_t esp_wifi_set_bandwidth(wifi_interface_t ifx, wifi_bandwidth_t bw)
{
    return esp_wifi_remote_set_bandwidth(ifx, bw);
}

WEAK esp_err_t esp_wifi_get_bandwidth(wifi_interface_t ifx, wifi_bandwidth_t *bw)
{
    return esp_wifi_remote_get_bandwidth(ifx, bw);
}

WEAK esp_err_t esp_wifi_set_channel(uint8_t primary, wifi_second_chan_t second)
{
    return esp_wifi_remote_set_channel(primary, second);
}

WEAK esp_err_t esp_wifi_get_channel(uint8_t *primary, wifi_second_chan_t *second)
{
    return esp_wifi_remote_get_channel(primary, second);
}

WEAK esp_err_t esp_wifi_set_country(const wifi_country_t *country)
{
    return esp_wifi_remote_set_country(country);
}

WEAK esp_err_t esp_wifi_get_country(wifi_country_t *country)
{
    return esp_wifi_remote_get_country(country);
}

WEAK esp_err_t esp_wifi_set_mac(wifi_interface_t ifx, const uint8_t mac[6])
{
    return esp_wifi_remote_set_mac(ifx, mac);
}

WEAK esp_err_t esp_wifi_get_mac(wifi_interface_t ifx, uint8_t mac[6])
{
    return esp_wifi_remote_get_mac(ifx, mac);
}

WEAK esp_err_t esp_wifi_set_config(wifi_interface_t interface, wifi_config_t *conf)
{
    return esp_wifi_remote_set_config(interface, conf);
}

WEAK esp_err_t esp_wifi_get_config(wifi_interface_t interface, wifi_config_t *conf)
{
    return esp_wifi_remote_get_config(interface, conf);
}

WEAK esp_err_t esp_wifi_ap_get_sta_list(wifi_sta_list_t *sta)
{
    return esp_wifi_remote_ap_get_sta_list(sta);
}

WEAK esp_err_t esp_wifi_ap_get_sta_aid(const uint8_t mac[6], uint16_t *aid)
{
    return esp_wifi_remote_ap_get_sta_aid(mac, aid);
}

WEAK esp_err_t esp_wifi_set_storage(wifi_storage_t storage)
{
    return esp_wifi_remote_set_storage(storage);
}

WEAK esp_err_t esp_wifi_set_max_tx_power(int8_t power)
{
    return esp_wifi_remote_set_max_tx_power(power);
}

WEAK esp_err_t esp_wifi_get_max_tx_power(int8_t *power)
{
    return esp_wifi_remote_get_max_tx_power(power);
}

WEAK esp_err_t esp_wifi_set_country_code(const char *country, bool ieee80211d_enabled)
{
    return esp_wifi_remote_set_country_code(country, ieee80211d_enabled);
}

WEAK esp_err_t esp_wifi_get_country_code(char *country)
{
    return esp_wifi_remote_get_country_code(country);
}

WEAK esp_err_t esp_wifi_sta_get_aid(uint16_t *aid)
{
    return esp_wifi_remote_sta_get_aid(aid);
}

WEAK esp_err_t esp_wifi_sta_get_rssi(int *rssi)
{
    return esp_wifi_remote_sta_get_rssi(rssi);
}

WEAK esp_err_t esp_wifi_set_band(wifi_band_t band)
{
    return esp_wifi_remote_set_band(band);
}

WEAK esp_err_t esp_wifi_get_band(wifi_band_t *band)
{
    return esp_wifi_remote_get_band(band);
}

WEAK esp_err_t esp_wifi_set_band_mode(wifi_band_mode_t band_mode)
{
    return esp_wifi_remote_set_band_mode(band_mode);
}

WEAK esp_err_t esp_wifi_get_band_mode(wifi_band_mode_t *band_mode)
{
    return esp_wifi_remote_get_band_mode(band_mode);
}

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0)
WEAK esp_err_t esp_wifi_set_protocols(wifi_interface_t ifx, wifi_protocols_t *protocols)
{
    return esp_wifi_remote_set_protocols(ifx, protocols);
}

WEAK esp_err_t esp_wifi_get_protocols(wifi_interface_t ifx, wifi_protocols_t *protocols)
{
    return esp_wifi_remote_get_protocols(ifx, protocols);
}

WEAK esp_err_t esp_wifi_set_bandwidths(wifi_interface_t ifx, wifi_bandwidths_t *bw)
{
    return esp_wifi_remote_set_bandwidths(ifx, bw);
}

WEAK esp_err_t esp_wifi_get_bandwidths(wifi_interface_t ifx, wifi_bandwidths_t *bw)
{
    return esp_wifi_remote_get_bandwidths(ifx, bw);
}
#endif