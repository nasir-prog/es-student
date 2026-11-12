#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef MODEL_RECORDER
#include <setjmp.h>
#include <signal.h>
#include <unistd.h>
#endif

#include "api/api.h"
#include "button-task/button-task.h"
#include "device/device.h"
#include "led-task/led-task.h"
#include "pi-task/pi-task.h"
#include "platform/platform.h"
#include "profiling/profiling.h"
#ifdef MODEL_RECORDER
#include "block-task/block-task.h"
#endif

// The scheduler advances by 250 simulated microseconds per iteration.
#define TICK_US 250
enum { WATCH_LOOP, WATCH_COMMAND, WATCH_LED, WATCH_BUTTON, WATCH_PI };

static uint64_t now_us;
static uint64_t boot_us;
static int input = -1;

int getchar_timeout_us(uint32_t timeout_us)
{
    assert(timeout_us == 0);
    const int ch = input;
    input = -1;
    return ch;
}

uint64_t time_us_64(void) { return now_us - boot_us; }
void platform_led_set(bool level) { (void)level; }
bool platform_button_read(void) { return false; }

void device_get_info(device_info_t *info)
{
    *info = (device_info_t){
        .board = "pico",
        .serial = "0000000000000000",
        .sdk_version = "2.3.0",
    };
}

#ifdef MODEL_ADC
static uint16_t adc_input(uint64_t sample_us)
{
    return (uint16_t)lround(1552 + 20 * sin(sample_us / 250000.0));
}

float platform_voltage_read(void)
{
    return adc_input(now_us) * (3.3f / 4096.0f);
}

float platform_temperature_read(void)
{
    const float voltage = (875 + now_us / 1000000 % 3) * (3.3f / 4096.0f);
    return 27.0f - (voltage - 0.706f) / 0.001721f;
}

void platform_charge_set(bool enabled) { (void)enabled; }
#endif

#ifdef MODEL_RECORDER
static uint64_t blink_started;
static uint32_t blink_period;
static uint32_t blink_count;
static uint32_t capture_count;
static uint32_t capture_period;
static uint32_t capture_low;
static uint32_t capture_high;
static uint64_t capture_end;
static uint32_t block_us;
static uint64_t block_end;
static uint16_t capture_codes[PLATFORM_CAPTURE_MAX];
static uint16_t block_codes[PLATFORM_BLOCK_MAX];
static uint32_t watchdog_timeout;
static uint32_t restarts;
static sigjmp_buf watchdog_jump;

bool platform_blink_start(uint32_t period_ms)
{
    blink_started = now_us;
    blink_period = period_ms;
    blink_count = 0;
    return true;
}

uint32_t platform_blink_stop(void)
{
    if (blink_period != 0) {
        blink_count = (uint32_t)((now_us - blink_started) / (1000u * blink_period));
        blink_period = 0;
    }
    return blink_count;
}

bool platform_adc_busy(void)
{
    return now_us < capture_end || now_us < block_end;
}

float platform_adc_volts(float code) { return code * (3.3f / 4096.0f); }

bool platform_capture_start(uint32_t count, uint32_t period_us)
{
    static const int offsets[] = {0, 2, -2, 1, -1};
    if (platform_adc_busy()) {
        return false;
    }
    capture_count = count;
    capture_period = period_us;
    capture_low = period_us;
    capture_high = period_us;
    uint64_t previous = now_us;
    for (uint32_t i = 0; i < count; ++i) {
        const uint64_t timestamp = now_us + (uint64_t)(i + 1) * period_us +
                                   offsets[i % (sizeof(offsets) / sizeof(offsets[0]))];
        const uint32_t interval = (uint32_t)(timestamp - previous);
        if (interval < capture_low) {
            capture_low = interval;
        }
        if (interval > capture_high) {
            capture_high = interval;
        }
        capture_codes[i] = adc_input(timestamp);
        previous = timestamp;
    }
    capture_end = previous;
    return true;
}

bool platform_capture_done(void) { return capture_count != 0 && now_us >= capture_end; }

void platform_capture_get(platform_capture_t *capture)
{
    // The model assigns 2 us to each ADC timer handler.
    *capture = (platform_capture_t){
        capture_codes, capture_count, capture_period, capture_low, capture_high, capture_count * 2,
    };
}

bool platform_block_start(uint32_t count)
{
    if (platform_adc_busy()) {
        return false;
    }
    block_us = (uint32_t)((uint64_t)count * 1000000 / PLATFORM_SAMPLE_RATE_HZ);
    block_end = now_us + block_us;
    for (uint32_t i = 0; i < count; ++i) {
        block_codes[i] = adc_input(now_us + (uint64_t)i * 1000000 / PLATFORM_SAMPLE_RATE_HZ);
    }
    return true;
}

bool platform_block_done(void) { return now_us >= block_end; }
const uint16_t *platform_block_samples(void) { return block_codes; }
uint32_t platform_block_time_us(void) { return block_us; }

bool platform_stream_start(uint8_t bits) { (void)bits; return false; }
void platform_stream_stop(void) {}
const uint8_t *platform_stream_next(uint32_t *samples, uint16_t *number)
{
    (void)samples;
    (void)number;
    return NULL;
}
uint32_t platform_stream_overruns(void) { return 0; }
void platform_write(const void *data, uint32_t bytes) { (void)data; (void)bytes; }

bool platform_tone_set(uint32_t hz) { return hz == 0 || (hz >= 20 && hz <= 20000); }

bool platform_watchdog_enable(uint32_t timeout_ms)
{
    if (timeout_ms == 0 || timeout_ms > 8388) {
        return false;
    }
    watchdog_timeout = timeout_ms;
    return true;
}

bool platform_watchdog_is_enabled(void) { return watchdog_timeout != 0; }
bool platform_watchdog_caused_reset(uint32_t *count) { *count = restarts; return restarts != 0; }

static void reset_from_alarm(int signal_number)
{
    (void)signal_number;
    siglongjmp(watchdog_jump, 1);
}

static void model_watchdog_reset(void)
{
    const uint64_t wait_us = watchdog_timeout * 1000u;
    struct sigaction action = {.sa_handler = reset_from_alarm};
    sigemptyset(&action.sa_mask);
    assert(sigaction(SIGALRM, &action, NULL) == 0);
    if (sigsetjmp(watchdog_jump, 1) == 0) {
        alarm(1);
        // Run the production command, including its intentional main-loop stall.
        api_handle(&(command_t){.name = "hang"});
        alarm(0);
        return;
    }
    ++restarts;
    watchdog_timeout = 0;
    boot_us = now_us + wait_us;
    now_us = boot_us;
    stdio_text_protocol_init();
    puts("@@RECONNECT@@");
}
#endif

static void dispatch(const command_t *command)
{
#ifdef MODEL_RECORDER
    if (!command->truncated && strcmp(command->name, "hang") == 0 && watchdog_timeout) {
        model_watchdog_reset();
        return;
    }
#endif
    api_handle(command);
}

static void poll_input(void)
{
    const command_t *command = stdio_text_protocol_handle();
    if (command) {
        dispatch(command);
    }
}

static void tick(void)
{
    const uint64_t start = now_us;
    profiling_start(WATCH_LOOP);
    profiling_start(WATCH_COMMAND);
    poll_input();
    now_us += 2;
    profiling_stop(WATCH_COMMAND);

    profiling_start(WATCH_LED);
    led_task_handle();
    now_us += 3;
    profiling_stop(WATCH_LED);

    profiling_start(WATCH_BUTTON);
    button_task_handle();
    now_us += 3;
    profiling_stop(WATCH_BUTTON);

    profiling_start(WATCH_PI);
    pi_task_handle();
    now_us = start + TICK_US;
    profiling_stop(WATCH_PI);
#ifdef MODEL_ADC
    telemetry_task_handle();
#endif
#ifdef MODEL_RECORDER
    block_task_handle();
#endif
    profiling_stop(WATCH_LOOP);
}

int main(void)
{
    stdio_text_protocol_init();
    led_task_init();
    button_task_init(led_task_next_state);
#ifdef MODEL_ADC
    telemetry_task_init(api_print_telemetry);
#endif
    profiling_stopwatch_init(WATCH_LOOP, "loop");
    profiling_stopwatch_init(WATCH_COMMAND, "command");
    profiling_stopwatch_init(WATCH_LED, "led");
    profiling_stopwatch_init(WATCH_BUTTON, "button");
    profiling_stopwatch_init(WATCH_PI, "pi");

    uint64_t listen_us;
    char hex[513];
    while (scanf("%" SCNu64 " %512s", &listen_us, hex) == 2) {
        if (strcmp(hex, "-") != 0) {
            for (size_t index = 0; hex[index] != '\0'; index += 2) {
                const char byte[] = {hex[index], hex[index + 1], '\0'};
                input = (int)strtoul(byte, NULL, 16);
                poll_input();
            }
        }
        for (uint64_t elapsed = 0; elapsed < listen_us; elapsed += TICK_US) {
            tick();
        }
        puts("\n@@END@@");
    }
    return 0;
}
