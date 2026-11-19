#pragma once

#include "platform/platform.h"

typedef void (*edge_task_callback_t)(platform_line_mode_t mode, bool ok, uint32_t rise_us,
                                     uint32_t fall_us);

bool edge_task_start(platform_line_mode_t mode, edge_task_callback_t on_done);
bool edge_task_active(void);
void edge_task_handle(void);
