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

/* Wrapper interfaces for SPI to communicated with slave using SDIO */

#ifndef __SPI_WRAPPER_H_
#define __SPI_WRAPPER_H_

#define MAX_TRANSPORT_BUFFER_SIZE        MAX_SPI_BUFFER_SIZE
/* Hosted SPI init function
 * returns a pointer to the spi context */
void * hosted_spi_init(void);

/* Hosted SPI transfer function */
int hosted_do_spi_transfer(void *trans);

#endif
