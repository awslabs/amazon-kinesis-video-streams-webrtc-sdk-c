/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "esp_hosted_wifi_api.h"

static inline esp_err_t esp_wifi_remote_init(const wifi_init_config_t *config)
{
    return esp_hosted_wifi_init(config);
}

static inline esp_err_t esp_wifi_remote_deinit(void)
{
    return esp_hosted_wifi_deinit();
}

static inline esp_err_t esp_wifi_remote_set_mode(wifi_mode_t mode)
{
    return esp_hosted_wifi_set_mode(mode);
}

static inline esp_err_t esp_wifi_remote_get_mode(wifi_mode_t *mode)
{
    return esp_hosted_wifi_get_mode(mode);
}

static inline esp_err_t esp_wifi_remote_start(void)
{
    return esp_hosted_wifi_start();
}

static inline esp_err_t esp_wifi_remote_stop(void)
{
    return esp_hosted_wifi_stop();
}

static inline esp_err_t esp_wifi_remote_restore(void)
{
    return esp_hosted_wifi_restore();
}

static inline esp_err_t esp_wifi_remote_connect(void)
{
    return esp_hosted_wifi_connect();
}

static inline esp_err_t esp_wifi_remote_disconnect(void)
{
    return esp_hosted_wifi_disconnect();
}

static inline esp_err_t esp_wifi_remote_clear_fast_connect(void)
{
    return esp_hosted_wifi_clear_fast_connect();
}

static inline esp_err_t esp_wifi_remote_deauth_sta(uint16_t aid)
{
    return esp_hosted_wifi_deauth_sta(aid);
}

static inline esp_err_t esp_wifi_remote_scan_start(const wifi_scan_config_t *config, _Bool block)
{
    return esp_hosted_wifi_scan_start(config, block);
}

static inline esp_err_t esp_wifi_remote_scan_stop(void)
{
    return esp_hosted_wifi_scan_stop();
}

static inline esp_err_t esp_wifi_remote_scan_get_ap_num(uint16_t *number)
{
    return esp_hosted_wifi_scan_get_ap_num(number);
}

static inline esp_err_t esp_wifi_remote_scan_get_ap_records(uint16_t *number, wifi_ap_record_t *ap_records)
{
    return esp_hosted_wifi_scan_get_ap_records(number, ap_records);
}

static inline esp_err_t esp_wifi_remote_scan_get_ap_record(wifi_ap_record_t *ap_record)
{
    return esp_hosted_wifi_scan_get_ap_record(ap_record);
}

static inline esp_err_t esp_wifi_remote_clear_ap_list(void)
{
    return esp_hosted_wifi_clear_ap_list();
}

static inline esp_err_t esp_wifi_remote_sta_get_ap_info(wifi_ap_record_t *ap_info)
{
    return esp_hosted_wifi_sta_get_ap_info(ap_info);
}

static inline esp_err_t esp_wifi_remote_set_ps(wifi_ps_type_t type)
{
    return esp_hosted_wifi_set_ps(type);
}

static inline esp_err_t esp_wifi_remote_get_ps(wifi_ps_type_t *type)
{
    return esp_hosted_wifi_get_ps(type);
}

static inline esp_err_t esp_wifi_remote_set_protocol(wifi_interface_t ifx, uint8_t protocol_bitmap)
{
    return esp_hosted_wifi_set_protocol(ifx, protocol_bitmap);
}

static inline esp_err_t esp_wifi_remote_get_protocol(wifi_interface_t ifx, uint8_t *protocol_bitmap)
{
    return esp_hosted_wifi_get_protocol(ifx, protocol_bitmap);
}

static inline esp_err_t esp_wifi_remote_set_bandwidth(wifi_interface_t ifx, wifi_bandwidth_t bw)
{
    return esp_hosted_wifi_set_bandwidth(ifx, bw);
}

static inline esp_err_t esp_wifi_remote_get_bandwidth(wifi_interface_t ifx, wifi_bandwidth_t *bw)
{
    return esp_hosted_wifi_get_bandwidth(ifx, bw);
}

static inline esp_err_t esp_wifi_remote_set_channel(uint8_t primary, wifi_second_chan_t second)
{
    return esp_hosted_wifi_set_channel(primary, second);
}

static inline esp_err_t esp_wifi_remote_get_channel(uint8_t *primary, wifi_second_chan_t *second)
{
    return esp_hosted_wifi_get_channel(primary, second);
}

static inline esp_err_t esp_wifi_remote_set_country(const wifi_country_t *country)
{
    return esp_hosted_wifi_set_country(country);
}

static inline esp_err_t esp_wifi_remote_get_country(wifi_country_t *country)
{
    return esp_hosted_wifi_get_country(country);
}

static inline esp_err_t esp_wifi_remote_set_mac(wifi_interface_t ifx, const uint8_t mac[6])
{
    return esp_hosted_wifi_set_mac(ifx, mac);
}

static inline esp_err_t esp_wifi_remote_get_mac(wifi_interface_t ifx, uint8_t mac[6])
{
    return esp_hosted_wifi_get_mac(ifx, mac);
}

static inline esp_err_t esp_wifi_remote_set_promiscuous_rx_cb(wifi_promiscuous_cb_t cb)
{
    return esp_hosted_wifi_set_promiscuous_rx_cb(cb);
}

static inline esp_err_t esp_wifi_remote_set_promiscuous(_Bool en)
{
    return esp_hosted_wifi_set_promiscuous(en);
}

static inline esp_err_t esp_wifi_remote_get_promiscuous(_Bool *en)
{
    return esp_hosted_wifi_get_promiscuous(en);
}

static inline esp_err_t esp_wifi_remote_set_promiscuous_filter(const wifi_promiscuous_filter_t *filter)
{
    return esp_hosted_wifi_set_promiscuous_filter(filter);
}

static inline esp_err_t esp_wifi_remote_get_promiscuous_filter(wifi_promiscuous_filter_t *filter)
{
    return esp_hosted_wifi_get_promiscuous_filter(filter);
}

static inline esp_err_t esp_wifi_remote_set_promiscuous_ctrl_filter(const wifi_promiscuous_filter_t *filter)
{
    return esp_hosted_wifi_set_promiscuous_ctrl_filter(filter);
}

static inline esp_err_t esp_wifi_remote_get_promiscuous_ctrl_filter(wifi_promiscuous_filter_t *filter)
{
    return esp_hosted_wifi_get_promiscuous_ctrl_filter(filter);
}

static inline esp_err_t esp_wifi_remote_set_config(wifi_interface_t interface, wifi_config_t *conf)
{
    return esp_hosted_wifi_set_config(interface, conf);
}

static inline esp_err_t esp_wifi_remote_get_config(wifi_interface_t interface, wifi_config_t *conf)
{
    return esp_hosted_wifi_get_config(interface, conf);
}

static inline esp_err_t esp_wifi_remote_ap_get_sta_list(wifi_sta_list_t *sta)
{
    return esp_hosted_wifi_ap_get_sta_list(sta);
}

static inline esp_err_t esp_wifi_remote_ap_get_sta_aid(const uint8_t mac[6], uint16_t *aid)
{
    return esp_hosted_wifi_ap_get_sta_aid(mac, aid);
}

static inline esp_err_t esp_wifi_remote_set_storage(wifi_storage_t storage)
{
    return esp_hosted_wifi_set_storage(storage);
}

static inline esp_err_t esp_wifi_remote_set_vendor_ie(_Bool enable, wifi_vendor_ie_type_t type, wifi_vendor_ie_id_t idx, const void *vnd_ie)
{
    return esp_hosted_wifi_set_vendor_ie(enable, type, idx, vnd_ie);
}

static inline esp_err_t esp_wifi_remote_set_vendor_ie_cb(esp_vendor_ie_cb_t cb, void *ctx)
{
    return esp_hosted_wifi_set_vendor_ie_cb(cb, ctx);
}

static inline esp_err_t esp_wifi_remote_set_max_tx_power(int8_t power)
{
    return esp_hosted_wifi_set_max_tx_power(power);
}

static inline esp_err_t esp_wifi_remote_get_max_tx_power(int8_t *power)
{
    return esp_hosted_wifi_get_max_tx_power(power);
}

static inline esp_err_t esp_wifi_remote_set_event_mask(uint32_t mask)
{
    return esp_hosted_wifi_set_event_mask(mask);
}

static inline esp_err_t esp_wifi_remote_get_event_mask(uint32_t *mask)
{
    return esp_hosted_wifi_get_event_mask(mask);
}

static inline esp_err_t esp_wifi_remote_80211_tx(wifi_interface_t ifx, const void *buffer, int len, _Bool en_sys_seq)
{
    return esp_hosted_wifi_80211_tx(ifx, buffer, len, en_sys_seq);
}

static inline esp_err_t esp_wifi_remote_set_csi_rx_cb(wifi_csi_cb_t cb, void *ctx)
{
    return esp_hosted_wifi_set_csi_rx_cb(cb, ctx);
}

static inline esp_err_t esp_wifi_remote_set_csi_config(const wifi_csi_config_t *config)
{
    return esp_hosted_wifi_set_csi_config(config);
}

static inline esp_err_t esp_wifi_remote_set_csi(_Bool en)
{
    return esp_hosted_wifi_set_csi(en);
}

static inline esp_err_t esp_wifi_remote_set_ant_gpio(const wifi_ant_gpio_config_t *config)
{
    return esp_hosted_wifi_set_ant_gpio(config);
}

static inline esp_err_t esp_wifi_remote_get_ant_gpio(wifi_ant_gpio_config_t *config)
{
    return esp_hosted_wifi_get_ant_gpio(config);
}

static inline esp_err_t esp_wifi_remote_set_ant(const wifi_ant_config_t *config)
{
    return esp_hosted_wifi_set_ant(config);
}

static inline esp_err_t esp_wifi_remote_get_ant(wifi_ant_config_t *config)
{
    return esp_hosted_wifi_get_ant(config);
}

static inline int64_t esp_wifi_remote_get_tsf_time(wifi_interface_t interface)
{
    return esp_hosted_wifi_get_tsf_time(interface);
}

static inline esp_err_t esp_wifi_remote_set_inactive_time(wifi_interface_t ifx, uint16_t sec)
{
    return esp_hosted_wifi_set_inactive_time(ifx, sec);
}

static inline esp_err_t esp_wifi_remote_get_inactive_time(wifi_interface_t ifx, uint16_t *sec)
{
    return esp_hosted_wifi_get_inactive_time(ifx, sec);
}

static inline esp_err_t esp_wifi_remote_statis_dump(uint32_t modules)
{
    return esp_hosted_wifi_statis_dump(modules);
}

static inline esp_err_t esp_wifi_remote_set_rssi_threshold(int32_t rssi)
{
    return esp_hosted_wifi_set_rssi_threshold(rssi);
}

static inline esp_err_t esp_wifi_remote_ftm_initiate_session(wifi_ftm_initiator_cfg_t *cfg)
{
    return esp_hosted_wifi_ftm_initiate_session(cfg);
}

static inline esp_err_t esp_wifi_remote_ftm_end_session(void)
{
    return esp_hosted_wifi_ftm_end_session();
}

static inline esp_err_t esp_wifi_remote_ftm_resp_set_offset(int16_t offset_cm)
{
    return esp_hosted_wifi_ftm_resp_set_offset(offset_cm);
}

static inline esp_err_t esp_wifi_remote_config_11b_rate(wifi_interface_t ifx, _Bool disable)
{
    return esp_hosted_wifi_config_11b_rate(ifx, disable);
}

static inline esp_err_t esp_wifi_remote_connectionless_module_set_wake_interval(uint16_t wake_interval)
{
    return esp_hosted_wifi_connectionless_module_set_wake_interval(wake_interval);
}

static inline esp_err_t esp_wifi_remote_force_wakeup_acquire(void)
{
    return esp_hosted_wifi_force_wakeup_acquire();
}

static inline esp_err_t esp_wifi_remote_force_wakeup_release(void)
{
    return esp_hosted_wifi_force_wakeup_release();
}

static inline esp_err_t esp_wifi_remote_set_country_code(const char *country, _Bool ieee80211d_enabled)
{
    return esp_hosted_wifi_set_country_code(country, ieee80211d_enabled);
}

static inline esp_err_t esp_wifi_remote_get_country_code(char *country)
{
    return esp_hosted_wifi_get_country_code(country);
}

static inline esp_err_t esp_wifi_remote_config_80211_tx_rate(wifi_interface_t ifx, wifi_phy_rate_t rate)
{
    return esp_hosted_wifi_config_80211_tx_rate(ifx, rate);
}

static inline esp_err_t esp_wifi_remote_disable_pmf_config(wifi_interface_t ifx)
{
    return esp_hosted_wifi_disable_pmf_config(ifx);
}

static inline esp_err_t esp_wifi_remote_sta_get_aid(uint16_t *aid)
{
    return esp_hosted_wifi_sta_get_aid(aid);
}

static inline esp_err_t esp_wifi_remote_sta_get_negotiated_phymode(wifi_phy_mode_t *phymode)
{
    return esp_hosted_wifi_sta_get_negotiated_phymode(phymode);
}

static inline esp_err_t esp_wifi_remote_set_dynamic_cs(_Bool enabled)
{
    return esp_hosted_wifi_set_dynamic_cs(enabled);
}

static inline esp_err_t esp_wifi_remote_sta_get_rssi(int *rssi)
{
    return esp_hosted_wifi_sta_get_rssi(rssi);
}
