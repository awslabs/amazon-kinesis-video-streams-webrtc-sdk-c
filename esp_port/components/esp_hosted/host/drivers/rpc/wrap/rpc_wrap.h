// SPDX-License-Identifier: Apache-2.0
// Copyright 2015-2021 Espressif Systems (Shanghai) PTE LTD
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

/** prevent recursive inclusion **/
#ifndef __RPC_WRAP_H__
#define __RPC_WRAP_H__

#ifdef __cplusplus
extern "C" {
#endif

/** Includes **/
#include "common.h"
#include "esp_wifi.h"
#include "rpc_types.h"

/** Exported variables **/

/** Inline functions **/

/** Exported Functions **/
esp_err_t rpc_init(void);
esp_err_t rpc_deinit(void);
esp_err_t rpc_unregister_event_callbacks(void);
esp_err_t rpc_register_event_callbacks(void);

esp_err_t rpc_wifi_init(const wifi_init_config_t *arg);
esp_err_t rpc_wifi_deinit(void);
esp_err_t rpc_wifi_set_mode(wifi_mode_t mode);
esp_err_t rpc_wifi_get_mode(wifi_mode_t* mode);
esp_err_t rpc_wifi_start(void);
esp_err_t rpc_wifi_stop(void);
esp_err_t rpc_wifi_connect(void);
esp_err_t rpc_wifi_disconnect(void);
esp_err_t rpc_wifi_set_config(wifi_interface_t interface, wifi_config_t *conf);
esp_err_t rpc_wifi_get_config(wifi_interface_t interface, wifi_config_t *conf);
esp_err_t rpc_wifi_get_mac(wifi_interface_t mode, uint8_t mac[6]);
esp_err_t rpc_wifi_set_mac(wifi_interface_t mode, const uint8_t mac[6]);

esp_err_t rpc_wifi_scan_start(const wifi_scan_config_t *config, bool block);
esp_err_t rpc_wifi_scan_stop(void);
esp_err_t rpc_wifi_scan_get_ap_num(uint16_t *number);
esp_err_t rpc_wifi_scan_get_ap_records(uint16_t *number, wifi_ap_record_t *ap_records);
esp_err_t rpc_wifi_clear_ap_list(void);
esp_err_t rpc_wifi_restore(void);
esp_err_t rpc_wifi_clear_fast_connect(void);
esp_err_t rpc_wifi_deauth_sta(uint16_t aid);
esp_err_t rpc_wifi_sta_get_ap_info(wifi_ap_record_t *ap_info);
esp_err_t rpc_wifi_set_ps(wifi_ps_type_t type);
esp_err_t rpc_wifi_get_ps(wifi_ps_type_t *type);
esp_err_t rpc_wifi_set_storage(wifi_storage_t storage);
esp_err_t rpc_wifi_set_bandwidth(wifi_interface_t ifx, wifi_bandwidth_t bw);
esp_err_t rpc_wifi_get_bandwidth(wifi_interface_t ifx, wifi_bandwidth_t *bw);
esp_err_t rpc_wifi_set_channel(uint8_t primary, wifi_second_chan_t second);
esp_err_t rpc_wifi_get_channel(uint8_t *primary, wifi_second_chan_t *second);
esp_err_t rpc_wifi_set_country_code(const char *country, bool ieee80211d_enabled);
esp_err_t rpc_wifi_get_country_code(char *country);
esp_err_t rpc_wifi_set_country(const wifi_country_t *country);
esp_err_t rpc_wifi_get_country(wifi_country_t *country);
esp_err_t rpc_wifi_ap_get_sta_list(wifi_sta_list_t *sta);
esp_err_t rpc_wifi_ap_get_sta_aid(const uint8_t mac[6], uint16_t *aid);
esp_err_t rpc_wifi_sta_get_rssi(int *rssi);
esp_err_t rpc_wifi_set_protocol(wifi_interface_t ifx, uint8_t protocol_bitmap);
esp_err_t rpc_wifi_get_protocol(wifi_interface_t ifx, uint8_t *protocol_bitmap);
esp_err_t rpc_wifi_set_max_tx_power(int8_t power);
esp_err_t rpc_wifi_get_max_tx_power(int8_t *power);
esp_err_t rpc_ota(const char* image_url);
esp_err_t rpc_set_dhcp_dns_status(wifi_interface_t interface, uint8_t link_up,
		uint8_t dhcp_up, char *dhcp_ip, char *dhcp_nm, char *dhcp_gw,
		uint8_t dns_up, char *dns_ip, uint8_t dns_type);
esp_err_t rpc_send_usr_request(uint8_t usr_req_num, rpc_usr_t *usr_req, rpc_usr_t *usr_resp);
esp_err_t rpc_register_usr_event_callback( void (*usr_evt_cb)(uint8_t , rpc_usr_t*));

#ifdef __cplusplus
}
#endif

#endif
