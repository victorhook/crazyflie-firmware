/**
 * ,---------,       ____  _ __
 * |  ,-^-,  |      / __ )(_) /_______________ _____  ___
 * | (  O  ) |     / __  / / __/ ___/ ___/ __ `/_  / / _ \
 * | / ,--´  |    / /_/ / / /_/ /__/ /  / /_/ / / /_/  __/
 *    +------`   /_____/_/\__/\___/_/   \__,_/ /___/\___/
 *
 * ESP deck firmware
 *
 * Copyright (C) 2022 Bitcraze AB
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, in version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "com.h"

#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"
#include "event_groups.h"

#include "debug.h"
#include "router.h"
#include "cpx_transport_stm.h"
#include "slamdeck.h"


#define ESP_SYS_QUEUE_LENGTH (2)
#define ESP_SYS_QUEUE_SIZE (sizeof(stm_routable_packet_t))
#define ESP_TEST_QUEUE_LENGTH (2)
#define ESP_TEST_QUEUE_SIZE (sizeof(stm_routable_packet_t))
#define ESP_APP_QUEUE_LENGTH (2)
#define ESP_APP_QUEUE_SIZE (sizeof(stm_routable_packet_t))

static xQueueHandle systemQueue;
static xQueueHandle testQueue;
static xQueueHandle appQueue;

static stm_routable_packet_t rxp;

bool isInit = false;

static void com_rx(void* _param) {

  while (1) {
    stmReceiveBlocking(&rxp);

    switch (rxp.route.function) {
      case CPX_F_TEST:
        xQueueSend(testQueue, &rxp, (TickType_t) portMAX_DELAY);
        break;
      case CPX_F_SYSTEM:
        xQueueSend(systemQueue, &rxp, (TickType_t) portMAX_DELAY);
        break;
      case CPX_F_APP:
        xQueueSend(appQueue, &rxp, (TickType_t) portMAX_DELAY);
        break;
      default:
        DEBUG_PRINT("Cannot handle 0x%02X", rxp.route.function);
    }
  }
}

void com_init() {
  if (isInit)
    return;

  systemQueue = xQueueCreate(ESP_SYS_QUEUE_LENGTH, ESP_SYS_QUEUE_SIZE);
  testQueue = xQueueCreate(ESP_TEST_QUEUE_LENGTH, ESP_TEST_QUEUE_SIZE);
  appQueue = xQueueCreate(ESP_APP_QUEUE_LENGTH, ESP_APP_QUEUE_SIZE);
  isInit = true;
}

void com_receive_test_blocking(stm_routable_packet_t * packet) {
	systemWaitStart();
	xQueueReceive(testQueue, packet, portMAX_DELAY);
}

void com_receive_system_blocking(stm_routable_packet_t * packet) {
	systemWaitStart();
	xQueueReceive(systemQueue, packet, (TickType_t) portMAX_DELAY);
}

void com_receive_app_blocking(stm_routable_packet_t * packet) {
	systemWaitStart();
	xQueueReceive(appQueue, packet, (TickType_t) portMAX_DELAY);
}
