#include "telemetry-task/telemetry-task.h"

#include "platform/platform.h"
#include "systime/systime.h"

static telemetry_state_t state;
static telemetry_task_callback_t callback;
static uint32_t period_ms;
static uint64_t last_sample_us;

void telemetry_task_init(telemetry_task_callback_t on_record)
{
    callback = on_record;
    state = TELEMETRY_STATE_IDLE;
}

bool telemetry_task_start(uint32_t requested_ms)
{
    if (requested_ms < 10) {
        return false;
    }
    period_ms = requested_ms;
    last_sample_us = systime_us() - (uint64_t)period_ms * 1000;
    state = TELEMETRY_STATE_RUN;
    return true;
}

void telemetry_task_stop(void)
{
    state = TELEMETRY_STATE_IDLE;
}

telemetry_state_t telemetry_task_get_state(void)
{
    return state;
}

uint32_t telemetry_task_get_period_ms(void)
{
    return period_ms;
}

void telemetry_task_handle(void)
{
    if (state != TELEMETRY_STATE_RUN ||
        !systime_period_elapsed(&last_sample_us, (uint64_t)period_ms * 1000)) {
        return;
    }
    telemetry_record_t record;
    record.time_us = systime_us();
    record.voltage_v = platform_voltage_read();
    record.temp_c = platform_temperature_read();
    callback(&record);
}
