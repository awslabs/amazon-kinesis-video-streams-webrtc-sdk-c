/*
 * SPDX-FileCopyrightText: 2015-2023 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef __ESP_HOSTED_CONFIG_H__
#define __ESP_HOSTED_CONFIG_H__

#include "sdkconfig.h"
#include "esp_task.h"
#include "hosted_os_adapter.h"
#include "adapter.h"

#ifdef CONFIG_ESP_SDIO_HOST_INTERFACE
#include "driver/sdmmc_host.h"
#endif

/* This file is to tune the main ESP-Hosted configurations.
 * In case you are not sure of some value, Let it be default.
 **/

#define H_GPIO_LOW                                   0
#define H_GPIO_HIGH                                  1

enum {
    H_GPIO_INTR_DISABLE = 0,     /*!< Disable GPIO interrupt                             */
    H_GPIO_INTR_POSEDGE = 1,     /*!< GPIO interrupt type : rising edge                  */
    H_GPIO_INTR_NEGEDGE = 2,     /*!< GPIO interrupt type : falling edge                 */
    H_GPIO_INTR_ANYEDGE = 3,     /*!< GPIO interrupt type : both rising and falling edge */
    H_GPIO_INTR_LOW_LEVEL = 4,   /*!< GPIO interrupt type : input low level trigger      */
    H_GPIO_INTR_HIGH_LEVEL = 5,  /*!< GPIO interrupt type : input high level trigger     */
    H_GPIO_INTR_MAX,
};




#ifdef CONFIG_ESP_SPI_HOST_INTERFACE
/*  -------------------------- SPI Master Config start ----------------------  */
/*
Pins in use. The SPI Master can use the GPIO mux,
so feel free to change these if needed.
*/


/* SPI config */

#ifdef CONFIG_HS_ACTIVE_LOW
  #define H_HANDSHAKE_ACTIVE_HIGH 0
#else
  /* Default HS: Active High */
  #define H_HANDSHAKE_ACTIVE_HIGH 1
#endif

#ifdef CONFIG_DR_ACTIVE_LOW
  #define H_DATAREADY_ACTIVE_HIGH 0
#else
  /* Default DR: Active High */
  #define H_DATAREADY_ACTIVE_HIGH 1
#endif

#if H_HANDSHAKE_ACTIVE_HIGH
  #define H_HS_VAL_ACTIVE                            H_GPIO_HIGH
  #define H_HS_VAL_INACTIVE                          H_GPIO_LOW
  #define H_HS_INTR_EDGE                             H_GPIO_INTR_POSEDGE
#else
  #define H_HS_VAL_ACTIVE                            H_GPIO_LOW
  #define H_HS_VAL_INACTIVE                          H_GPIO_HIGH
  #define H_HS_INTR_EDGE                             H_GPIO_INTR_NEGEDGE
#endif

#if H_DATAREADY_ACTIVE_HIGH
  #define H_DR_VAL_ACTIVE                            H_GPIO_HIGH
  #define H_DR_VAL_INACTIVE                          H_GPIO_LOW
  #define H_DR_INTR_EDGE                             H_GPIO_INTR_POSEDGE
#else
  #define H_DR_VAL_ACTIVE                            H_GPIO_LOW
  #define H_DR_VAL_INACTIVE                          H_GPIO_HIGH
  #define H_DR_INTR_EDGE                             H_GPIO_INTR_NEGEDGE
#endif

#define H_GPIO_HANDSHAKE_Port                        NULL
#define H_GPIO_HANDSHAKE_Pin                         CONFIG_ESP_SPI_GPIO_HANDSHAKE
#define H_GPIO_DATA_READY_Port                       NULL
#define H_GPIO_DATA_READY_Pin                        CONFIG_ESP_SPI_GPIO_DATA_READY



#define H_GPIO_MOSI_Port                             NULL
#define H_GPIO_MOSI_Pin                              CONFIG_ESP_SPI_GPIO_MOSI
#define H_GPIO_MISO_Port                             NULL
#define H_GPIO_MISO_Pin                              CONFIG_ESP_SPI_GPIO_MISO
#define H_GPIO_SCLK_Port                             NULL
#define H_GPIO_SCLK_Pin                              CONFIG_ESP_SPI_GPIO_CLK
#define H_GPIO_CS_Port                               NULL
#define H_GPIO_CS_Pin                                CONFIG_ESP_SPI_GPIO_CS

#define H_SPI_TX_Q                                   CONFIG_ESP_SPI_TX_Q_SIZE
#define H_SPI_RX_Q                                   CONFIG_ESP_SPI_RX_Q_SIZE

#define H_SPI_MODE                                   CONFIG_ESP_SPI_MODE
#define H_SPI_INIT_CLK_MHZ                           CONFIG_ESP_SPI_CLK_FREQ

/*  -------------------------- SPI Master Config end ------------------------  */
#endif

#ifdef CONFIG_ESP_SDIO_HOST_INTERFACE
/*  -------------------------- SDIO Host Config start -----------------------  */

#ifdef CONFIG_SOC_SDMMC_USE_GPIO_MATRIX
#define H_SDIO_SOC_USE_GPIO_MATRIX
#endif

#define H_SDIO_CLOCK_FREQ_KHZ                        CONFIG_ESP_SDIO_CLOCK_FREQ_KHZ
#define H_SDIO_BUS_WIDTH                             CONFIG_ESP_SDIO_BUS_WIDTH
#define H_SDMMC_HOST_SLOT                            SDMMC_HOST_SLOT_1

#ifdef H_SDIO_SOC_USE_GPIO_MATRIX
#define H_SDIO_PIN_CLK                               CONFIG_ESP_SDIO_PIN_CLK
#define H_SDIO_PIN_CMD                               CONFIG_ESP_SDIO_PIN_CMD
#define H_SDIO_PIN_D0                                CONFIG_ESP_SDIO_PIN_D0
#define H_SDIO_PIN_D1                                CONFIG_ESP_SDIO_PIN_D1
#if (H_SDIO_BUS_WIDTH == 4)
#define H_SDIO_PIN_D2                                CONFIG_ESP_SDIO_PIN_D2
#define H_SDIO_PIN_D3                                CONFIG_ESP_SDIO_PIN_D3
#endif
#endif

#define H_SDIO_HOST_STREAMING_MODE 1
#define H_SDIO_ALWAYS_HOST_RX_MAX_TRANSPORT_SIZE 2
#define H_SDIO_OPTIMIZATION_RX_NONE 3

#ifdef CONFIG_ESP_SDIO_OPTIMIZATION_RX_STREAMING_MODE
  #define H_SDIO_HOST_RX_MODE H_SDIO_HOST_STREAMING_MODE
#elif defined(CONFIG_ESP_SDIO_OPTIMIZATION_RX_MAX_SIZE)
  #define H_SDIO_HOST_RX_MODE H_SDIO_ALWAYS_HOST_RX_MAX_TRANSPORT_SIZE
#else
  /* Use this if unsure */
  #define H_SDIO_HOST_RX_MODE H_SDIO_OPTIMIZATION_RX_NONE
#endif

// Pad transfer len for host operation
#define H_SDIO_TX_LEN_TO_TRANSFER(x) ((x + 3) & (~3))
#define H_SDIO_RX_LEN_TO_TRANSFER(x) ((x + 3) & (~3))

// workarounds for some SDIO transfer errors that may occur
#if 0
/* Below workarounds could be enabled for non-ESP MCUs to test first
 * Once everything is stable, can disable workarounds and test again
 * */
#define H_SDIO_TX_LIMIT_XFER_SIZE_WORKAROUND // limit transfer to one ESP_BLOCK_SIZE at a time
#define H_SDIO_RX_LIMIT_XFER_SIZE_WORKDAROUND // limit transfer to one ESP_BLOCK_SIZE at a time
#endif

#if defined(H_SDIO_TX_LIMIT_XFER_SIZE_WORKAROUND)
#define H_SDIO_TX_BLOCKS_TO_TRANSFER(x) (1)
#else
#define H_SDIO_TX_BLOCKS_TO_TRANSFER(x) (x / ESP_BLOCK_SIZE)
#endif

#if defined(H_SDIO_RX_LIMIT_XFER_SIZE_WORKDAROUND)
#define H_SDIO_RX_BLOCKS_TO_TRANSFER(x) (1)
#else
#define H_SDIO_RX_BLOCKS_TO_TRANSFER(x) (x / ESP_BLOCK_SIZE)
#endif

/*  -------------------------- SDIO Host Config end -------------------------  */
#endif

/* Generic reset pin config */
#define H_GPIO_PIN_RESET_Port                        NULL
#define H_GPIO_PIN_RESET_Pin                         CONFIG_ESP_GPIO_SLAVE_RESET_SLAVE

/* If Reset pin is Enable, it is Active High.
 * If it is RST, active low */
#ifdef CONFIG_RESET_GPIO_ACTIVE_LOW
  #define H_RESET_ACTIVE_HIGH                           0
#else
  #define H_RESET_ACTIVE_HIGH                           1
#endif

#ifdef H_RESET_ACTIVE_HIGH
  #define H_RESET_VAL_ACTIVE                            H_GPIO_HIGH
  #define H_RESET_VAL_INACTIVE                          H_GPIO_LOW
#else
  #define H_RESET_VAL_ACTIVE                            H_GPIO_LOW
  #define H_RESET_VAL_INACTIVE                          H_GPIO_HIGH
#endif


#define TIMEOUT_PSERIAL_RESP                         30


#define PRE_FORMAT_NEWLINE_CHAR                      ""
#define POST_FORMAT_NEWLINE_CHAR                     "\n"

#define USE_STD_C_LIB_MALLOC                         0

#ifdef CONFIG_H2S_WIFI_FLOW_CTRL
  #define H_H2S_WIFI_FLOW_CTRL_LOW_TH        CONFIG_H2S_WIFI_FLOW_CTRL_LOW_TH
  #define H_H2S_WIFI_FLOW_CTRL_HIGH_TH       CONFIG_H2S_WIFI_FLOW_CTRL_HIGH_TH
#else
  #define H_H2S_WIFI_FLOW_CTRL_LOW_TH                  0
  #define H_H2S_WIFI_FLOW_CTRL_HIGH_TH                 0
#endif

/* Raw Throughput Testing */
#define H_TEST_RAW_TP     CONFIG_ESP_RAW_THROUGHPUT_TRANSPORT

#if H_TEST_RAW_TP
#if CONFIG_ESP_RAW_THROUGHPUT_TX_TO_SLAVE
#define H_TEST_RAW_TP_DIR (ESP_TEST_RAW_TP__HOST_TO_ESP)
#elif CONFIG_ESP_RAW_THROUGHPUT_RX_FROM_SLAVE
#define H_TEST_RAW_TP_DIR (ESP_TEST_RAW_TP__ESP_TO_HOST)
#elif CONFIG_ESP_RAW_THROUGHPUT_BIDIRECTIONAL
#define H_TEST_RAW_TP_DIR (ESP_TEST_RAW_TP__BIDIRECTIONAL)
#else
#error Test Raw TP direction not defined
#endif
#else
#define H_TEST_RAW_TP_DIR (ESP_TEST_RAW_TP_NONE)
#endif

/* ----------------------- Enable packet stats ------------------------------- */
#ifdef CONFIG_ESP_PKT_STATS
#define ESP_PKT_STATS 1
#define ESP_PKT_STATS_REPORT_INTERVAL  CONFIG_ESP_PKT_STATS_INTERVAL_SEC
#endif

/* ----------------- Host to slave Wi-Fi flow control ------------------------ */
/* Bit0: slave request host to enable flow control */
#define H_EVTGRP_BIT_FC_ALLOW_WIFI BIT(0)

#if defined(CONFIG_H2S_WIFI_FLOW_CTRL)
  /* Flow control enable */
  #define H_H2S_WIFI_FLOW_CTRL                      1
  /* Policy to be in effect once slave instructs to host to start flow control */
  #if defined(CONFIG_H2S_WIFI_FLOW_CTRL_DROP)
    /* Drop packets till slave stops flow control. Ideal for UDP type apps where,
	 * App can get immediate feedback of drop packet.
	 * Example: video calling. Drop immediately when flow control. app can ignore
	 * current (stale) packet and lower the link speed or bit-rate quality and
	 * attempt newer packets */
    #define H_H2S_WIFI_FLOW_CTRL_DROP               1
  #elif defined(CONFIG_H2S_WIFI_FLOW_CTRL_BLOCK)
    /* Block host wifi tx altogether till slave stops flow control.
	 * This would slower the link (and the application).
	 * Good for blocking type of applicattion, where it is not time critical and
	 * easier if driver takes care of everything */
    #define H_H2S_WIFI_FLOW_CTRL_BLOCK              2
  #elif defined(CONFIG_H2S_WIFI_FLOW_CTRL_BLOCK_WITH_TIMEOUT)
    /* trade off of both above */
    #define H_H2S_WIFI_FLOW_CTRL_BLOCK_TIMEOUT      3
    #define H_H2S_WIFI_FLOW_CTRL_BLOCK_TIMEOUT_TICKS \
		pdMS_TO_TICKS(CONFIG_H2S_WIFI_FLOW_CTRL_BLOCK_WITH_TIMEOUT_IN_MS)
  #else
    #error "Define flow control policy"
  #endif
#endif

/* --------------------- Host Power saving ----------------------------------- */

#ifdef CONFIG_HOST_DEEP_SLEEP_ALLOWED
  #define H_HOST_PS_ALLOWED 1
#else
  #define H_HOST_PS_ALLOWED 0
#endif

#ifdef CONFIG_HOST_WAKEUP_GPIO
  #define H_HOST_WAKEUP_GPIO CONFIG_HOST_WAKEUP_GPIO

  #if -1 == H_HOST_WAKEUP_GPIO
    #error "Please configure valid value for Host wake-up GPIO"
  #endif

  #define H_HOST_WAKEUP_GPIO_LEVEL CONFIG_HOST_WAKEUP_GPIO_LEVEL
#endif

#ifdef CONFIG_HOST_WAKEUP_USING_RTC_GPIO
  #define H_HOST_WAKEUP_USING_RTC_GPIO 1
#else
  #define H_HOST_WAKEUP_USING_RTC_GPIO 0
#endif

#ifdef CONFIG_HOST_WAKEUP_USING_RTC_TIMER
  #define H_HOST_WAKEUP_USING_RTC_TIMER 1
#else
  #define H_HOST_WAKEUP_USING_RTC_TIMER 0
#endif

#if H_HOST_WAKEUP_USING_RTC_GPIO && H_HOST_WAKEUP_USING_RTC_TIMER
  #error "Host Wakeup GPIO and RTC Timer cannot be used together"
#endif

#if defined(CONFIG_SDIO_DISABLE_GPIO_BASED_SLAVE_RESET) && defined(CONFIG_ESP_SDIO_HOST_INTERFACE)
  #define H_SDIO_DISABLE_GPIO_BASED_SLAVE_RESET 1
#elif defined(CONFIG_SDIO_DISABLE_GPIO_BASED_SLAVE_RESET) && !defined(CONFIG_ESP_SDIO_HOST_INTERFACE)
  #error "SDIO Disable GPIO Based Slave Reset is only supported for SDIO transport yet"
#elif !defined(CONFIG_SDIO_DISABLE_GPIO_BASED_SLAVE_RESET) && defined(CONFIG_ESP_SDIO_HOST_INTERFACE)
  #define H_SDIO_DISABLE_GPIO_BASED_SLAVE_RESET 0
#endif

/* One of below way to reset the slave is mandatory, to avoid any transport communication failure:
1. If the transport is not SDIO, the reset pin is still mandatory. No other options below would be valid.
2. With SDIO transport, Host resets slave using GPIO - Easier & stable legacy way. But needs one dedicated GPIO from host to slave's EN/RST pin
3. With SDIO transport, User wants to save the GPIOs used at host. So there is no reset pin but has Host Wake-Up pin from slave to host.
   In this case, host would reset itself if it identified Wake up pin interrupt while awake, to safely assume slave was crashed.
*/

#if CONFIG_SDIO_DISABLE_GPIO_BASED_SLAVE_RESET && !H_HOST_PS_ALLOWED
#error "Invalid combination. Host Wake-Up GPIO is mandatory if Slave-Reset GPIO is not configured"
#endif

#endif /*__ESP_HOSTED_CONFIG_H__*/
