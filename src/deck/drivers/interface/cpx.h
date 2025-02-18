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

#ifndef CC403C54_C845_4BA0_8684_E7BCF7570287
#define CC403C54_C845_4BA0_8684_E7BCF7570287


#include <stdint.h>
#include <stdbool.h>

// This enum is used to identify source and destination for CPX routing information
typedef enum {
  CPX_T_STM32 = 1, // The STM in the Crazyflie
  CPX_T_ESP32 = 2, // The ESP on the AI-deck
  CPX_T_HOST = 3,  // A remote computer connected via Wifi
  CPX_T_GAP8 = 4,   // The GAP8 on the AI-deck
  CPX_T_HOST_NRF = 5 // Host through NRF radio
} CPXTarget_t;

typedef enum {
  CPX_F_SYSTEM = 1,
  CPX_F_CONSOLE = 2,
  CPX_F_CRTP = 3,
  CPX_F_WIFI_CTRL = 4,
  CPX_F_APP = 5,
  CPX_F_TEST = 0x0E,
  CPX_F_BOOTLOADER = 0x0F
} CPXFunction_t;

typedef struct {
  CPXTarget_t destination;
  CPXTarget_t source;
  bool lastPacket;
  CPXFunction_t function;
} CPXRouting_t;

// This struct contains routing information in a packed format. This struct
// should mainly be used to serialize data when tranfering. Unpacked formats
// should be preferred in application code.
typedef struct {
  CPXTarget_t destination : 3;
  CPXTarget_t source : 3;
  bool lastPacket : 1;
  bool reserved : 1;
  CPXFunction_t function : 8;
} __attribute__((packed)) CPXRoutingPacked_t;

#define CPX_ROUTING_PACKED_SIZE 2 //(sizeof(CPXRoutingPacked_t))

// The maximum MTU of any link
#define CPX_MAX_PAYLOAD_SIZE 1022

typedef struct {
  CPXRouting_t route;
  uint16_t dataLength;
  uint8_t data[CPX_MAX_PAYLOAD_SIZE - CPX_ROUTING_PACKED_SIZE];
} CPXPacket_t;

/**
 * @brief Initialize CPX routing data.
 *
 * Initialize values and set lastPacket to true.
 *
 * @param source The starting point of the packet
 * @param destination The destination to send the packet to
 * @param function The function of the content
 * @param route Pointer to the route data to initialize
 */
void cpxInitRoute(const CPXTarget_t source, const CPXTarget_t destination, const CPXFunction_t function, CPXRouting_t* route);
void cpxRouteToPacked(const CPXRouting_t* route, CPXRoutingPacked_t* packed);
void cpxPackedToRoute(const CPXRoutingPacked_t* packed, CPXRouting_t* route);


#endif /* CC403C54_C845_4BA0_8684_E7BCF7570287 */
