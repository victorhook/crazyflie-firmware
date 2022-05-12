#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"
#include "queue.h"
#include "log.h"
#include "param.h"
#include "system.h"
#include "uart2.h"
#include "debug.h"
#include "deck.h"

#include "cpx.h"
#include "cpx_transport_uart.h"


static bool isInit = false;

#define TX_QUEUE_LENGTH 4
#define RX_QUEUE_LENGTH 4

static xQueueHandle txQueue;
static xQueueHandle rxQueue;

// Length of start + payloadLength
#define UART_HEADER_LENGTH 2
#define UART_CRC_LENGTH 1
#define UART_META_LENGTH (UART_HEADER_LENGTH + UART_CRC_LENGTH)

#define UART_TASK_PRIORITY 2

typedef struct {
    CPXRoutingPacked_t route;
    uint8_t data[UART_TRANSPORT_MTU - CPX_ROUTING_PACKED_SIZE];
} __attribute__((packed)) uartTransportPayload_t;

typedef struct {
    uint8_t start;
    uint8_t payloadLength; // Excluding start and crc
    union {
        uartTransportPayload_t routablePayload;
        uint8_t payload[UART_TRANSPORT_MTU];
    };

    uint8_t crcPlaceHolder; // Not actual position. CRC is added after the last byte of payload
} __attribute__((packed)) uart_transport_packet_t;

// Used when sending/receiving data on the UART
static uart_transport_packet_t txp;
static CPXPacket_t cpxTxp;
static uart_transport_packet_t rxp;

static EventGroupHandle_t evGroup;
#define CTS_EVENT (1 << 0)
#define CTR_EVENT (1 << 1)
#define TXQ_EVENT (1 << 2)


static uint8_t calcCrc(const uart_transport_packet_t* packet) {
	const uint8_t* start = (const uint8_t*) packet;
	const uint8_t* end = &packet->payload[packet->payloadLength];

	uint8_t crc = 0;
	for (const uint8_t* p = start; p < end; p++) {
		crc ^= *p;
	}

	return crc;
}

static void assemblePacket(const CPXPacket_t *packet, uart_transport_packet_t * txp) {
	ASSERT((packet->route.destination >> 4) == 0);
	ASSERT((packet->route.source >> 4) == 0);
	ASSERT((packet->route.function >> 8) == 0);
	ASSERT(packet->dataLength <= UART_TRANSPORT_MTU - CPX_ROUTING_PACKED_SIZE);

	txp->payloadLength = packet->dataLength + CPX_ROUTING_PACKED_SIZE;
	txp->routablePayload.route.destination = packet->route.destination;
	txp->routablePayload.route.source = packet->route.source;
	txp->routablePayload.route.lastPacket = packet->route.lastPacket;
	txp->routablePayload.route.function = packet->route.function;
	memcpy(txp->routablePayload.data, &packet->data, packet->dataLength);
	txp->payload[txp->payloadLength] = calcCrc(txp);
}

static void uartRxTask(void *param)
{
  systemWaitStart();

	DEBUG_PRINT("ESP RX Started\n");

	while (1)
	{
		// Wait for start!
		do
		{
			uart2GetDataWithTimeout(&rxp.start, (TickType_t)portMAX_DELAY);
		} while (rxp.start != 0xFF);

		uart2GetDataWithTimeout(&rxp.payloadLength, (TickType_t)portMAX_DELAY);

		if (rxp.payloadLength == 0)
		{
			xEventGroupSetBits(evGroup, CTS_EVENT);
			//DEBUG_PRINT("RECEIVED CTS\n");
		}
		else
		{
			//consolePrintf("%d\n", rxp.payloadLength);
			consoleFlush();
			for (int i = 0; i < rxp.payloadLength; i++)
			{
				uart2GetDataWithTimeout(&rxp.payload[i], (TickType_t)portMAX_DELAY);
			}

			uint8_t crc;
			uart2GetDataWithTimeout(&crc, (TickType_t)portMAX_DELAY);
			ASSERT(crc == calcCrc(&rxp));

			xQueueSend(rxQueue, &rxp, portMAX_DELAY);
			xEventGroupSetBits(evGroup, CTR_EVENT);
		}
	}
}

static void uartTxTask(void *param)
{
	systemWaitStart();
	DEBUG_PRINT("UART TX Started\n");

	uint8_t ctr[] = {0xFF, 0x00};
	EventBits_t evBits = 0;

	// We need to hold off here to make sure that the RX task
	// has started up and is waiting for chars, otherwise we might send
	// CTR and miss CTS (which means that the ESP32 will stop sending CTS
	// too early and we cannot sync)
	vTaskDelay(100);

	// Sync with ESP32 so both are in CTS
	do
	{
	uart2SendData(sizeof(ctr), (uint8_t *)&ctr);
	vTaskDelay(M2T(10));
	evBits = xEventGroupGetBits(evGroup);
	} while ((evBits & CTS_EVENT) != CTS_EVENT);

	while (1)
	{
		// If we have nothing to send then wait, either for something to be
		// queued or for a request to send CTR
		if (uxQueueMessagesWaiting(txQueue) == 0)
		{
			evBits = xEventGroupWaitBits(evGroup,
										CTR_EVENT | TXQ_EVENT,
										pdTRUE,  // Clear bits before returning
										pdFALSE, // Wait for any bit
										portMAX_DELAY);
			if ((evBits & CTR_EVENT) == CTR_EVENT)
			{
			uart2SendData(sizeof(ctr), (uint8_t *)&ctr);
			}
		}

		if (uxQueueMessagesWaiting(txQueue) > 0)
		{
			// Dequeue and wait for either CTS or CTR
			xQueueReceive(txQueue, &cpxTxp, 0);
			txp.start = 0xFF;
			assemblePacket(&cpxTxp, &txp);
			do
			{
			evBits = xEventGroupWaitBits(evGroup,
											CTR_EVENT | CTS_EVENT,
											pdTRUE,  // Clear bits before returning
											pdFALSE, // Wait for any bit
											portMAX_DELAY);
			if ((evBits & CTR_EVENT) == CTR_EVENT)
			{
				uart2SendData(sizeof(ctr), (uint8_t *)&ctr);
			}
			} while ((evBits & CTS_EVENT) != CTS_EVENT);
			uart2SendData((uint32_t) txp.payloadLength + UART_META_LENGTH, (uint8_t *)&txp);
		}
	}
}

void uartTransportInit()
{
    if (isInit)
        return;

    txQueue = xQueueCreate(TX_QUEUE_LENGTH, sizeof(CPXPacket_t));
    rxQueue = xQueueCreate(RX_QUEUE_LENGTH, sizeof(uart_transport_packet_t));
    evGroup = xEventGroupCreate();

    uart2Init(115200);

    xTaskCreate(uartRxTask, "UART RX", configMINIMAL_STACK_SIZE, NULL, UART_TASK_PRIORITY, NULL);
    xTaskCreate(uartTxTask, "UART TX", configMINIMAL_STACK_SIZE, NULL, UART_TASK_PRIORITY, NULL);

    isInit = true;
}

void uartTransportSend(const CPXPacket_t* packet) {
  /*
  ASSERT(packet->dataLength <= UART_TRANSPORT_MTU - CPX_ROUTING_PACKED_SIZE);
  xQueueSend(txQueue, packet, portMAX_DELAY);
  xEventGroupSetBits(evGroup, TXQ_EVENT);
  */
	xQueueSend(txQueue, packet, portMAX_DELAY);
	xEventGroupSetBits(evGroup, TXQ_EVENT);
}

void uartTransportReceive(CPXPacket_t* packet) {
  // Not reentrant safe. Assume only one task dequeues packets
  /*
  static uart_transport_packet_t rxp;
  DEBUG_PRINT("UART TR RX\n");

  xQueueReceive(rxQueue, &rxp, portMAX_DELAY);

  packet->dataLength = rxp.payloadLength - CPX_ROUTING_PACKED_SIZE;

  cpxPackedToRoute(&rxp.routablePayload.route, &packet->route);

  memcpy(packet->data, rxp.routablePayload.data, packet->dataLength);
  */
	static uart_transport_packet_t cpxRxp;

	xQueueReceive(rxQueue, &cpxRxp, portMAX_DELAY);

	packet->dataLength = (uint32_t) cpxRxp.payloadLength - CPX_ROUTING_PACKED_SIZE;
	packet->route.destination = cpxRxp.routablePayload.route.destination;
	packet->route.source = cpxRxp.routablePayload.route.source;
	packet->route.function = cpxRxp.routablePayload.route.function;
	packet->route.lastPacket = cpxRxp.routablePayload.route.lastPacket;
	memcpy(&packet->data, cpxRxp.routablePayload.data, packet->dataLength);
}