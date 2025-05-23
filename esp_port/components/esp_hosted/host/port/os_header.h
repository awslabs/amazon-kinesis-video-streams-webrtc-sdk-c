// Copyright 2015-2021 Espressif Systems (Shanghai) PTE LTD
/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */

#ifndef __OS_HEADER_H
#define __OS_HEADER_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#ifndef DEFINE_LOG_TAG
#define DEFINE_LOG_TAG(sTr) static const char TAG[] = #sTr
#endif
#endif /*__OS_HEADER_H*/
