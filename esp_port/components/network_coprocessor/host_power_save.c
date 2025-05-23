/*
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "host_power_save.h"
#include "adapter.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include <string.h>

#include "esp_timer.h"

static char *TAG = "host_ps";

#if HOST_PS_ALLOWED
  SemaphoreHandle_t wakeup_sem;

  volatile uint8_t power_save_on;
  #define GPIO_HOST_WAKEUP (CONFIG_HOST_WAKEUP_GPIO)

  /* Assuming wakup gpio neg 'level' interrupt */
  #define set_host_wakeup_gpio() gpio_set_level(GPIO_HOST_WAKEUP, 1)
  #define reset_host_wakeup_gpio() gpio_set_level(GPIO_HOST_WAKEUP, 0)

  static void oobTimerCallback( TimerHandle_t xTimer );
  static void (*host_wakeup_cb)(void);
#endif


int is_host_power_saving(void)
{
#if HOST_PS_ALLOWED
	return power_save_on;
#else
	return 0;
#endif
}

int is_host_wakeup_needed(interface_buffer_handle_t *buf_handle)
{
	int wakup_needed = 0;
	char reason[100] = "";
#if HOST_PS_ALLOWED
    uint8_t *buf_start;

    buf_start = buf_handle->payload;

	/* Flow conttrol packet cannot miss */
	if (buf_handle->flow_ctl_en) {
		strlcpy(reason, "flow_ctl_pkt", sizeof(reason));
		wakup_needed = 1;
		goto end;
	}

    if (!buf_start) {
        /* Do not wake up */
		strlcpy(reason, "NULL_TxBuff", sizeof(reason));
		wakup_needed = 0;
		goto end;
    }

	/* Wake up for serial msg */
	switch (buf_handle->if_type) {

		case ESP_SERIAL_IF:
			  strlcpy(reason, "serial tx msg", sizeof(reason));
			  wakup_needed = 1;
			  goto end;
			  break;

		case ESP_HCI_IF:
			  strlcpy(reason, "bt tx msg", sizeof(reason));
			  wakup_needed = 1;
			  goto end;
			  break;

		case ESP_PRIV_IF:
			  strlcpy(reason, "priv tx msg", sizeof(reason));
			  wakup_needed = 1;
			  goto end;
			  break;

		case ESP_TEST_IF:
			  strlcpy(reason, "test tx msg", sizeof(reason));
			  wakup_needed = 1;
			  goto end;
			  break;

		case ESP_STA_IF:

			  /* TODO: parse packet if lwip split not configured.
			   * Decide if packets need to reach to host or not
			   **/
			  strlcpy(reason, "sta tx msg", sizeof(reason));
			  wakup_needed = 1;
			  goto end;
			  break;

		case ESP_AP_IF:
			  strlcpy(reason, "ap tx msg", sizeof(reason));
			  wakup_needed = 1;
			  goto end;
			  break;
	}

#else
	strlcpy(reason, "host_ps_disabled", sizeof(reason));
	wakup_needed = 0;
#endif

end:
	if (wakup_needed) {
		ESP_LOGI(TAG, "Wakeup needed, reason %s", reason);
	} else {
		ESP_LOGI(TAG, "Wakeup not needed");
	}
	return wakup_needed;
}

void host_power_save_init(void (*fn_host_wakeup_cb)(void))
{
#if HOST_PS_ALLOWED
	assert(GPIO_HOST_WAKEUP != -1);
	/* Configuration for the OOB line */
	gpio_config_t io_conf={
		.intr_type=GPIO_INTR_DISABLE,
		.mode=GPIO_MODE_OUTPUT,
		.pin_bit_mask=(1ULL<<GPIO_HOST_WAKEUP)
	};


	ESP_LOGI(TAG, "Host wakeup: IO%u, level:High", GPIO_HOST_WAKEUP);
	gpio_config(&io_conf);
	reset_host_wakeup_gpio();

	assert(wakeup_sem = xSemaphoreCreateBinary());
	xSemaphoreGive(wakeup_sem);
	host_wakeup_cb = fn_host_wakeup_cb;
#endif
}

void host_power_save_deinit(void)
{
#if HOST_PS_ALLOWED
	if (wakeup_sem) {
		/* Dummy take and give sema before deleting it */
		xSemaphoreTake(wakeup_sem, portMAX_DELAY);
		xSemaphoreGive(wakeup_sem);
		vSemaphoreDelete(wakeup_sem);
		wakeup_sem = NULL;
	}
	host_wakeup_cb = NULL;
#endif
}


#define GET_CURR_TIME_IN_MS() esp_timer_get_time()/1000
int wakeup_host_mandate(uint32_t timeout_ms)
{

#if HOST_PS_ALLOWED

	TimerHandle_t xTimer = NULL;
	esp_err_t ret = ESP_OK;
	uint64_t start_time = GET_CURR_TIME_IN_MS();
	uint8_t wakeup_success = 0;

	ESP_LOGI(TAG, "WAKE UP Host!!!!!\n");

	do {
		set_host_wakeup_gpio();
		xTimer = xTimerCreate("Timer", pdMS_TO_TICKS(10) , pdFALSE, 0, oobTimerCallback);
		if (xTimer == NULL) {
			ESP_LOGE(TAG, "Failed to create timer for host wakeup");
			break;
		}

		ret = xTimerStart(xTimer, 0);
		if (ret != pdPASS) {
			ESP_LOGE(TAG, "Failed to start timer for host wakeup");
			break;
		}

		if (wakeup_sem) {
			/* wait for host resume */
			ret = xSemaphoreTake(wakeup_sem, pdMS_TO_TICKS(100));

			if (ret == pdPASS) {
				/* usleep(100*1000);*/
				xSemaphoreGive(wakeup_sem);
				wakeup_success = 1;
				break;
			}
		}

		if (GET_CURR_TIME_IN_MS() - start_time > timeout_ms) {
			/* timeout */
			break;
		}

	} while (1);

	return wakeup_success;

#else
	return 1;
#endif
}

int wakeup_host(uint32_t timeout_ms)
{

#if HOST_PS_ALLOWED

	if(!is_host_power_saving()) {
		return 1;
	}

	return wakeup_host_mandate(timeout_ms);
#else
	return 1;
#endif
}

void host_power_save_alert(uint32_t ps_evt)
{
#if HOST_PS_ALLOWED
    if (ESP_POWER_SAVE_ON == ps_evt) {
        ESP_EARLY_LOGI(TAG, "Host Sleep");
        if (wakeup_sem) {
            /* Host sleeping */
            xSemaphoreTake(wakeup_sem, portMAX_DELAY);
        }
        power_save_on = 1;

    } else if (ESP_POWER_SAVE_OFF == ps_evt || ESP_OPEN_DATA_PATH == ps_evt) {
        /* Also handle ESP_OPEN_DATA_PATH as power save off to handle reset cases */
        ESP_EARLY_LOGI(TAG, "Host Awake");

        power_save_on = 0;
        if (host_wakeup_cb) {
            host_wakeup_cb();
        }
        if (wakeup_sem) {
            xSemaphoreGive(wakeup_sem);
        }
    } else {
        ESP_EARLY_LOGI(TAG, "Ignore event[%u]", ps_evt);
    }
#endif
}

#if HOST_PS_ALLOWED
static void oobTimerCallback( TimerHandle_t xTimer )
{
    xTimerDelete(xTimer, 0);
	reset_host_wakeup_gpio();
}
#endif

