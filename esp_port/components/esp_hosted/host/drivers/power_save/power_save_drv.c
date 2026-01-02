// SPDX-License-Identifier: Apache-2.0
// Copyright 2015-2023 Espressif Systems (Shanghai) PTE LTD
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

/** Includes **/
#include "esp_log.h"
#include "esp_hosted_log.h"
#include "power_save_drv.h"
#include "sdio_drv.h"
#include "stats.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "esp_hosted_config.h"

static const char TAG[] = "H_power_save";

static uint8_t power_save_on;
static int reboot_reason;

static uint8_t power_save_drv_init_done;

/* Add state tracking */
static volatile bool reset_in_progress = false;

/* ISR handler for wakeup GPIO */
static void IRAM_ATTR wakeup_gpio_isr_handler(void* arg)
{
#ifdef H_SDIO_DISABLE_GPIO_BASED_SLAVE_RESET
    if (!power_save_on && !reset_in_progress) {

        int current_level = gpio_get_level(H_HOST_WAKEUP_GPIO);

        /* Double check GPIO level and state before reset */
        if (current_level == H_HOST_WAKEUP_GPIO_LEVEL) {
            ESP_EARLY_LOGW(TAG, "Slave reset detected via wakeup GPIO, level: %d", current_level);

            /* Set flag to prevent re-entry */
            reset_in_progress = true;

            /* Disable interrupt and remove handler before reset */
            gpio_intr_disable(H_HOST_WAKEUP_GPIO);
            gpio_isr_handler_remove(H_HOST_WAKEUP_GPIO);

            /* Force power save off and trigger reset */
            esp_unregister_shutdown_handler((shutdown_handler_t)esp_wifi_stop);
            esp_restart();
        }
    }
#else
    ESP_EARLY_LOGW(TAG, "Slave reset detected, ignored as of now");
#endif
}

/* Initialize power save driver and configure GPIO for slave reset detection */
void power_save_drv_init(uint32_t gpio_num)
{
    ESP_LOGI(TAG, "power_save_drv_init with gpio_num: %" PRIu32, gpio_num);

    if (power_save_drv_init_done) {
        ESP_LOGI(TAG, "Power save driver already initialized");
        return;
    }

#if H_HOST_PS_ALLOWED
#if H_HOST_WAKEUP_USING_RTC_GPIO && H_HOST_WAKEUP_GPIO
    esp_err_t ret;

    /* Reset state flags */
    power_save_on = 0;
    reset_in_progress = false;

    /* Configure GPIO with interrupt disabled initially */
    const gpio_config_t config = {
        .pin_bit_mask = BIT(gpio_num),
        .mode = GPIO_MODE_INPUT,
        .intr_type = GPIO_INTR_DISABLE,  // Start with interrupts disabled
        .pull_down_en = 1,
        .pull_up_en = 0
    };

    ret = gpio_config(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure GPIO %" PRIu32 ", err %d", gpio_num, ret);
        return;
    }

    int initial_level = gpio_get_level(gpio_num);
    ESP_LOGI(TAG, "Initial GPIO level: %d", initial_level);

    /* Install ISR service if needed */
    ret = gpio_install_isr_service(0);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to install GPIO ISR service, err %d", ret);
        return;
    }

    /* Only proceed with ISR setup if conditions are right */
    if (!power_save_on && initial_level == 0) {
        /* First remove any existing handler */
        gpio_isr_handler_remove(gpio_num);

        /* Add ISR handler */
        ret = gpio_isr_handler_add(gpio_num, wakeup_gpio_isr_handler, NULL);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to add GPIO ISR handler, err %d", ret);
            return;
        }

        /* Now enable the interrupt type */
        ret = gpio_set_intr_type(gpio_num, H_HOST_WAKEUP_GPIO_LEVEL);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set GPIO interrupt type, err %d", ret);
            gpio_isr_handler_remove(gpio_num);
            return;
        }

        /* Finally enable the interrupt */
        gpio_intr_enable(gpio_num);
    }

    ESP_LOGI(TAG, "Initialized wakeup/reset GPIO %" PRIu32 " for slave reset detection", gpio_num);
#endif
    power_save_drv_init_done = 1;
#endif
}


/* Add this new function to safely disable wakeup detection */
void power_save_drv_disable_wakeup(void)
{
#if H_HOST_PS_ALLOWED && H_HOST_WAKEUP_USING_RTC_GPIO && H_HOST_WAKEUP_GPIO
    gpio_intr_disable(H_HOST_WAKEUP_GPIO);
    gpio_isr_handler_remove(H_HOST_WAKEUP_GPIO);
#endif
}

int is_feature_enabled_host_power_save(void)
{
#if H_HOST_PS_ALLOWED
	return 1;
#endif
	return 0;
}


int is_host_reboot_due_to_deep_sleep(void)
{
#if H_HOST_PS_ALLOWED
    if ((reboot_reason == ESP_SLEEP_WAKEUP_GPIO) ||
	    (reboot_reason == ESP_SLEEP_WAKEUP_EXT1)) {
		ESP_LOGI(TAG, "Wakeup using GPIO");
        return 1;
	} else if (reboot_reason == ESP_SLEEP_WAKEUP_TIMER) {
		ESP_LOGI(TAG, "Wakeup using Timer");
        return 1;
	}
#endif
    return 0;
}

int set_host_wakeup_reason(int reason)
{
#if H_HOST_PS_ALLOWED
	ESP_LOGE(TAG, "reboot_reason set to: %d", reason);
	reboot_reason = reason;
	return 0;
#endif
	return -1;
}

int is_host_power_saving(void)
{
#if H_HOST_PS_ALLOWED
	return power_save_on;
#else
	return 0;
#endif
}

#if H_HOST_PS_ALLOWED
#if H_HOST_WAKEUP_USING_RTC_GPIO && H_HOST_WAKEUP_GPIO
static void enable_deep_sleep_register_gpio_wakeup(void)
{
    /* Only configure for deep sleep wakeup, interrupt already configured */
    if (!esp_sleep_is_valid_wakeup_gpio(H_HOST_WAKEUP_GPIO)) {
        ESP_LOGE(TAG, "GPIO %d is not an RTC IO", H_HOST_WAKEUP_GPIO);
        return;
    }
    ESP_ERROR_CHECK(esp_deep_sleep_enable_gpio_wakeup(BIT(H_HOST_WAKEUP_GPIO), H_HOST_WAKEUP_GPIO_LEVEL));
    ESP_LOGW(TAG, "Enabling GPIO %d for deep sleep wakeup", H_HOST_WAKEUP_GPIO);
}
#endif
#if H_HOST_WAKEUP_USING_RTC_TIMER
static void enable_deep_sleep_register_rtc_timer_wakeup(void)
{
    const int wakeup_time_sec = 10;
    ESP_LOGW(TAG, "Enabling timer wakeup, %ds", wakeup_time_sec);
    ESP_ERROR_CHECK(esp_sleep_enable_timer_wakeup(wakeup_time_sec * 1000000));
}
#endif
#endif

int start_host_power_save(void)
{
    if (!power_save_drv_init_done) {
        ESP_LOGE(TAG, "Power save driver not initialized, Rejecting deep sleep request");
        return -1;
    }

#if H_HOST_PS_ALLOWED
    /* Disable interrupts and remove handler before sleep */
    power_save_drv_disable_wakeup();

    /* Set power save flag after disabling interrupts */
    power_save_on = 1;

#if H_HOST_WAKEUP_USING_RTC_GPIO
    enable_deep_sleep_register_gpio_wakeup();
#else
    enable_deep_sleep_register_rtc_timer_wakeup();
#endif

    /* Notify slave before going to sleep */
    send_slave_power_save(1);

    /* Double check state before sleep */
    if (!reset_in_progress) {
        esp_deep_sleep_start();
    } else {
        ESP_LOGE(TAG, "Reset in progress, aborting sleep");
        return -1;
    }

    ESP_LOGE(TAG, "This log should never be printed");
    return 0;
#endif
    return -1;
}

#if H_HOST_PS_ALLOWED
static esp_timer_handle_t timer_handle = NULL;

static void deep_sleep_timer_callback(void *arg)
{
	start_host_power_save();
}
#endif
int host_power_save_timer_start(uint32_t time_ms)
{
    esp_err_t err = ESP_OK;
#if H_HOST_PS_ALLOWED
    if (!timer_handle) {
        esp_timer_create_args_t timer_create_args = {
            .callback = &deep_sleep_timer_callback,
            .arg = NULL,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "deep_sleep_timer",
            .skip_unhandled_events = true,
        };
        err = esp_timer_create(&timer_create_args, &timer_handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to create timer: %s", esp_err_to_name(err));
            return err; // early return
        }
    }

    if (esp_timer_is_active(timer_handle)) {
        err = esp_timer_restart(timer_handle, time_ms * 1000);
    } else {
        err = esp_timer_start_once(timer_handle, time_ms * 1000);
    }

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start timer: %s", esp_err_to_name(err));
    }
#endif
    return err;
}

int host_power_save_timer_stop(void)
{
    esp_err_t err = ESP_OK;
#if H_HOST_PS_ALLOWED
    if (!timer_handle) {
        ESP_LOGW(TAG, "No timer exists");
        return -1;
    }
    err = esp_timer_stop(timer_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to stop timer: %s", esp_err_to_name(err));
    }
#endif
    return err;
}

int host_power_save_timer_restart(uint32_t time_ms)
{
    ESP_LOGW(TAG, "Not supported yet!");
    return -1;
}
