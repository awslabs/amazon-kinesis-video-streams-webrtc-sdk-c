/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <netdb.h>
#include <memory>
#include <cinttypes>
#include "esp_log.h"
#include "esp_tls.h"
#include "esp_wifi.h"
#include "esp_check.h"
#include "wifi_remote_rpc_impl.hpp"
#include "eppp_link.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "wifi_remote_rpc_params.h"

extern "C" esp_netif_t *wifi_remote_eppp_init(eppp_type_t role);

namespace eppp_rpc {

namespace client {
const char *TAG = "rpc_client";

const unsigned char ca_crt[] = "-----BEGIN CERTIFICATE-----\n" CONFIG_ESP_WIFI_REMOTE_EPPP_SERVER_CA "\n-----END CERTIFICATE-----";
const unsigned char crt[] = "-----BEGIN CERTIFICATE-----\n" CONFIG_ESP_WIFI_REMOTE_EPPP_CLIENT_CRT "\n-----END CERTIFICATE-----";
const unsigned char key[] = "-----BEGIN PRIVATE KEY-----\n" CONFIG_ESP_WIFI_REMOTE_EPPP_CLIENT_KEY "\n-----END PRIVATE KEY-----";
// TODO: Add option to supply keys and certs via a global symbol (file)

}

using namespace client;

class Sync {
    friend class RpcInstance;
public:
    void lock()
    {
        xSemaphoreTake(mutex, portMAX_DELAY);
    }
    void unlock()
    {
        xSemaphoreGive(mutex);
    }
    esp_err_t init()
    {
        mutex = xSemaphoreCreateMutex();
        events = xEventGroupCreate();
        return mutex == nullptr || events == nullptr ? ESP_ERR_NO_MEM : ESP_OK;
    }
    esp_err_t wait_for(EventBits_t bits, uint32_t timeout = portMAX_DELAY)
    {
        return (xEventGroupWaitBits(events, bits, pdTRUE, pdTRUE, timeout) & bits) == bits ? ESP_OK : ESP_FAIL;
    }
    esp_err_t notify(EventBits_t bits)
    {
        xEventGroupSetBits(events, bits);
        return ESP_OK;
    }
    ~Sync()
    {
        if (mutex) {
            vSemaphoreDelete(mutex);
        }
        if (events) {
            vEventGroupDelete(events);
        }
    }


private:
    SemaphoreHandle_t mutex{nullptr};
    EventGroupHandle_t events{nullptr};

    const int request = 1;
    const int resp_header = 2;
    const int resp_payload = 4;
    const int restart = 8;
};

class RpcInstance {
    friend class Sync;
public:

    template<typename T>
    esp_err_t send(api_id id, T *t)
    {
        pending_resp = id;
        ESP_RETURN_ON_ERROR(sync.notify(sync.request), TAG, "failed to notify req");
        ESP_RETURN_ON_ERROR(rpc.send<T>(id, t), TAG, "Failed to send request");
        return ESP_OK;
    }

    // overload of the templated method (used for functions with no arguments)
    esp_err_t send(api_id id)
    {
        pending_resp = id;
        ESP_RETURN_ON_ERROR(sync.notify(sync.request), TAG, "failed to notify req");
        ESP_RETURN_ON_ERROR(rpc.send(id), TAG, "Failed to send request");
        return ESP_OK;
    }

    template<typename T>
    T get_resp(api_id id)
    {
        sync.wait_for(sync.resp_header);
        auto ret = rpc.template get_payload<T>(id, pending_header);
        sync.notify(sync.resp_payload);
        return ret;
    }
    esp_err_t init()
    {
        ESP_RETURN_ON_FALSE(netif = wifi_remote_eppp_init(EPPP_CLIENT), ESP_FAIL, TAG, "Failed to connect to EPPP server");
        ESP_RETURN_ON_ERROR(esp_event_handler_register(IP_EVENT, IP_EVENT_PPP_GOT_IP, got_ip, this), TAG, "Failed to register event");
        ESP_RETURN_ON_ERROR(sync.init(), TAG, "Failed to init sync primitives");
        ESP_RETURN_ON_ERROR(rpc.init(), TAG, "Failed to init RPC engine");
        return xTaskCreate(task, "client", 8192, this, 5, nullptr) == pdTRUE ? ESP_OK : ESP_FAIL;
    }
    RpcEngine rpc{eppp_rpc::role::CLIENT};
    Sync sync;
private:
    api_id pending_resp{api_id::UNDEF};
    RpcHeader pending_header{};
    esp_err_t process_ip_event(RpcHeader &header)
    {
        auto event = rpc.get_payload<esp_wifi_remote_eppp_ip_event>(api_id::IP_EVENT, header);
        // Now bypass network layers with EPPP interface
        ESP_RETURN_ON_ERROR(esp_netif_set_dns_info(netif, ESP_NETIF_DNS_MAIN, &event.dns), TAG, "Failed to set DNS info");
        ESP_RETURN_ON_ERROR(esp_netif_set_default_netif(netif), TAG, "Failed to set default netif to EPPP");
        ip_event_got_ip_t evt = {
            .esp_netif = netif,
            .ip_info = {},
            .ip_changed = true,
        };
        esp_netif_get_ip_info(netif, &evt.ip_info);
        ESP_RETURN_ON_ERROR(esp_event_post(IP_EVENT, IP_EVENT_STA_GOT_IP, &evt, sizeof(evt), 0), TAG, "Failed to post IP event");
        ESP_LOGI(TAG, "Main DNS:" IPSTR, IP2STR(&event.dns.ip.u_addr.ip4));
        ESP_LOGI(TAG, "EPPP IP:" IPSTR, IP2STR(&event.ppp_ip.ip));
        ESP_LOGI(TAG, "WIFI IP:" IPSTR, IP2STR(&event.wifi_ip.ip));
        ESP_LOGI(TAG, "WIFI GW:" IPSTR, IP2STR(&event.wifi_ip.gw));
        ESP_LOGI(TAG, "WIFI mask:" IPSTR, IP2STR(&event.wifi_ip.netmask));
        return ESP_OK;
    }
    esp_err_t process_wifi_event(RpcHeader &header)
    {
        auto event_id = rpc.get_payload<int32_t>(api_id::WIFI_EVENT, header);
        ESP_RETURN_ON_ERROR(esp_event_post(WIFI_EVENT, event_id, nullptr, 0, 0), TAG, "Failed to post WiFi event");
        return ESP_OK;
    }
    esp_err_t perform()
    {
        auto header = rpc.get_header();
        if (api_id(header.id) == api_id::ERROR) {   // network error
            return ESP_FAIL;
        }
        if (api_id(header.id) == api_id::UNDEF) {   // network timeout
            return ESP_OK;
        }

        if (api_id(header.id) == api_id::IP_EVENT) {
            return process_ip_event(header);
        }
        if (api_id(header.id) == api_id::WIFI_EVENT) {
            return process_wifi_event(header);
        }
        if (sync.wait_for(sync.request, 0) == ESP_OK && api_id(header.id) == pending_resp) {
            pending_header = header;
            pending_resp = api_id::UNDEF;
            sync.notify(sync.resp_header);
            sync.wait_for(sync.resp_payload);
            return ESP_OK;
        }
        ESP_LOGE(TAG, "Unexpected header %" PRIi32, static_cast<uint32_t>(header.id));
        return ESP_FAIL;

    }
    static void task(void *ctx)
    {
        auto instance = static_cast<RpcInstance *>(ctx);
        do {
            while (instance->perform() == ESP_OK) {}
        } while (instance->restart() == ESP_OK);
        vTaskDelete(nullptr);
    }
    esp_err_t restart()
    {
        rpc.deinit();
        ESP_RETURN_ON_ERROR(sync.wait_for(sync.restart, pdMS_TO_TICKS(10000)), TAG, "Didn't receive EPPP address in time");
        return rpc.init();
    }
    static void got_ip(void *ctx, esp_event_base_t base, int32_t id, void *data)
    {
        auto instance = static_cast<RpcInstance *>(ctx);
        instance->sync.notify(instance->sync.restart);
    }
    esp_netif_t *netif{nullptr};
};


namespace client {
constinit RpcInstance instance;
}   // namespace client

RpcInstance *RpcEngine::init_client()
{
    char host[4 * 4 + 1] = {}; // IPv4: 4 x (3 numbers + '.') + \0
    esp_ip4_addr_t ip = { .addr = EPPP_DEFAULT_SERVER_IP() };
    if (esp_ip4addr_ntoa(&ip, host, sizeof(host)) == nullptr) {
        return nullptr;
    }

    esp_tls_cfg_t cfg = {};
    cfg.cacert_buf = client::ca_crt;
    cfg.cacert_bytes = sizeof(client::ca_crt);
    cfg.clientcert_buf = client::crt;
    cfg.clientcert_bytes = sizeof(client::crt);
    cfg.clientkey_buf = client::key;
    cfg.clientkey_bytes = sizeof(client::key);
    cfg.common_name = "espressif.local";

    ESP_RETURN_ON_FALSE(tls_ = esp_tls_init(), nullptr, TAG, "Failed to create ESP-TLS instance");
    int retries = 0;
    while (esp_tls_conn_new_sync(host, strlen(host), rpc_port, &cfg, tls_) <= 0) {
        esp_tls_conn_destroy(tls_);
        tls_ = nullptr;
        ESP_RETURN_ON_FALSE(retries++ < 3, nullptr, TAG, "Failed to open connection to %s", host);
        ESP_LOGW(TAG, "Connection to RPC server failed! Will retry in %d second(s)", retries);
        vTaskDelay(pdMS_TO_TICKS(1000 * retries));
        ESP_RETURN_ON_FALSE(tls_ = esp_tls_init(), nullptr, TAG, "Failed to create ESP-TLS instance");
    }
    return &client::instance;
}
}   // namespace eppp_rpc

//
//  esp_wifi_remote API implementation
//
using namespace eppp_rpc;
using namespace client;

extern "C" esp_err_t esp_wifi_remote_init(const wifi_init_config_t *config)
{
    // Here we initialize this client's RPC
    ESP_RETURN_ON_ERROR(instance.init(), TAG, "Failed to initialize eppp-rpc");

    std::lock_guard<Sync> lock(instance.sync);
    ESP_RETURN_ON_ERROR(instance.send(api_id::INIT, config), TAG, "Failed to send request");
    return instance.get_resp<esp_err_t>(api_id::INIT);
}

extern "C" esp_err_t esp_wifi_remote_set_config(wifi_interface_t interface, wifi_config_t *conf)
{
    esp_wifi_remote_config params = { .interface = interface, .conf = {} };
    memcpy(&params.conf, conf, sizeof(wifi_config_t));
    std::lock_guard<Sync> lock(instance.sync);
    ESP_RETURN_ON_ERROR(instance.send(api_id::SET_CONFIG, &params), TAG, "Failed to send request");
    return instance.get_resp<esp_err_t>(api_id::SET_CONFIG);
}

extern "C" esp_err_t esp_wifi_remote_start(void)
{
    std::lock_guard<Sync> lock(instance.sync);
    ESP_RETURN_ON_ERROR(instance.send(api_id::START), TAG, "Failed to send request");
    return instance.get_resp<esp_err_t>(api_id::START);
}

extern "C" esp_err_t esp_wifi_remote_stop(void)
{
    std::lock_guard<Sync> lock(instance.sync);
    ESP_RETURN_ON_ERROR(instance.send(api_id::STOP), TAG, "Failed to send request");
    return instance.get_resp<esp_err_t>(api_id::STOP);
}

extern "C" esp_err_t esp_wifi_remote_connect(void)
{
    std::lock_guard<Sync> lock(instance.sync);
    ESP_RETURN_ON_ERROR(instance.send(api_id::CONNECT), TAG, "Failed to send request");
    return instance.get_resp<esp_err_t>(api_id::CONNECT);
}

extern "C" esp_err_t esp_wifi_remote_get_mac(wifi_interface_t ifx, uint8_t mac[6])
{
    std::lock_guard<Sync> lock(instance.sync);
    ESP_RETURN_ON_ERROR(instance.send(api_id::GET_MAC, &ifx), TAG, "Failed to send request");
    auto ret = instance.get_resp<esp_wifi_remote_mac_t>(api_id::GET_MAC);
    ESP_LOG_BUFFER_HEXDUMP("MAC", ret.mac, 6, ESP_LOG_DEBUG);
    memcpy(mac, ret.mac, 6);
    return ret.err;
}

extern "C" esp_err_t esp_wifi_remote_set_mode(wifi_mode_t mode)
{
    std::lock_guard<Sync> lock(instance.sync);
    ESP_RETURN_ON_ERROR(instance.send(api_id::SET_MODE, &mode), TAG, "Failed to send request");
    return instance.get_resp<esp_err_t>(api_id::SET_MODE);
}

extern "C" esp_err_t esp_wifi_remote_deinit(void)
{
    std::lock_guard<Sync> lock(instance.sync);
    ESP_RETURN_ON_ERROR(instance.send(api_id::DEINIT), TAG, "Failed to send request");
    return instance.get_resp<esp_err_t>(api_id::DEINIT);
}

extern "C" esp_err_t esp_wifi_remote_disconnect(void)
{
    std::lock_guard<Sync> lock(instance.sync);
    ESP_RETURN_ON_ERROR(instance.send(api_id::DISCONNECT), TAG, "Failed to send request");
    return instance.get_resp<esp_err_t>(api_id::DISCONNECT);
}

extern "C" esp_err_t esp_wifi_remote_set_storage(wifi_storage_t storage)
{
    std::lock_guard<Sync> lock(instance.sync);
    ESP_RETURN_ON_ERROR(instance.send(api_id::SET_STORAGE, &storage), TAG, "Failed to send request");
    return instance.get_resp<esp_err_t>(api_id::SET_STORAGE);
}
