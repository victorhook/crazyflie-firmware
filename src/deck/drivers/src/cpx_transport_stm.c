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

#include <string.h>
#include <assert.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"
#include "event_groups.h"

#include "cpx_transport_stm.h"

// This is probably too big, but let's keep things simple....
#define ROUTER_TX_QUEUE_LENGTH (4)
#define ROUTER_RX_QUEUE_LENGTH (4)

static xQueueHandle rxQueue;
static xQueueHandle txQueue;

static bool isInit = false;


void stmTransportInit() {
	if (isInit)
		return;

	rxQueue = xQueueCreate(ROUTER_RX_QUEUE_LENGTH, sizeof(CPXPacket_t));
	txQueue = xQueueCreate(ROUTER_TX_QUEUE_LENGTH, sizeof(stm_routable_packet_t));

	isInit = true;
}

// Used by router
void stmTransportSend(const CPXPacket_t* packet) {
	ASSERT(packet->dataLength <= STM_TRANSPORT_MTU - CPX_ROUTING_PACKED_SIZE);
	xQueueSend(rxQueue, packet, portMAX_DELAY);
}

void stmTransportReceive(CPXPacket_t* packet) {
	stm_routable_packet_t* buf = packet;
	xQueueReceive(txQueue, buf, portMAX_DELAY);
}


// Used by applications
void stmSendBlocking(const stm_routable_packet_t* packet) {
	xQueueSend(txQueue, packet, portMAX_DELAY);
}

void stmReceiveBlocking(stm_routable_packet_t* packet) {
	CPXPacket_t* buf = packet;
	xQueueReceive(rxQueue, buf, portMAX_DELAY);
}
