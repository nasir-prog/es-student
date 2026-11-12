#pragma once

#include <stdbool.h>
#include <stdint.h>

#define STREAM_SYNC_WORD 0x5aa5

typedef enum {
    STREAM_STATE_IDLE,
    STREAM_STATE_RUN
} stream_state_t;

typedef struct {
    uint16_t sync;
    uint16_t number;
    uint16_t samples;
} stream_header_t;

typedef void (*stream_task_callback_t)(const stream_header_t *header, const uint8_t *data,
                                      uint32_t bytes);

void stream_task_init(stream_task_callback_t on_frame);
void stream_task_handle(void);
bool stream_task_start(uint8_t bits);
void stream_task_stop(void);
stream_state_t stream_task_get_state(void);
uint32_t stream_task_get_frames(void);
uint32_t stream_task_get_samples(void);
uint32_t stream_task_get_overruns(void);
