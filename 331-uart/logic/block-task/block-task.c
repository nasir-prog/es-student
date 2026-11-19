#include "block-task/block-task.h"

#include <stddef.h>
#include "platform/platform.h"

static block_task_callback_t callback;
static uint32_t pending_count;

bool block_task_start(uint32_t count, block_task_callback_t on_block)
{
    if (callback || !on_block || !platform_block_start(count)) {
        return false;
    }
    pending_count = count;
    callback = on_block;
    return true;
}

void block_task_handle(void)
{
    if (!callback || !platform_block_done()) {
        return;
    }

    block_task_callback_t ready = callback;
    callback = NULL;
    ready(platform_block_samples(), pending_count, platform_block_time_us());
}
