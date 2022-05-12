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

#ifndef D686C79D_0661_4A28_9B64_5075B31D3FC7
#define D686C79D_0661_4A28_9B64_5075B31D3FC7


#include <stdint.h>
#include <stddef.h>

#include "cpx.h"
#include "cpx_transport_stm.h"


void comInit();

void comReceiveTestBlocking(stm_routable_packet_t * packet);

void comReceiveSystemBlocking(stm_routable_packet_t * packet);

void comReceiveAppBlocking(stm_routable_packet_t * packet);


#endif /* D686C79D_0661_4A28_9B64_5075B31D3FC7 */
