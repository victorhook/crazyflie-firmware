/**
 * ,---------,       ____  _ __
 * |  ,-^-,  |      / __ )(_) /_______________ _____  ___
 * | (  O  ) |     / __  / / __/ ___/ ___/ __ `/_  / / _ \
 * | / ,--´  |    / /_/ / / /_/ /__/ /  / /_/ / / /_/  __/
 *    +------`   /_____/_/\__/\___/_/   \__,_/ /___/\___/
 *
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

#pragma once

// The STM transport module represents a virtual transport link between the router and application code executing in the CF

#include <stdint.h>
#include <stddef.h>
#include "cpx.h"

#define STM_TRANSPORT_MTU 128

#if STM_TRANSPORT_MTU > CPX_MAX_PAYLOAD_SIZE
    #pragma warn "cf MTU bigger than defined by CPX"
#endif


// stm_routable_packet_t is identical to CPXPacket_t
typedef CPXPacket_t stm_routable_packet_t;

void stmTransportInit();

// Interface used by the router
void stmTransportSend(const CPXPacket_t* packet);
void stmTransportReceive(CPXPacket_t* packet);

// Interface used by cf applications to exchange CPX packets with other part of the system
void stmSendBlocking(const CPXPacket_t* packet);
void stmReceiveBlocking(CPXPacket_t* packet);