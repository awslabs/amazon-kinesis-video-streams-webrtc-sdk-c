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

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_hosted_rpc.pb-c.h"
typedef esp_err_t (*pserial_xmit)(uint8_t *buf, ssize_t len);
typedef ssize_t (*pserial_recv)(uint8_t *buf, ssize_t len);

esp_err_t protocomm_pserial_start(protocomm_t *pc, pserial_xmit xmit, pserial_recv recv);
esp_err_t protocomm_pserial_data_ready(protocomm_t *pc, uint8_t * in, int len, int msg_id);


#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0) 
  #define QUEUE_HANDLE QueueHandle_t
#else
  #define QUEUE_HANDLE xQueueHandle
#endif

#ifdef __cplusplus
}
#endif

