#pragma once

#include "stdio-text-protocol/stdio-text-protocol.h"

typedef struct {
    const char *name;
    const char *help;
    void (*callback)(const command_t *);
} api_command_t;

extern const api_command_t api_commands[];
