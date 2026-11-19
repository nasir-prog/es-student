#include "edge-task/edge-task.h"

#include "systime/systime.h"

#define TIMEOUT_US 2000000u

typedef enum { IDLE, DISCHARGE, RISE, CHARGE, FALL } state_t;

static state_t state;
static platform_line_mode_t line_mode;
static edge_task_callback_t callback;
static uint64_t phase_started;
static uint32_t rise_us;

static void enter(state_t next, bool high)
{
    platform_line_set(line_mode, high);
    phase_started = systime_us();
    state = next;
}

static void finish(bool ok, uint32_t fall_us)
{
    state = IDLE;
    callback(line_mode, ok, rise_us, fall_us);
}

bool edge_task_active(void)
{
    return state != IDLE;
}

bool edge_task_start(platform_line_mode_t mode, edge_task_callback_t on_done)
{
    if (edge_task_active() || !on_done) {
        return false;
    }
    line_mode = mode;
    callback = on_done;
    rise_us = 0;
    enter(DISCHARGE, false);
    return true;
}

void edge_task_handle(void)
{
    if (!edge_task_active()) {
        return;
    }
    uint64_t elapsed = systime_us() - phase_started;
    float voltage = platform_voltage_read();
    if (elapsed >= TIMEOUT_US) {
        platform_line_set(line_mode, false);
        finish(false, 0);
        return;
    }
    switch (state) {
    case DISCHARGE:
        if (voltage < 0.05f) {
            enter(RISE, true);
        }
        break;
    case RISE:
        if (voltage > 2.0f) {
            rise_us = (uint32_t)elapsed;
            phase_started = systime_us();
            state = CHARGE;
        }
        break;
    case CHARGE:
        if (voltage > 3.2f) {
            enter(FALL, false);
        }
        break;
    case FALL:
        if (voltage < 1.3f) {
            finish(true, (uint32_t)elapsed);
        }
        break;
    case IDLE:
        break;
    }
}
