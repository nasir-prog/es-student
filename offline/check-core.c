#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "api/api.h"
#include "button-task/button-task.h"
#include "device/device.h"
#include "led-task/led-task.h"
#include "pi-task/pi-task.h"
#include "platform/platform.h"
#include "profiling/profiling.h"
#include "systime/systime.h"

static uint64_t now_us;
static int input = -1;
static bool button_level;
static bool led_level;
static telemetry_record_t records[4];
static unsigned record_count;

int getchar_timeout_us(uint32_t timeout_us)
{
    assert(timeout_us == 0);
    int ch = input;
    input = -1;
    return ch;
}

uint64_t time_us_64(void)
{
    return now_us;
}

void platform_led_set(bool level)
{
    led_level = level;
}

bool platform_button_read(void)
{
    return button_level;
}

float platform_voltage_read(void)
{
    return 1.625f;
}

float platform_temperature_read(void)
{
    return 25.0f;
}

void platform_charge_set(bool enabled)
{
    (void)enabled;
}

void device_get_info(device_info_t *info)
{
    *info = (device_info_t){.board = "model", .sdk_version = "model", .serial = "model"};
}

static void frame(const char *text)
{
    for (; *text; ++text) {
        input = (unsigned char)*text;
        const command_t *command = stdio_text_protocol_handle();
        if (command) {
            api_handle(command);
        }
    }
}

static void capture(const telemetry_record_t *record)
{
    assert(record_count < sizeof(records) / sizeof(records[0]));
    records[record_count++] = *record;
}

int main(void)
{
    stdio_text_protocol_init();
    led_task_init();
    button_task_init(led_task_next_state);
    assert(led_level);

    now_us = 500020;
    led_task_handle();
    assert(!led_level);
    now_us = 1000000;
    led_task_handle();
    assert(led_level);

    button_level = true;
    button_task_handle();
    now_us += 1000;
    button_level = false;
    button_task_handle();
    assert(button_task_get_press_count() == 0);
    button_level = true;
    button_task_handle();
    now_us += 19999;
    button_task_handle();
    assert(!button_task_is_pressed());
    ++now_us;
    button_task_handle();
    assert(button_task_is_pressed() && button_task_get_press_count() == 1);
    assert(led_task_get_state() == LED_STATE_OFF);
    now_us += 400000;
    button_task_handle();
    assert(button_task_get_press_count() == 1);

    profiling_stopwatch_init(0, "loop");
    profiling_start(0);
    now_us += 6;
    profiling_stop(0);
    profiling_result_t metrics;
    assert(profiling_get(0, &metrics) && metrics.count == 1 && metrics.max_us == 6);
    profiling_reset();
    assert(profiling_get(0, &metrics) && metrics.count == 0);

    frame("info\r");
    frame("  upx\x7ftime   \r\n");
    frame("led_period 4294967295\r");
    assert(led_task_get_period_ms() == UINT32_MAX);
    frame("led_period 4294967296\r");
    frame("led_period 1 2 3 4 5\r");
    frame("led_period 0\r");
    frame("pi_start\r");
    frame("pi\r");
    frame("pi_start 5\r");
    frame("p");
    pi_result_t result;
    assert(pi_task_get_result(&result) == PI_RUNNING);
    for (unsigned i = 0; i < 20000; ++i) {
        now_us += 250;
        pi_task_handle();
    }
    frame("i\r");
    assert(pi_task_get_result(&result) == PI_READY);
    assert(fabs(result.value - acos(-1.0)) <= 1.001e-6);

    telemetry_task_init(capture);
    assert(!telemetry_task_start(5));
    assert(telemetry_task_start(50));
    telemetry_task_handle();
    now_us += 49999;
    telemetry_task_handle();
    assert(record_count == 1);
    ++now_us;
    telemetry_task_handle();
    assert(record_count == 2 && records[1].time_us - records[0].time_us == 50000);
    assert(records[1].voltage_v == 1.625f && records[1].temp_c == 25.0f);
    telemetry_task_stop();
    now_us += 50000;
    telemetry_task_handle();
    assert(record_count == 2);

    puts("MODELED INPUTS: protocol, task scheduling, debounce, stepped pi, profiling and telemetry OK");
    return 0;
}
