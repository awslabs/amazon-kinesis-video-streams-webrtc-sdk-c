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

/** Includes **/
#include "common.h"
#include "esp_log.h"
#include <stdlib.h>
#include <errno.h>

#if 0
DEFINE_LOG_TAG(utils);
#endif
/** Constants/Macros **/

/** Exported variables **/


/** Function declaration **/

/** Exported Functions **/
/**
  * @brief  debug buffer print
  * @param  buff - input buffer to print in hex
  *         rx_len - buff len
  *         human_str - helping string to describe about buffer
  * @retval None
  */
#if DEBUG_HEX_STREAM_PRINT
char print_buff[MAX_SPI_BUFFER_SIZE*3];
#endif

uint16_t hton_short (uint16_t x)
{
#if BYTE_ORDER == BIG_ENDIAN
  return x;
#elif BYTE_ORDER == LITTLE_ENDIAN
  uint16_t val = 0;

  val = (x &0x00FF)<<8;
  val |= (x &0xFF00)>>8;

  return val;
#else
# error "not able to identify endianness"
#endif
}

uint32_t hton_long (uint32_t x)
{
#if BYTE_ORDER == BIG_ENDIAN
  return x;
#elif BYTE_ORDER == LITTLE_ENDIAN
    uint32_t val = (x&0xFF000000) >> 24;

    val |= (x&0x00FF0000) >> 8;
    val |= (x&0x0000FF00) << 8;
    val |= (x&0x000000FF) << 24;

    return val;
#else
# error "not able to identify endianness"
#endif
}

/**
  * @brief  Calculate minimum
  * @param  x - number
  *         y - number
  * @retval minimum
  */
int min(int x, int y) {
    return (x < y) ? x : y;
}

#if 0
/**
  * @brief  get numbers from string
  * @param  val - return integer value,
  *         arg - input string
  * @retval STM_OK on success, else STM_FAIL
  */
int get_num_from_string(int *val, char *arg)
{
  int base = 10;
  char *endptr = NULL, *str = NULL;

  if (!arg || (arg[0]=='\0')) {
    ESP_LOGE(TAG, "No number Identified \n");
    return STM_FAIL;
  }

  if (!val) {
    ESP_LOGE(TAG, "No memory allocated \n");
    return STM_FAIL;
  }

  errno = 0;
  str = arg;
  *val = strtol(str, &endptr, base);

  if (endptr == str) {
    ESP_LOGE(TAG, "No digits found \n");
    *val = 0;
    return STM_FAIL;
  }

  if ((errno == ERANGE) && ((*val == INT32_MAX) || (*val == INT32_MIN))) {
    perror("strtol");
    *val = 0;
    return STM_FAIL;
  }

  return STM_OK;
}
#endif

/** Local functions **/
