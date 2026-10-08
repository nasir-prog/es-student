#pragma once

#include "pico/types.h"

typedef void (*command_handler_t)(void);

struct command_t
{
    const char *name;
    command_handler_t handler;
};

extern const struct command_t commands[];
extern const uint command_count;
