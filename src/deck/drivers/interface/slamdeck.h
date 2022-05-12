#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint8_t command;
    uint8_t data[100];
} slamdeck_packet_t;

typedef enum {
    SLAMDECK_COMMAND_GET_DATA        = 0,
    SLAMDECK_COMMAND_GET_SETTINGS    = 1,
    SLAMDECK_COMMAND_SET_SETTINGS    = 2,
    SLAMDECK_COMMAND_START_STREAMING = 3,
    SLAMDECK_COMMAND_STOP_STREAMING  = 4
} slamdeck_command_e;
