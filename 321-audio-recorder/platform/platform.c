#include "platform/platform.h"

#include <string.h>
#include "hardware/adc.h"
#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/pwm.h"
#include "hardware/sync.h"
#include "hardware/watchdog.h"
#include "pico/stdlib.h"

#define LED_PIN PICO_DEFAULT_LED_PIN
#define BUTTON_PIN 15
#define TONE_PIN 20
#define CHARGE_PIN 22
#define ADC_PIN 26
#define ADC_CHANNEL 0
#define TEMP_CHANNEL 4
#define ADC_VREF_V 3.3f
#define ADC_LEVELS 4096
#define ADC_CLOCK_HZ 48000000.0f
#define TONE_COUNT_HZ 1000000u
#define WATCHDOG_MAX_MS 8388
#define STREAM_RING_BITS 10

typedef enum {
    ADC_FREE,
    ADC_CAPTURE,
    ADC_BLOCK,
    ADC_STREAM
} adc_user_t;

static volatile adc_user_t adc_user;
static repeating_timer_t blink_timer;
static bool blink_running;
static volatile bool blink_on;
static volatile uint32_t blink_toggles;

static repeating_timer_t capture_timer;
static uint16_t capture_samples[PLATFORM_CAPTURE_MAX];
static volatile uint32_t capture_count;
static volatile uint32_t capture_goal;
static uint32_t capture_period;
static volatile uint32_t capture_last_us;
static volatile uint32_t capture_min_interval;
static volatile uint32_t capture_max_interval;
static volatile uint32_t capture_handler_us;

static int block_channel;
static uint16_t block_samples[PLATFORM_BLOCK_MAX];
static uint32_t block_started_us;
static volatile uint32_t block_finished_us;
static volatile bool block_ready;

static int stream_channels[2];
_Alignas(PLATFORM_STREAM_BYTES) static uint8_t stream_buffers[2][PLATFORM_STREAM_BYTES];
static uint8_t stream_snapshot[PLATFORM_STREAM_BYTES];
static volatile bool stream_ready[2];
static volatile uint16_t stream_numbers[2];
static volatile uint16_t stream_sequence;
static volatile uint32_t stream_overruns;
static volatile unsigned stream_irq_next;
static uint32_t stream_samples;

static unsigned tone_slice;
static bool watchdog_enabled;
static bool watchdog_reset;
static uint32_t watchdog_restarts;

_Static_assert((1u << STREAM_RING_BITS) == PLATFORM_STREAM_BYTES, "DMA ring must match buffer size");
_Static_assert(sizeof(capture_samples) + sizeof(block_samples) +
               sizeof(stream_buffers) + sizeof(stream_snapshot) <= 32 * 1024,
               "ADC buffers exceed the memory budget");

static bool on_blink(repeating_timer_t *timer)
{
    (void)timer;
    blink_on = !blink_on;
    gpio_put(LED_PIN, blink_on);
    ++blink_toggles;
    return true;
}

static bool on_capture(repeating_timer_t *timer)
{
    (void)timer;
    const uint32_t started = time_us_32();
    const uint32_t index = capture_count;
    capture_samples[index] = adc_read();
    if (index != 0) {
        const uint32_t interval = started - capture_last_us;
        if (interval < capture_min_interval) {
            capture_min_interval = interval;
        }
        if (interval > capture_max_interval) {
            capture_max_interval = interval;
        }
    }
    capture_last_us = started;
    capture_handler_us += time_us_32() - started;
    capture_count = index + 1;
    if (capture_count == capture_goal) {
        adc_user = ADC_FREE;
        return false;
    }
    return true;
}

static void adc_free_running(bool shift)
{
    adc_select_input(ADC_CHANNEL);
    adc_fifo_setup(true, true, 1, false, shift);
    adc_set_clkdiv(ADC_CLOCK_HZ / PLATFORM_SAMPLE_RATE_HZ - 1.0f);
}

static void adc_one_shot(void)
{
    adc_run(false);
    adc_fifo_drain();
    adc_fifo_setup(false, false, 0, false, false);
    adc_set_clkdiv(0);
}

static void on_block_done(void)
{
    dma_channel_acknowledge_irq1(block_channel);
    block_finished_us = time_us_32();
    adc_one_shot();
    block_ready = true;
    adc_user = ADC_FREE;
}

static void on_stream_buffer(void)
{
    for (unsigned event = 0; event < 2; ++event) {
        const unsigned current = stream_irq_next;
        const unsigned next = current ^ 1u;
        const int channel = stream_channels[current];
        if (!dma_channel_get_irq0_status(channel)) {
            break;
        }
        dma_channel_acknowledge_irq0(channel);
        if (!dma_channel_is_busy(channel)) {
            dma_channel_set_write_addr(channel, stream_buffers[current], false);
        }

        // The next DMA channel owns its buffer as soon as the previous transfer completes.
        if (stream_ready[next]) {
            stream_ready[next] = false;
            ++stream_overruns;
        }
        stream_numbers[current] = stream_sequence++;
        stream_ready[current] = true;
        stream_irq_next = next;
    }
}

void platform_init(void)
{
    stdio_init_all();
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_init(BUTTON_PIN);
    gpio_pull_up(BUTTON_PIN);
    gpio_init(CHARGE_PIN);
    gpio_put(CHARGE_PIN, false);
    gpio_set_dir(CHARGE_PIN, GPIO_OUT);

    adc_init();
    adc_gpio_init(ADC_PIN);
    adc_set_temp_sensor_enabled(true);

    block_channel = dma_claim_unused_channel(true);
    dma_channel_set_irq1_enabled(block_channel, true);
    irq_set_exclusive_handler(DMA_IRQ_1, on_block_done);
    irq_set_enabled(DMA_IRQ_1, true);
    stream_channels[0] = dma_claim_unused_channel(true);
    stream_channels[1] = dma_claim_unused_channel(true);
    irq_set_exclusive_handler(DMA_IRQ_0, on_stream_buffer);
    irq_set_enabled(DMA_IRQ_0, true);

    gpio_set_function(TONE_PIN, GPIO_FUNC_PWM);
    tone_slice = pwm_gpio_to_slice_num(TONE_PIN);
    pwm_config pwm = pwm_get_default_config();
    pwm_config_set_clkdiv(&pwm, (float)clock_get_hz(clk_sys) / TONE_COUNT_HZ);
    pwm_config_set_wrap(&pwm, 999);
    pwm_init(tone_slice, &pwm, true);

    watchdog_reset = watchdog_enable_caused_reboot();
    watchdog_hw->scratch[0] = watchdog_reset ? watchdog_hw->scratch[0] + 1 : 0;
    watchdog_restarts = watchdog_hw->scratch[0];
}

void platform_led_set(bool enabled)
{
    if (!blink_running) {
        gpio_put(LED_PIN, enabled);
    }
}

bool platform_button_read(void)
{
    return !gpio_get(BUTTON_PIN);
}

bool platform_blink_start(uint32_t period_ms)
{
    if (period_ms < 10 || period_ms > 60000) {
        return false;
    }
    platform_blink_stop();
    blink_toggles = 0;
    blink_running = add_repeating_timer_ms(-(int32_t)period_ms, on_blink, NULL, &blink_timer);
    return blink_running;
}

uint32_t platform_blink_stop(void)
{
    if (blink_running) {
        cancel_repeating_timer(&blink_timer);
        blink_running = false;
    }
    blink_on = false;
    gpio_put(LED_PIN, false);
    return blink_toggles;
}

bool platform_adc_busy(void)
{
    return adc_user != ADC_FREE;
}

float platform_adc_volts(float code)
{
    return code * ADC_VREF_V / ADC_LEVELS;
}

float platform_voltage_read(void)
{
    adc_select_input(ADC_CHANNEL);
    return platform_adc_volts(adc_read());
}

float platform_temperature_read(void)
{
    adc_select_input(TEMP_CHANNEL);
    const float voltage = platform_adc_volts(adc_read());
    return 27.0f - (voltage - 0.706f) / 0.001721f;
}

void platform_charge_set(bool enabled)
{
    gpio_put(CHARGE_PIN, enabled);
}

bool platform_capture_start(uint32_t count, uint32_t period_us)
{
    if (platform_adc_busy() || count == 0 || count > PLATFORM_CAPTURE_MAX || period_us < 10) {
        return false;
    }

    adc_select_input(ADC_CHANNEL);
    capture_count = 0;
    capture_goal = count;
    capture_period = period_us;
    capture_min_interval = UINT32_MAX;
    capture_max_interval = 0;
    capture_handler_us = 0;
    adc_user = ADC_CAPTURE;
    if (!add_repeating_timer_us(-(int64_t)period_us, on_capture, NULL, &capture_timer)) {
        adc_user = ADC_FREE;
        capture_goal = 0;
        return false;
    }
    return true;
}

bool platform_capture_done(void)
{
    return capture_goal != 0 && capture_count == capture_goal && adc_user != ADC_CAPTURE;
}

void platform_capture_get(platform_capture_t *capture)
{
    *capture = (platform_capture_t){
        .samples = capture_samples,
        .count = capture_count,
        .period_us = capture_period,
        .min_interval_us = capture_count > 1 ? capture_min_interval : 0,
        .max_interval_us = capture_max_interval,
        .total_handler_us = capture_handler_us,
    };
}

static dma_channel_config adc_dma_config(int channel, enum dma_channel_transfer_size size)
{
    dma_channel_config config = dma_channel_get_default_config(channel);
    channel_config_set_transfer_data_size(&config, size);
    channel_config_set_read_increment(&config, false);
    channel_config_set_write_increment(&config, true);
    channel_config_set_dreq(&config, DREQ_ADC);
    return config;
}

bool platform_block_start(uint32_t count)
{
    if (platform_adc_busy() || count == 0 || count > PLATFORM_BLOCK_MAX) {
        return false;
    }
    adc_free_running(false);
    dma_channel_config config = adc_dma_config(block_channel, DMA_SIZE_16);
    block_ready = false;
    adc_user = ADC_BLOCK;
    dma_channel_configure(block_channel, &config, block_samples, &adc_hw->fifo, count, true);
    block_started_us = time_us_32();
    adc_run(true);
    return true;
}

bool platform_block_done(void)
{
    return block_ready;
}

const uint16_t *platform_block_samples(void)
{
    return block_samples;
}

uint32_t platform_block_time_us(void)
{
    return block_finished_us - block_started_us;
}

bool platform_stream_start(uint8_t bits)
{
    if (platform_adc_busy() || (bits != 8 && bits != 16)) {
        return false;
    }

    const bool shift = bits == 8;
    stream_samples = shift ? PLATFORM_STREAM_BYTES : PLATFORM_STREAM_BYTES / 2;
    stream_overruns = 0;
    stream_sequence = 0;
    stream_irq_next = 0;
    adc_free_running(shift);
    adc_user = ADC_STREAM;

    for (unsigned index = 0; index < 2; ++index) {
        const int channel = stream_channels[index];
        stream_ready[index] = false;
        dma_channel_config config = adc_dma_config(channel, shift ? DMA_SIZE_8 : DMA_SIZE_16);
        channel_config_set_chain_to(&config, stream_channels[index ^ 1u]);
        channel_config_set_ring(&config, true, STREAM_RING_BITS);
        dma_channel_configure(channel, &config, stream_buffers[index], &adc_hw->fifo,
                              stream_samples, false);
        dma_channel_acknowledge_irq0(channel);
        dma_channel_set_irq0_enabled(channel, true);
    }
    dma_channel_start(stream_channels[0]);
    adc_run(true);
    return true;
}

void platform_stream_stop(void)
{
    if (adc_user != ADC_STREAM) {
        return;
    }

    adc_run(false);
    const uint32_t irq_state = save_and_disable_interrupts();

    // RP2040-E13: break the chain before aborting either channel.
    for (unsigned index = 0; index < 2; ++index) {
        const int channel = stream_channels[index];
        dma_channel_config config = dma_get_channel_config(channel);
        channel_config_set_chain_to(&config, channel);
        dma_channel_set_config(channel, &config, false);
        dma_channel_set_irq0_enabled(channel, false);
    }
    for (unsigned index = 0; index < 2; ++index) {
        const int channel = stream_channels[index];
        dma_channel_abort(channel);
        dma_channel_acknowledge_irq0(channel);
        stream_ready[index] = false;
    }
    restore_interrupts(irq_state);
    adc_one_shot();
    adc_user = ADC_FREE;
}

const uint8_t *platform_stream_next(uint32_t *samples, uint16_t *number)
{
    // Only the channel opposite the next completion can hold an unread frame.
    uint32_t irq_state = save_and_disable_interrupts();
    const unsigned index = stream_irq_next ^ 1u;
    const uint16_t sequence = stream_numbers[index];
    const bool ready = stream_ready[index] && !dma_channel_is_busy(stream_channels[index]);
    restore_interrupts(irq_state);
    if (!ready) {
        return NULL;
    }

    memcpy(stream_snapshot, stream_buffers[index], sizeof(stream_snapshot));

    irq_state = save_and_disable_interrupts();
    // DMA may have reclaimed this buffer while memcpy was running.
    const bool valid = stream_ready[index] && stream_numbers[index] == sequence &&
                       !dma_channel_is_busy(stream_channels[index]);
    if (valid) {
        stream_ready[index] = false;
    }
    restore_interrupts(irq_state);
    if (!valid) {
        return NULL;
    }
    *samples = stream_samples;
    *number = sequence;
    return stream_snapshot;
}

uint32_t platform_stream_overruns(void)
{
    return stream_overruns;
}

void platform_write(const void *data, uint32_t bytes)
{
    stdio_put_string((const char *)data, (int)bytes, false, false);
}

bool platform_tone_set(uint32_t freq_hz)
{
    if (freq_hz == 0) {
        pwm_set_gpio_level(TONE_PIN, 0);
        return true;
    }
    if (freq_hz < 20 || freq_hz > 20000) {
        return false;
    }

    const uint32_t counts = (TONE_COUNT_HZ + freq_hz / 2) / freq_hz;
    pwm_set_wrap(tone_slice, (uint16_t)(counts - 1));
    pwm_set_gpio_level(TONE_PIN, (uint16_t)(counts / 2));
    return true;
}

bool platform_watchdog_enable(uint32_t timeout_ms)
{
    if (timeout_ms == 0 || timeout_ms > WATCHDOG_MAX_MS) {
        return false;
    }
    watchdog_enable(timeout_ms, true);
    watchdog_enabled = true;
    return true;
}

bool platform_watchdog_is_enabled(void)
{
    return watchdog_enabled;
}

void platform_watchdog_feed(void)
{
    if (watchdog_enabled) {
        watchdog_update();
    }
}

bool platform_watchdog_caused_reset(uint32_t *restarts)
{
    *restarts = watchdog_restarts;
    return watchdog_reset;
}
