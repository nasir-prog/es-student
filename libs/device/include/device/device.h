#pragma once

#include <stdint.h>

typedef struct {
    const char *board;
    const char *sdk_version;
    char serial[17];
    uint32_t manufacturer;
    uint32_t part;
    uint32_t revision;
} device_info_t;

void device_get_info(device_info_t *info);
