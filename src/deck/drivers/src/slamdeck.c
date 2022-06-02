#define DEBUG_MODULE "slamdeck"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "FreeRTOS.h"
#include "config.h"
#include "console.h"
#include "debug.h"
#include "deck.h"
#include "task.h"
#include "event_groups.h"
#include "queue.h"
#include "log.h"
#include "param.h"
#include "queue.h"
#include "system.h"

#include "crtp.h"
#include "slamdeck.h"
#include "cpx.h"
#include "cpx_transport_uart.h"
#include "cpx_transport_stm.h"
#include "cpx_transport_crtp.h"
#include "router.h"

static bool isInit = false;


#define get_current_time() (xTaskGetTickCount() / portTICK_PERIOD_MS)

static void print_sensor_data(uint8_t* buf)
{
    int grid = 0;
    int row_size = 8;
    for (int row = row_size-1; row >= 0; row--) {
        for (int col = 0; col < row_size; col++) {
            consolePrintf(" %d ", buf[grid]);
            grid++;
        }
        consolePrintf("\n");
    }
    consolePrintf("\n");
}

#define SLAMDECK_MAX_DATA_SIZE 640
#define SLAMDECK_GET_DATA 0
#define SLAMDECK_FRAME_SIZE 640

static int slamdeckDataRead = 0;
static uint8_t slamdeckData[SLAMDECK_MAX_DATA_SIZE];
static uint8_t slamdeckBuf[SLAMDECK_MAX_DATA_SIZE];

bool isStreaming = false;


static void slamdeckReadData()
{
  systemWaitStart();

  static CPXPacket_t cpxRx;

  while (1) {
    stmReceiveBlocking(&cpxRx);
    memcpy(&slamdeckBuf[slamdeckDataRead], cpxRx.data, cpxRx.dataLength);
    slamdeckDataRead += cpxRx.dataLength;

    if (cpxRx.route.lastPacket) {
        memcpy(slamdeckData, slamdeckBuf, slamdeckDataRead);
        slamdeckDataRead = 0;
    }

    vTaskDelay(M2T(5));
  }
}

static void slamdeckCrtpCB(CRTPPacket* packet)
{
  isStreaming = true;
  return;

	packet->port = CRTP_PORT_CPX;
	packet->channel = 0;

	static int dataSent = 0;

	int dataToSend = SLAMDECK_FRAME_SIZE - dataSent;
	if (dataToSend > CRTP_MAX_DATA_SIZE) {
			dataToSend = CRTP_MAX_DATA_SIZE;
	}

	memcpy(packet->data, &slamdeckData[dataSent], dataToSend);
	packet->size = dataToSend;

  // Last packet of frame
  bool lastPacket = dataSent >= SLAMDECK_FRAME_SIZE;
  if (lastPacket) {
    packet->channel = 1;
  }

	if (crtpSendPacket(packet) == pdTRUE) {
    if (lastPacket) {
      dataSent = 0;
    } else {
      dataSent += dataToSend;
    }
  }
}


static void startStreaming()
{
    static CPXPacket_t packet;
    cpxInitRoute(CPX_T_STM32, CPX_T_ESP32, CPX_F_APP, &packet.route);
    packet.data[0] = SLAMDECK_COMMAND_START_STREAMING;
    packet.dataLength = 1;
    stmSendBlocking(&packet);
}


static void slamdeckSendData();


static void slamdeckInit(DeckInfo *info)
{
    if (isInit)
        return;

    // Register new port callback with CRTP handler.
    crtpInit();
    crtpRegisterPortCB(CRTP_PORT_CPX, slamdeckCrtpCB);

    // Pull reset pin LOW, before initializing any UART.
    pinMode(DECK_GPIO_IO4, OUTPUT);
    digitalWrite(DECK_GPIO_IO4, LOW);

    uartTransportInit();
    stmTransportInit();
    crtpTransportInit();
    routerInit();

    // Initialize task for the ESP while it's held in reset
    //xTaskCreate(slamdeckReadData, "SDRead", configMINIMAL_STACK_SIZE, NULL, 2, NULL);
		isStreaming = false;
    //xTaskCreate(slamdeckSendData, "SDWrite", configMINIMAL_STACK_SIZE, NULL, 0, NULL);

    // Pull reset pin HIGH again, now that UART is initialized.
    digitalWrite(DECK_GPIO_IO4, HIGH);
    pinMode(DECK_GPIO_IO4, INPUT_PULLUP);

    //startStreaming();

    isInit = true;
}

static int slamdeckTest()
{
    return true;
}

static const DeckDriver slamdeck_deck = {
    .vid = 0xBC,
    .pid = 0x12,
    .name = "bcSLAM",

    .usedGpio = DECK_USING_IO_4,
    .usedPeriph = DECK_USING_UART2,

    .init = slamdeckInit,
    .test = slamdeckTest,
};

PARAM_GROUP_START(deck)
PARAM_ADD_CORE(PARAM_UINT8 | PARAM_RONLY, bcSLAMDeck, &isInit)
PARAM_GROUP_STOP(deck)

DECK_DRIVER(slamdeck_deck);


static void slamdeckSendData()
{
  systemWaitStart();
	CRTPPacket packet;
	packet.size = 30;
	packet.port = CRTP_PORT_CPX;

  while (1) {
    if (!crtpIsConnected()) {
      isStreaming = false;
    }

		if (!isStreaming) {
			vTaskDelay(M2T(10));
			continue;
		}

    //crtpSendPacketBlock(&packet);
		crtpSendPacket(&packet);
		vTaskDelay(M2T(3));
  }
}
