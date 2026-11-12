#pragma once

#include <stdbool.h>
#include <stdint.h>

void platform_init(void);
void platform_led_set(bool enabled);
bool platform_button_read(void);

float platform_voltage_read(void);
float platform_temperature_read(void);
void platform_charge_set(bool enabled);

#define PLATFORM_CAPTURE_MAX 2000
#define PLATFORM_BLOCK_MAX 8192
#define PLATFORM_SAMPLE_RATE_HZ 44100
#define PLATFORM_STREAM_BYTES 1024

typedef struct {
    const uint16_t *samples;
    uint32_t count;
    uint32_t period_us;
    uint32_t min_interval_us;
    uint32_t max_interval_us;
    uint32_t total_handler_us;
} platform_capture_t;

bool platform_blink_start(uint32_t period_ms);
uint32_t platform_blink_stop(void);
bool platform_adc_busy(void);
float platform_adc_volts(float code);

bool platform_capture_start(uint32_t count, uint32_t period_us);
bool platform_capture_done(void);
void platform_capture_get(platform_capture_t *capture);

bool platform_block_start(uint32_t count);
bool platform_block_done(void);
const uint16_t *platform_block_samples(void);
uint32_t platform_block_time_us(void);

bool platform_stream_start(uint8_t bits);
void platform_stream_stop(void);
const uint8_t *platform_stream_next(uint32_t *samples, uint16_t *number);
uint32_t platform_stream_overruns(void);
void platform_write(const void *data, uint32_t bytes);

bool platform_tone_set(uint32_t freq_hz);
bool platform_watchdog_enable(uint32_t timeout_ms);
bool platform_watchdog_is_enabled(void);
void platform_watchdog_feed(void);
bool platform_watchdog_caused_reset(uint32_t *restarts);
