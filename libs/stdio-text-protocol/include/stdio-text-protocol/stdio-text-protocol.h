#pragma once

#include <stdbool.h>

typedef struct {
    const char *name;
    unsigned argc;
    const char *argv[4];
    bool truncated;
} command_t;

void stdio_text_protocol_init(void);
const command_t *stdio_text_protocol_handle(void);
