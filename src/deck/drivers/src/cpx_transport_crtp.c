#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "FreeRTOS.h"
#include "debug.h"
#include "deck.h"
#include "task.h"
#include "event_groups.h"
#include "queue.h"
#include "system.h"

#include "cpx.h"
#include "cpx_transport_crtp.h"

static bool isInit = false;
static uint8_t byte;

#define TX_QUEUE_LENGTH 4
#define RX_QUEUE_LENGTH 4

#define CRTP_TASK_PRIORITY 2

static xQueueHandle txQueue;
static xQueueHandle rxQueue;

typedef struct {
    CPXRoutingPacked_t route;
    uint8_t data[CRTP_TRANSPORT_MTU - CPX_ROUTING_PACKED_SIZE];
} __attribute__((packed)) crtpTransportPayload_t;

typedef struct {
    uint8_t payloadLength;
    union {
        crtpTransportPayload_t routablePayload;
        uint8_t payload[CRTP_TRANSPORT_MTU];
    };

} __attribute__((packed)) crtp_transport_packet_t;


static crtp_transport_packet_t rxp;

// These two flags are used to know when it's OK to start sending data through
// CPX through CRTP. To start sending data through CPX we must first receive
// a packet. If this is not done, the connection process and TOC download
// can be very slow as the link is congested by CPX packets.
static bool sending = false;
static bool sendRequest = false;


static void crtpCpxCallback(CRTPPacket* packet)
{
    // Copy CRTP data into transport payload.
    memcpy(&rxp.payload, packet->data, packet->size);
    rxp.payloadLength = packet->size; // CRTP packet size is data + header.

    sendRequest = true;

    // TODO: Create CPX packet and put in CPX queue?
}

void crtpTxTask()
{
    systemWaitStart();

    static CPXPacket_t cpxPacket;
    static CPXRoutingPacked_t routePacked;

    static CRTPPacket crtpPacket = {
        .port = CRTP_PORT_CPX,
        .channel = 0
    };

    while (1) {
        if (!crtpIsConnected()) {
            sending = false;
        }

        if (sendRequest) {
            sendRequest = false;
            sending = true;
        }

        if (!sending) {
            vTaskDelay(10 / portTICK_PERIOD_MS);
            continue;
        }

        // Get packet from queue
        xQueueReceive(txQueue, &cpxPacket, portMAX_DELAY);
        //DEBUG_PRINT("PKT\n");

        cpxRouteToPacked(&cpxPacket.route, &routePacked);

        // Copy route header
        memcpy(crtpPacket.data, &routePacked, CPX_ROUTING_PACKED_SIZE);

        // Copy payload data
        memcpy(&crtpPacket.data[CPX_ROUTING_PACKED_SIZE], cpxPacket.data, cpxPacket.dataLength);

        //setCpxPayloadLength(&crtpPacket, cpxPacket.dataLength);
        crtpPacket.size = cpxPacket.dataLength + CPX_ROUTING_PACKED_SIZE;

        crtpSendPacketBlock(&crtpPacket);
        vTaskDelay(M2T(3));
    }
}

static StaticQueue_t xTxQueue;
uint8_t txBuf[TX_QUEUE_LENGTH * sizeof(CPXPacket_t)];
static StaticQueue_t xRxQueue;
uint8_t rxBuf[RX_QUEUE_LENGTH * sizeof(crtp_transport_packet_t)];

void crtpTransportInit()
{
    if (isInit)
        return;

    // Register new port callback with CRTP handler.
    crtpInit();
    crtpRegisterPortCB(CRTP_PORT_CPX, crtpCpxCallback);

    txQueue = xQueueCreateStatic(TX_QUEUE_LENGTH, sizeof(CPXPacket_t), txBuf, &xTxQueue);
    rxQueue = xQueueCreateStatic(RX_QUEUE_LENGTH, sizeof(crtp_transport_packet_t), rxBuf, &xRxQueue);

    xTaskCreate(crtpTxTask, "CRTP TX", configMINIMAL_STACK_SIZE, NULL, CRTP_TASK_PRIORITY, NULL);

    //DEBUG_PRINT("CRTP Initialized\n");
    isInit = true;
}

void crtpTransportSend(const CPXPacket_t* cpxPacket)
{
    ASSERT(cpxPacket->dataLength <= CRTP_TRANSPORT_MTU - CPX_ROUTING_PACKED_SIZE);
    xQueueSend(txQueue, cpxPacket, portMAX_DELAY);
}

void crtpTransportReceive(CPXPacket_t* packet)
{
    static crtp_transport_packet_t rxp;
    xQueueReceive(rxQueue, &rxp, portMAX_DELAY);

    packet->dataLength = rxp.payloadLength - CPX_ROUTING_PACKED_SIZE;
    cpxPackedToRoute(&rxp.routablePayload.route, &packet->route);
    memcpy(packet->data, rxp.routablePayload.data, packet->dataLength);
}

