#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    PI_IDLE,
    PI_RUNNING,
    PI_READY
} pi_state_t;

typedef struct {
    double value;
    uint64_t time_ms;
} pi_result_t;

bool pi_task_start(uint32_t terms);
void pi_task_handle(void);
pi_state_t pi_task_get_result(pi_result_t *result);
