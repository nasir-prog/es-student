#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef void (*block_task_callback_t)(const uint16_t *samples, uint32_t count, uint32_t time_us);

bool block_task_start(uint32_t count, block_task_callback_t on_block);
void block_task_handle(void);
