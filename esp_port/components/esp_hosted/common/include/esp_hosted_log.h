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
//

#ifndef __ESP_HOSTED_LOG_H
#define __ESP_HOSTED_LOG_H
#include "esp_log.h"

#define ESP_PRIV_HEXDUMP(tag1, tag2, buff, buf_len, display_len, curr_level)    \
  if ( LOG_LOCAL_LEVEL >= curr_level) {                                         \
	  int len_to_print = 0;                                                     \
	len_to_print = display_len<buf_len? display_len: buf_len;                   \
    ESP_LOG_LEVEL_LOCAL(curr_level, tag1, "%s: buf_len[%d], print_len[%d]",     \
			tag2, (int)buf_len, (int)len_to_print);                             \
    ESP_LOG_BUFFER_HEXDUMP(tag2, buff, len_to_print, curr_level);               \
  }

#define ESP_HEXLOGE(tag2, buff, buf_len, display_len) ESP_PRIV_HEXDUMP(TAG, tag2, buff, buf_len, display_len, ESP_LOG_ERROR)
#define ESP_HEXLOGW(tag2, buff, buf_len, display_len) ESP_PRIV_HEXDUMP(TAG, tag2, buff, buf_len, display_len, ESP_LOG_WARN)
#define ESP_HEXLOGI(tag2, buff, buf_len, display_len) ESP_PRIV_HEXDUMP(TAG, tag2, buff, buf_len, display_len, ESP_LOG_INFO)
#define ESP_HEXLOGD(tag2, buff, buf_len, display_len) ESP_PRIV_HEXDUMP(TAG, tag2, buff, buf_len, display_len, ESP_LOG_DEBUG)
#define ESP_HEXLOGV(tag2, buff, buf_len, display_len) ESP_PRIV_HEXDUMP(TAG, tag2, buff, buf_len, display_len, ESP_LOG_VERBOSE)

#endif

