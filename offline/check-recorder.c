#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "api/api.h"
#include "block-task/block-task.h"
#include "device/device.h"
#include "platform/platform.h"
#include "stream-task/stream-task.h"

static bool busy;
static bool captured;
static bool block_ready;
static bool watchdog;
static bool modeled_restart;
static bool frame_ready;
static uint32_t tone;
static uint32_t blink_period;
static uint32_t block_size;
static uint8_t stream_bits;
static uint16_t samples[PLATFORM_BLOCK_MAX];
static uint8_t payload[PLATFORM_STREAM_BYTES];
static FILE *frames;

int getchar_timeout_us(uint32_t timeout_us)
{
    assert(timeout_us == 0);
    return -1;
}

uint64_t time_us_64(void)
{
    return 125000;
}

void platform_led_set(bool level) { (void)level; }
bool platform_button_read(void) { return false; }
float platform_voltage_read(void) { return 1.25f; }
float platform_temperature_read(void) { return 26.0f; }
void platform_charge_set(bool enabled) { (void)enabled; }
bool platform_adc_busy(void) { return busy; }
float platform_adc_volts(float code) { return code * 3.3f / 4096; }

bool platform_blink_start(uint32_t period_ms)
{
    blink_period = period_ms;
    return true;
}

uint32_t platform_blink_stop(void)
{
    blink_period = 0;
    return 24;
}

bool platform_capture_start(uint32_t count, uint32_t period)
{
    assert(count == 4 && period == 125);
    if (busy) {
        return false;
    }
    busy = true;
    captured = false;
    return true;
}

bool platform_capture_done(void) { return captured; }
void platform_capture_get(platform_capture_t *capture)
{
    static const uint16_t codes[] = {1551, 1552, 1553, 1554};
    *capture = (platform_capture_t){codes, 4, 125, 124, 126, 8};
}

bool platform_block_start(uint32_t count)
{
    if (busy) {
        return false;
    }
    block_size = count;
    block_ready = false;
    busy = true;
    for (uint32_t i = 0; i < count; ++i) {
        samples[i] = i % 2 ? 1540 : 1564;
    }
    return true;
}

bool platform_block_done(void) { return block_ready; }
const uint16_t *platform_block_samples(void) { return samples; }
uint32_t platform_block_time_us(void)
{
    return (uint32_t)(((uint64_t)block_size * 1000000 + 22050) / 44100);
}

bool platform_stream_start(uint8_t bits)
{
    if (busy) {
        return false;
    }
    busy = true;
    frame_ready = true;
    stream_bits = bits;
    memset(payload, 0x0a, sizeof(payload));
    return true;
}

void platform_stream_stop(void) { busy = false; }
const uint8_t *platform_stream_next(uint32_t *count, uint16_t *number)
{
    if (!frame_ready) {
        return NULL;
    }
    frame_ready = false;
    *count = stream_bits == 8 ? 1024 : 512;
    *number = 0;
    return payload;
}
uint32_t platform_stream_overruns(void) { return 0; }

void platform_write(const void *data, uint32_t bytes)
{
    assert(bytes == 1030);
    assert(fwrite(data, 1, bytes, frames) == bytes);
}

bool platform_tone_set(uint32_t hz)
{
    if (hz != 0 && (hz < 20 || hz > 20000)) {
        return false;
    }
    tone = hz;
    return true;
}

bool platform_watchdog_enable(uint32_t ms)
{
    watchdog = ms > 0 && ms <= 8388;
    return watchdog;
}

bool platform_watchdog_is_enabled(void) { return watchdog; }
bool platform_watchdog_caused_reset(uint32_t *restarts)
{
    *restarts = modeled_restart ? 1 : 0;
    return modeled_restart;
}

static void command(const char *name, const char *arg1, const char *arg2)
{
    command_t cmd = {.name = name, .argc = arg2 ? 2u : arg1 ? 1u : 0u, .argv = {arg1, arg2}};
    api_handle(&cmd);
}

static void finish_block(void)
{
    busy = false;
    block_ready = true;
    block_task_handle();
}

void device_get_info(device_info_t *info) { (void)info; }

int main(int argc, char **argv)
{
    assert(argc == 2);
    frames = fopen(argv[1], "wb");
    assert(frames);
    puts("Учебный прогон 3.2. Источник входных данных — программная модель.");

    stream_task_init(api_send_frame);
    command("timer_blink_start", "250", NULL);
    assert(blink_period == 250);
    command("uptime", NULL, NULL);
    command("timer_blink_stop", NULL, NULL);

    command("cap_start", "4", "125");
    assert(busy);
    command("get_voltage", NULL, NULL);
    captured = true;
    busy = false;
    command("cap_dump", NULL, NULL);

    command("dma_cap", "4410", NULL);
    assert(busy && block_size == 4410);
    block_task_handle();
    finish_block();

    command("stream_start", NULL, NULL);
    assert(busy && stream_bits == 16 && blink_period == 250);
    stream_task_handle();
    stream_task_handle();
    assert(stream_task_get_frames() == 1 && stream_task_get_samples() == 512);
    command("stream_stop", NULL, NULL);
    assert(!busy && blink_period == 0);

    command("generate_tone", "440", NULL);
    assert(tone == 440);
    command("generate_tone", "0", NULL);
    assert(tone == 0);

    command("reset_reason", NULL, NULL);
    command("wdt_enable", "0", NULL);
    command("hang", NULL, NULL);
    command("wdt_enable", "2000", NULL);
    assert(watchdog);
    modeled_restart = true;
    command("reset_reason", NULL, NULL);

    command("mic_bias", NULL, NULL);
    finish_block();
    command("mic_level", "100", NULL);
    finish_block();

    command("stream_start", "8", NULL);
    assert(stream_bits == 8);
    stream_task_handle();
    assert(stream_task_get_frames() == 1 && stream_task_get_samples() == 1024);
    command("stream_stop", NULL, NULL);
    assert(fclose(frames) == 0);
    return 0;
}
