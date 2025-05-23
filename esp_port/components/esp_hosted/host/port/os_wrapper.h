// Copyright 2015-2021 Espressif Systems (Shanghai) PTE LTD
/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */

#ifndef __OS_WRAPPER_H
#define __OS_WRAPPER_H

#include "os_header.h"
#include "esp_task.h"
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>

#include "mempool.h"
//#include "esp_hosted_config.h"

#include "esp_hosted_config.h"
#include "hosted_os_adapter.h"
#include "esp_timer.h"
#include "esp_event.h"
#include "esp_netif_types.h"
#include "esp_wifi_types.h"
#include "esp_wifi_default.h"


ESP_EVENT_DECLARE_BASE(WIFI_EVENT);
#define MCU_SYS                                      1

#include "common.h"
#include "esp_dma_utils.h"

#define MAX_PAYLOAD_SIZE (MAX_TRANSPORT_BUFFER_SIZE-H_ESP_PAYLOAD_HEADER_OFFSET)


#define RPC__TIMER_ONESHOT                           0
#define RPC__TIMER_PERIODIC                          1

#define HOSTED_BLOCKING                              -1
#define HOSTED_NON_BLOCKING                          0

#define thread_handle_t                              TaskHandle_t
#define queue_handle_t                               QueueHandle_t
#define semaphore_handle_t                           SemaphoreHandle_t
#define mutex_handle_t                               SemaphoreHandle_t
#define event_group_handle_t                         EventGroupHandle_t

#define spinlock_handle_t                            portMUX_TYPE
#define gpio_port_handle_t                           (void*)

#define FAST_RAM_ATTR                                IRAM_ATTR
/* this is needed when there is no gpio port being used */
#define H_GPIO_PORT_DEFAULT                          -1

#define gpio_pin_state_t                             int

#define HOSTED_BLOCK_MAX                             portMAX_DELAY

#define RPC_TASK_STACK_SIZE                          (5*1024)
#define RPC_TASK_PRIO                                23
#define DFLT_TASK_STACK_SIZE                         (5*1024)
#define DFLT_TASK_PRIO                               22



#define H_GPIO_MODE_DEF_DISABLE         (0)
#define H_GPIO_MODE_DEF_INPUT           (BIT0)    ///< bit mask for input
#define H_GPIO_MODE_DEF_OUTPUT          (BIT1)    ///< bit mask for output
#define H_GPIO_MODE_DEF_OD              (BIT2)    ///< bit mask for OD mode
enum {
	H_GPIO_MODE_DISABLE = H_GPIO_MODE_DEF_DISABLE,                                                         /*!< GPIO mode : disable input and output             */
	H_GPIO_MODE_INPUT = H_GPIO_MODE_DEF_INPUT,                                                             /*!< GPIO mode : input only                           */
	H_GPIO_MODE_OUTPUT = H_GPIO_MODE_DEF_OUTPUT,                                                           /*!< GPIO mode : output only mode                     */
	H_GPIO_MODE_OUTPUT_OD = ((H_GPIO_MODE_DEF_OUTPUT) | (H_GPIO_MODE_DEF_OD)),                               /*!< GPIO mode : output only with open-drain mode     */
	H_GPIO_MODE_INPUT_OUTPUT_OD = ((H_GPIO_MODE_DEF_INPUT) | (H_GPIO_MODE_DEF_OUTPUT) | (H_GPIO_MODE_DEF_OD)), /*!< GPIO mode : output and input with open-drain mode*/
	H_GPIO_MODE_INPUT_OUTPUT = ((H_GPIO_MODE_DEF_INPUT) | (H_GPIO_MODE_DEF_OUTPUT)),                         /*!< GPIO mode : output and input mode                */
};

#if 0
#if 0 //defined OS_CMSIS
#define thread_handle_t                        osThreadId
#define osThreadId                             osThreadId
#define semaphore_handle_t                     osSemaphoreId
#define mutex_handle_t                         osMutexId
#else //if defined OS_FREERTOS
#define thread_handle_t                        TaskHandle_t
#define queue_handle_t                         QueueHandle_t
#define semaphore_handle_t                     SemaphoreHandle_t
#define mutex_handle_t                         SemaphoreHandle_t
#define osDelay                                vTaskDelay
//#else
//#error "Port the os calls to your system"
#endif
#endif


#define RET_OK                                       0
#define RET_FAIL                                     -1
#define RET_INVALID                                  -2
#define RET_FAIL_MEM                                 -3
#define RET_FAIL4                                    -4

/* without alignment */
#define MALLOC(x)                        malloc(x)

/* This is [malloc + aligned DMA] */
#define MEM_ALLOC(x)       ({                                       \
	esp_dma_mem_info_t dma_mem_info = {                             \
		.extra_heap_caps = MALLOC_CAP_SPIRAM,                       \
		.dma_alignment_bytes = 64,                                  \
	};                                                              \
	void *tmp_buf = NULL;                                           \
	size_t actual_size = 0;                                         \
	esp_err_t err = ESP_OK;                                         \
	err = esp_dma_capable_malloc((x), &dma_mem_info, &tmp_buf, &actual_size);\
	if (err) tmp_buf = NULL;                                        \
	tmp_buf;})

#define FREE(x)                          free(x);


/** Enumeration **/
enum hardware_type_e {
	HARDWARE_TYPE_ESP32,
	HARDWARE_TYPE_OTHER_ESP_CHIPSETS,
	HARDWARE_TYPE_INVALID,
};

//TODO: redesign common code over




#define MILLISEC_TO_SEC			1000
#define TICKS_PER_SEC(x) (1000*(x) / portTICK_PERIOD_MS)
#define SEC_TO_MILLISEC(x) (1000*(x))
#define SEC_TO_MICROSEC(x) (1000*1000*(x))


#define MEM_DUMP(s) \
	printf("%s free:%lu min-free:%lu lfb-def:%u lfb-8bit:%u\n\n", s,	\
			(unsigned long int)esp_get_free_heap_size(), (unsigned long int)esp_get_minimum_free_heap_size(), \
			heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT),		\
			heap_caps_get_largest_free_block(MALLOC_CAP_8BIT))

#if 0
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0)
  #define ESP_MUTEX_INIT(mUtEx) portMUX_INITIALIZE(&(mUtEx));
#else
  #define ESP_MUTEX_INIT(mUtEx) vPortCPUInitializeMutex(&(mUtEx));
#endif
#endif




/* -------- Create handle ------- */
#define HOSTED_CREATE_HANDLE(tYPE, hANDLE) {                                   \
	hANDLE = (tYPE *)g_h.funcs->_h_malloc(sizeof(tYPE));                       \
	if (!hANDLE) {                                                             \
		printf("%s:%u Mem alloc fail while create handle\n", __func__,__LINE__); \
		return NULL;                                                           \
	}                                                                          \
}

/* -------- Calloc, Free handle ------- */
#define HOSTED_FREE(buff) if (buff) { g_h.funcs->_h_free(buff); buff = NULL; }
#define HOSTED_CALLOC(struct_name, buff, nbytes, gotosym) do {    \
	buff = (struct_name *)g_h.funcs->_h_calloc(1, nbytes);	  \
	if (!buff) {                                                  \
		printf("%s, Failed to allocate memory \n", __func__);     \
		goto gotosym;                                             \
	}                                                             \
} while(0);

#define HOSTED_MALLOC(struct_name, buff, nbytes, gotosym) do {    \
	buff = (struct_name *)g_h.funcs->_h_malloc(nbytes);		  \
	if (!buff) {                                                  \
		printf("%s, Failed to allocate memory \n", __func__);     \
		goto gotosym;                                             \
	}                                                             \
} while(0);

/* Driver Handle */
struct serial_drv_handle_t;

/* Timer handle */
struct timer_handle_t;
extern struct mempool * nw_mp_g;

#endif /*__OS_WRAPPER_H*/
