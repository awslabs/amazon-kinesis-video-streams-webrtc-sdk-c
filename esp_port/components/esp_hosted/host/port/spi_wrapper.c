// SPDX-License-Identifier: Apache-2.0
// Copyright 2015-2021 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "esp_check.h"
#include "esp_log.h"

#include "driver/spi_master.h"

#include "os_wrapper.h"
#include "transport_drv.h"
#include "spi_wrapper.h"
#include "driver/gpio.h"

DEFINE_LOG_TAG(spi_wrapper);

extern void * spi_handle;

void * hosted_spi_init(void)
{

#ifdef CONFIG_IDF_TARGET_ESP32
#define SENDER_HOST                                  HSPI_HOST

#else
#define SENDER_HOST                                  SPI2_HOST

#endif


    esp_err_t ret;
	ESP_LOGI(TAG, "Transport: SPI, Mode:%u Freq:%uMHz TxQ:%u RxQ:%u\n GPIOs: MOSI:%u MISO:%u CLK:%u CS:%u HS:%u DR:%u SlaveReset:%u",
			H_SPI_MODE, H_SPI_INIT_CLK_MHZ, H_SPI_TX_Q, H_SPI_RX_Q,
			H_GPIO_MOSI_Pin, H_GPIO_MISO_Pin, H_GPIO_SCLK_Pin,
			H_GPIO_CS_Pin, H_GPIO_HANDSHAKE_Pin, H_GPIO_DATA_READY_Pin,
			H_GPIO_PIN_RESET_Pin);

    HOSTED_CREATE_HANDLE(spi_device_handle_t, spi_handle);
    assert(spi_handle);


    //Configuration for the SPI bus
    spi_bus_config_t buscfg={
        .mosi_io_num=H_GPIO_MOSI_Pin,
        .miso_io_num=H_GPIO_MISO_Pin,
        .sclk_io_num=H_GPIO_SCLK_Pin,
        .quadwp_io_num=-1,
        .quadhd_io_num=-1
    };

    //Configuration for the SPI device on the other side of the bus
    spi_device_interface_config_t devcfg={
        .command_bits=0,
        .address_bits=0,
        .dummy_bits=0,
        .clock_speed_hz=MHZ_TO_HZ(H_SPI_INIT_CLK_MHZ),
        .duty_cycle_pos=128,        //50% duty cycle
        .mode=H_SPI_MODE,
        .spics_io_num=H_GPIO_CS_Pin,
        .cs_ena_posttrans=3,        //Keep the CS low 3 cycles after transaction, to stop slave from missing the last bit when CS has less propagation delay than CLK
        .queue_size=3
    };

    //Initialize the SPI bus and add the device we want to send stuff to.
    ret=spi_bus_initialize(SENDER_HOST, &buscfg, SPI_DMA_CH_AUTO);
    assert(ret==ESP_OK);
    ret=spi_bus_add_device(SENDER_HOST, &devcfg, spi_handle);
    assert(ret==ESP_OK);

    //Assume the slave is ready for the first transmission: if the slave started up before us, we will not detect
    //positive edge on the handshake line.
	gpio_set_drive_capability(H_GPIO_CS_Pin, GPIO_DRIVE_CAP_3);
	gpio_set_drive_capability(H_GPIO_SCLK_Pin, GPIO_DRIVE_CAP_3);
    return spi_handle;
}

int hosted_do_spi_transfer(void *trans)
{
    spi_transaction_t t = {0};
	struct hosted_transport_context_t * spi_trans = trans;

    t.length=spi_trans->tx_buf_size*8;
    t.tx_buffer=spi_trans->tx_buf;
    t.rx_buffer=spi_trans->rx_buf;

    return spi_device_transmit(*((spi_device_handle_t *)spi_handle), &t);
}
