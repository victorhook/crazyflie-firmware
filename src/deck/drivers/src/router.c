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
#include <stdio.h>
#include <string.h>
#include <debug.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"
#include "event_groups.h"

#include "router.h"
#include "cpx.h"
#include "cpx_transport_uart.h"
#include "cpx_transport_stm.h"
#include "cpx_transport_crtp.h"


typedef struct {
  CPXPacket_t txp;
} RouteContext_t;

#define STACK_SIZE configMINIMAL_STACK_SIZE
#define ROUTER_TASK_PRIORITY 2

// ESP
static RouteContext_t esp_task_context;
static CPXPacket_t uartRxBuf;
static StaticTask_t espTaskBuffer;
static StackType_t espStack[STACK_SIZE];

// STM
static RouteContext_t stm_task_context;
static CPXPacket_t stmRxBuf;
static StaticTask_t stmTaskBuffer;
static StackType_t stmStack[STACK_SIZE];

// CRTP
static RouteContext_t crtp_task_context;
static CPXPacket_t crtpRxBuf;
static StaticTask_t crtpTaskBuffer;
static StackType_t crtpStack[STACK_SIZE];

typedef void (*Receiver_t)(CPXPacket_t* packet);
typedef void (*Sender_t)(const CPXPacket_t* packet);



static void splitAndSend(const CPXPacket_t* rxp, RouteContext_t* context, Sender_t sender, const uint16_t mtu, bool routeAblePacket) {
	CPXPacket_t* txp = &context->txp;

	txp->route = rxp->route;

	uint16_t remainingToSend = rxp->dataLength;
	const uint8_t* startOfDataToSend = rxp->data;
	while (remainingToSend > 0) {

		uint16_t toSend = remainingToSend;
		bool lastPacket = rxp->route.lastPacket;

    if (toSend > mtu) {
      toSend = mtu;
      lastPacket = false;
    }

		memcpy(txp->data, startOfDataToSend, toSend);
		txp->dataLength = toSend;
		txp->route.lastPacket = lastPacket;

		sender(txp);

		remainingToSend -= toSend;
		startOfDataToSend += toSend;

		// This is needed to let other tasks run as well, otherwise we sometimes crash.
		//vTaskDelay(5 / portTICK_PERIOD_MS);
	}

}

static void route(Receiver_t receive, CPXPacket_t* rxp, RouteContext_t* context, const char* routerName) {
	DEBUG_PRINT("Router %s started \n", routerName);
	while(1) {
		receive(rxp);

		const CPXTarget_t source = rxp->route.source;
		const CPXTarget_t destination = rxp->route.destination;
		const uint16_t cpxDataLength = rxp->dataLength;
		const bool lastPacket = rxp->route.lastPacket;

		//DEBUG_PRINT("[R] %02x -> %02x\n", source, destination);

		switch (destination) {
			case CPX_T_ESP32:
				splitAndSend(rxp, context, uartTransportSend, UART_TRANSPORT_MTU - CPX_ROUTING_PACKED_SIZE, lastPacket);
				break;
			case CPX_T_STM32:
				splitAndSend(rxp, context, stmTransportSend, STM_TRANSPORT_MTU - CPX_ROUTING_PACKED_SIZE, lastPacket);
				break;
			case CPX_T_HOST_NRF:
				splitAndSend(rxp, context, crtpTransportSend, CRTP_TRANSPORT_MTU - CPX_ROUTING_PACKED_SIZE, lastPacket);
				break;
			default:
				break;
		}

	}
}

static void routerFromStm(void* _param) {
	systemWaitStart();
	route(stmTransportReceive, &stmRxBuf, &stm_task_context, "STM");
}

static void routerFromEsp32(void* _param) {
	systemWaitStart();
	route(uartTransportReceive, &uartRxBuf, &esp_task_context, "ESP32");
}

static void routerFromCrtp(void* _param) {
	systemWaitStart();
	route(crtpTransportReceive, &crtpRxBuf, &crtp_task_context, "CRTP");
}

void routerInit() {
	// These tasks must be created in static memory due to small heap.
	xTaskCreateStatic(routerFromStm,   "Router from STM",   STACK_SIZE, NULL, ROUTER_TASK_PRIORITY, stmStack, &stmTaskBuffer);
	xTaskCreateStatic(routerFromEsp32, "Router from ESP32", STACK_SIZE, NULL, ROUTER_TASK_PRIORITY, espStack, &espTaskBuffer);
	//xTaskCreateStatic(routerFromCrtp,  "Router from CRTP",  STACK_SIZE, NULL, ROUTER_TASK_PRIORITY, crtpStack, &crtpTaskBuffer);

	DEBUG_PRINT("Router intialized\n");
}
