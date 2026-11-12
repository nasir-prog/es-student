#include "stream-task/stream-task.h"

#include "platform/platform.h"

static stream_task_callback_t callback;
static stream_state_t state;
static uint32_t frames;
static uint32_t total_samples;

void stream_task_init(stream_task_callback_t on_frame)
{
    callback = on_frame;
}

bool stream_task_start(uint8_t bits)
{
    if (state == STREAM_STATE_RUN || !callback || !platform_stream_start(bits)) {
        return false;
    }
    frames = 0;
    total_samples = 0;
    state = STREAM_STATE_RUN;
    return true;
}

void stream_task_stop(void)
{
    platform_stream_stop();
    state = STREAM_STATE_IDLE;
}

stream_state_t stream_task_get_state(void)
{
    return state;
}

uint32_t stream_task_get_frames(void)
{
    return frames;
}

uint32_t stream_task_get_samples(void)
{
    return total_samples;
}

uint32_t stream_task_get_overruns(void)
{
    return platform_stream_overruns();
}

void stream_task_handle(void)
{
    if (state != STREAM_STATE_RUN) {
        return;
    }

    uint32_t samples;
    uint16_t number;
    const uint8_t *data = platform_stream_next(&samples, &number);
    if (data) {
        const stream_header_t header = {STREAM_SYNC_WORD, number, (uint16_t)samples};
        callback(&header, data, PLATFORM_STREAM_BYTES);
        ++frames;
        total_samples += samples;
    }
}
