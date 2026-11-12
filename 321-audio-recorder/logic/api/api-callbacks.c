#include "api/api-commands.h"
#include "api/api.h"

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "block-task/block-task.h"
#include "button-task/button-task.h"
#include "device/device.h"
#include "firmware.h"
#include "led-task/led-task.h"
#include "pi-task/pi-task.h"
#include "platform/platform.h"
#include "profiling/profiling.h"
#include "systime/systime.h"

static bool parse_u32(const char *text, uint32_t *value)
{
    if (*text == '\0') {
        return false;
    }
    for (const char *digit = text; *digit; ++digit) {
        if (*digit < '0' || *digit > '9') {
            return false;
        }
    }
    errno = 0;
    const unsigned long result = strtoul(text, NULL, 10);
    if (errno == ERANGE) {
        return false;
    }
#if ULONG_MAX > UINT32_MAX
    if (result > UINT32_MAX) {
        return false;
    }
#endif
    *value = (uint32_t)result;
    return true;
}

static void cmd_info(const command_t *command)
{
    (void)command;
    device_info_t info;
    device_get_info(&info);
    printf("name: %s\nversion: %s\nproject: %s\nrepo: %s\n",
           FIRMWARE_NAME, FIRMWARE_VERSION, FIRMWARE_PROJECT, FIRMWARE_REPO);
    printf("board: %s\nserial: %s\n", info.board, info.serial);
    printf("chip: manufacturer 0x%03" PRIx32 ", part 0x%04" PRIx32 ", revision %" PRIu32 "\n",
           info.manufacturer, info.part, info.revision);
    printf("pico-sdk: %s\n", info.sdk_version);
}

static void cmd_uptime(const command_t *command)
{
    (void)command;
    printf("uptime: %llu ms\n", (unsigned long long)systime_ms());
}

static void print_led(void)
{
    static const char *const states[] = {"off", "on", "blink"};
    printf("led: %s, period %" PRIu32 " ms\n",
           states[led_task_get_state()], led_task_get_period_ms());
}

static void cmd_led_enable(const command_t *command)
{
    (void)command;
    led_task_set_state(LED_STATE_ON);
    print_led();
}

static void cmd_led_disable(const command_t *command)
{
    (void)command;
    led_task_set_state(LED_STATE_OFF);
    print_led();
}

static void cmd_led_blink(const command_t *command)
{
    (void)command;
    led_task_set_state(LED_STATE_BLINK);
    print_led();
}

static void cmd_led_period(const command_t *command)
{
    uint32_t period_ms;
    if (command->argc != 1 || !parse_u32(command->argv[0], &period_ms)) {
        puts("error: usage led_period <period_ms>");
    } else if (period_ms == 0) {
        puts("error: period_ms must be greater than 0");
    } else {
        led_task_set_period_ms(period_ms);
        print_led();
    }
}

static void cmd_button(const command_t *command)
{
    (void)command;
    printf("button: %s, presses %" PRIu32 "\n",
           button_task_is_pressed() ? "pressed" : "released", button_task_get_press_count());
}

static void cmd_pi_start(const command_t *command)
{
    uint32_t terms = 1000000;
    if (command->argc > 1 || (command->argc == 1 && !parse_u32(command->argv[0], &terms))) {
        puts("error: usage pi_start [<terms>]");
    } else if (terms == 0) {
        puts("error: terms must be greater than 0");
    } else if (!pi_task_start(terms)) {
        puts("error: pi is busy");
    } else {
        printf("pi: started, %" PRIu32 " terms\n", terms);
    }
}

static void cmd_pi(const command_t *command)
{
    (void)command;
    pi_result_t result;
    switch (pi_task_get_result(&result)) {
    case PI_IDLE:
        puts("pi: not started");
        break;
    case PI_RUNNING:
        puts("pi: running");
        break;
    case PI_READY:
        printf("pi: %.9f in %llu ms\n", result.value, (unsigned long long)result.time_ms);
        break;
    }
}

static void cmd_profiling(const command_t *command)
{
    (void)command;
#if PROFILING_ENABLED
    profiling_result_t result;
    for (unsigned i = 0; profiling_get(i, &result); ++i) {
        printf("%-10s count %-10llu mean %.2f us  max %" PRIu32 " us\n",
               result.name, (unsigned long long)result.count, result.mean_us, result.max_us);
    }
#else
    puts("profiling is disabled in this build");
#endif
}

static void cmd_profiling_reset(const command_t *command)
{
    (void)command;
    profiling_reset();
    puts("profiling reset");
}

static void cmd_get_voltage(const command_t *command)
{
    (void)command;
    if (platform_adc_busy()) {
        puts("error: adc busy");
        return;
    }
    printf("voltage: %.3f V\n", platform_voltage_read());
}

static void cmd_get_temp(const command_t *command)
{
    (void)command;
    if (platform_adc_busy()) {
        puts("error: adc busy");
        return;
    }
    printf("temp: %.1f C\n", platform_temperature_read());
}

void api_print_telemetry(const telemetry_record_t *record)
{
    printf("tm: %llu %.3f %.1f\n", (unsigned long long)record->time_us,
           record->voltage_v, record->temp_c);
}

static void cmd_tm_start(const command_t *command)
{
    uint32_t period_ms = 100;
    if (command->argc > 1 || (command->argc == 1 && !parse_u32(command->argv[0], &period_ms))) {
        puts("error: usage tm_start [<period_ms>]");
    } else if (platform_adc_busy()) {
        puts("error: adc busy");
    } else if (!telemetry_task_start(period_ms)) {
        puts("error: period_ms must be at least 10");
    } else {
        printf("telemetry: started, period %" PRIu32 " ms\n", telemetry_task_get_period_ms());
    }
}

static void cmd_tm_stop(const command_t *command)
{
    (void)command;
    telemetry_task_stop();
    puts("telemetry: stopped");
}

static void cmd_charge_on(const command_t *command)
{
    (void)command;
    platform_charge_set(true);
    puts("charge: on");
}

static void cmd_charge_off(const command_t *command)
{
    (void)command;
    platform_charge_set(false);
    puts("charge: off");
}

static bool telemetry_active(void)
{
    return telemetry_task_get_state() == TELEMETRY_STATE_RUN;
}

static void cmd_timer_blink_start(const command_t *command)
{
    uint32_t period_ms;
    if (command->argc != 1 || !parse_u32(command->argv[0], &period_ms)) {
        puts("error: usage timer_blink_start <period_ms>");
    } else if (period_ms < 10 || period_ms > 60000) {
        puts("error: period_ms must be 10..60000");
    } else if (stream_task_get_state() == STREAM_STATE_RUN || !platform_blink_start(period_ms)) {
        puts("error: indicator busy");
    } else {
        led_task_set_state(LED_STATE_OFF);
        printf("timer_blink: started, period %" PRIu32 " ms\n", period_ms);
    }
}

static void cmd_timer_blink_stop(const command_t *command)
{
    (void)command;
    if (stream_task_get_state() == STREAM_STATE_RUN) {
        puts("error: indicator busy");
        return;
    }
    const uint32_t toggles = platform_blink_stop();
    led_task_set_state(led_task_get_state());
    printf("timer_blink: stopped, %" PRIu32 " toggles\n", toggles);
}

static void cmd_cap_start(const command_t *command)
{
    uint32_t count;
    uint32_t period_us;
    if (command->argc != 2 || !parse_u32(command->argv[0], &count) ||
        !parse_u32(command->argv[1], &period_us)) {
        puts("error: usage cap_start <N> <period_us>");
    } else if (count == 0 || count > PLATFORM_CAPTURE_MAX || period_us < 10) {
        puts("error: N must be 1..2000, period_us at least 10");
    } else if (telemetry_active() || !platform_capture_start(count, period_us)) {
        puts("error: adc busy");
    } else {
        printf("capture: started, %" PRIu32 " samples, period %" PRIu32 " us\n", count, period_us);
    }
}

static void cmd_cap_dump(const command_t *command)
{
    (void)command;
    if (!platform_capture_done()) {
        puts("error: no capture");
        return;
    }
    // Keep large diagnostic replies out of a live audio stream.
    if (stream_task_get_state() == STREAM_STATE_RUN) {
        puts("error: adc busy");
        return;
    }

    platform_capture_t capture;
    platform_capture_get(&capture);
    const double handler_pct = 100.0 * capture.total_handler_us /
                               ((double)capture.period_us * capture.count);
    printf("capture: %" PRIu32 " samples, period %" PRIu32 " us, interval %" PRIu32 "..%" PRIu32
           " us, handler %.1f %%\n", capture.count, capture.period_us,
           capture.min_interval_us, capture.max_interval_us, handler_pct);
    for (uint32_t i = 0; i < capture.count; ++i) {
        if (i % 16 == 0) {
            fputs("cap:", stdout);
        }
        printf(" %u", (unsigned)capture.samples[i]);
        if (i % 16 == 15 || i + 1 == capture.count) {
            putchar('\n');
        }
    }
}

static void print_dma_block(const uint16_t *samples, uint32_t count, uint32_t time_us)
{
    uint16_t minimum = samples[0];
    uint16_t maximum = samples[0];
    uint32_t sum = 0;
    for (uint32_t i = 0; i < count; ++i) {
        const uint16_t sample = samples[i];
        if (sample < minimum) {
            minimum = sample;
        }
        if (sample > maximum) {
            maximum = sample;
        }
        sum += sample;
    }

    const uint32_t expected = (uint32_t)(((uint64_t)count * 1000000 + PLATFORM_SAMPLE_RATE_HZ / 2) /
                                         PLATFORM_SAMPLE_RATE_HZ);
    printf("dma_cap: %" PRIu32 " samples in %" PRIu32 " us, expected %" PRIu32
           " us, min %u max %u mean %.1f\n",
           count, time_us, expected, (unsigned)minimum, (unsigned)maximum, (double)sum / count);
}

static void cmd_dma_cap(const command_t *command)
{
    uint32_t count;
    if (command->argc != 1 || !parse_u32(command->argv[0], &count)) {
        puts("error: usage dma_cap <N>");
    } else if (count == 0 || count > PLATFORM_BLOCK_MAX) {
        puts("error: N must be 1..8192");
    } else if (telemetry_active() || !block_task_start(count, print_dma_block)) {
        puts("error: adc busy");
    }
}

void api_send_frame(const stream_header_t *header, const uint8_t *data, uint32_t bytes)
{
    static uint8_t frame[6 + PLATFORM_STREAM_BYTES];
    frame[0] = (uint8_t)header->sync;
    frame[1] = (uint8_t)(header->sync >> 8);
    frame[2] = (uint8_t)header->number;
    frame[3] = (uint8_t)(header->number >> 8);
    frame[4] = (uint8_t)header->samples;
    frame[5] = (uint8_t)(header->samples >> 8);
    memcpy(frame + 6, data, bytes);
    platform_write(frame, 6 + bytes);
}

static void cmd_stream_start(const command_t *command)
{
    uint32_t bits = 16;
    if (command->argc > 1 || (command->argc == 1 && !parse_u32(command->argv[0], &bits)) ||
        (bits != 8 && bits != 16)) {
        puts("error: usage stream_start [8|16]");
        return;
    }
    if (telemetry_active() || !stream_task_start((uint8_t)bits)) {
        puts("error: adc busy");
        return;
    }
    if (!platform_blink_start(250)) {
        stream_task_stop();
        puts("error: indicator busy");
        return;
    }

    led_task_set_state(LED_STATE_OFF);
    printf("stream: started, 44100 Hz, %" PRIu32 " bit\n", bits);
}

static void cmd_stream_stop(const command_t *command)
{
    (void)command;
    if (stream_task_get_state() == STREAM_STATE_RUN) {
        stream_task_stop();
        platform_blink_stop();
        led_task_set_state(led_task_get_state());
    }
    printf("stream: stopped, %" PRIu32 " frames, %" PRIu32 " samples, %" PRIu32 " overruns\n",
           stream_task_get_frames(), stream_task_get_samples(), stream_task_get_overruns());
}

static void cmd_generate_tone(const command_t *command)
{
    uint32_t freq;
    if (command->argc != 1 || !parse_u32(command->argv[0], &freq)) {
        puts("error: usage generate_tone <Hz>");
    } else if (!platform_tone_set(freq)) {
        puts("error: Hz must be 0 or 20..20000");
    } else if (freq == 0) {
        puts("tone: off");
    } else {
        printf("tone: %" PRIu32 " Hz\n", freq);
    }
}

static void cmd_wdt_enable(const command_t *command)
{
    uint32_t timeout_ms;
    if (command->argc != 1 || !parse_u32(command->argv[0], &timeout_ms)) {
        puts("error: usage wdt_enable <timeout_ms>");
    } else if (!platform_watchdog_enable(timeout_ms)) {
        puts("error: timeout_ms must be 1..8388");
    } else {
        printf("wdt: enabled, timeout %" PRIu32 " ms\n", timeout_ms);
    }
}

static void cmd_hang(const command_t *command)
{
    (void)command;
    if (!platform_watchdog_is_enabled()) {
        puts("error: watchdog is not enabled");
        return;
    }

    puts("hang: superloop stopped");
    fflush(stdout);
    // This command intentionally stops the superloop until the watchdog resets the Pico.
    for (;;) {
    }
}

static void cmd_reset_reason(const command_t *command)
{
    (void)command;
    uint32_t restarts;
    const bool watchdog = platform_watchdog_caused_reset(&restarts);
    printf("reset: %s, restarts %" PRIu32 "\n", watchdog ? "watchdog" : "other", restarts);
}

static float sample_mean(const uint16_t *samples, uint32_t count)
{
    uint32_t sum = 0;
    for (uint32_t i = 0; i < count; ++i) {
        sum += samples[i];
    }
    return (float)sum / (float)count;
}

static void print_mic_bias(const uint16_t *samples, uint32_t count, uint32_t time_us)
{
    (void)time_us;
    printf("mic: bias %.3f V\n", platform_adc_volts(sample_mean(samples, count)));
}

static void print_mic_level(const uint16_t *samples, uint32_t count, uint32_t time_us)
{
    (void)time_us;
    const float mean = sample_mean(samples, count);
    float squares = 0;
    float peak = 0;
    for (uint32_t i = 0; i < count; ++i) {
        const float deviation = fabsf((float)samples[i] - mean);
        squares += deviation * deviation;
        if (deviation > peak) {
            peak = deviation;
        }
    }

    const float rms = sqrtf(squares / (float)count);
    const uint32_t ms = (count * 1000 + PLATFORM_SAMPLE_RATE_HZ / 2) / PLATFORM_SAMPLE_RATE_HZ;
    printf("mic: level %.1f mV rms, peak %.1f mV, %" PRIu32 " ms\n",
           1000.0 * platform_adc_volts(rms), 1000.0 * platform_adc_volts(peak), ms);
}

static void cmd_mic_bias(const command_t *command)
{
    (void)command;
    if (telemetry_active() || !block_task_start(4410, print_mic_bias)) {
        puts("error: adc busy");
    }
}

static void cmd_mic_level(const command_t *command)
{
    uint32_t ms = 100;
    if (command->argc > 1 || (command->argc == 1 && !parse_u32(command->argv[0], &ms))) {
        puts("error: usage mic_level [<ms>]");
    } else if (ms == 0 || ms > 185) {
        puts("error: ms must be 1..185");
    } else if (telemetry_active() || !block_task_start(ms * PLATFORM_SAMPLE_RATE_HZ / 1000, print_mic_level)) {
        puts("error: adc busy");
    }
}

const api_command_t api_commands[] = {
    {"info", "device passport", cmd_info},
    {"uptime", "time since reset", cmd_uptime},
    {"led_enable", "turn the LED on", cmd_led_enable},
    {"led_disable", "turn the LED off", cmd_led_disable},
    {"led_blink", "blink the LED", cmd_led_blink},
    {"led_period", "set blink period in milliseconds", cmd_led_period},
    {"button", "show button state and press count", cmd_button},
    {"pi_start", "start a stepped Leibniz sum", cmd_pi_start},
    {"pi", "show the result or state of the calculation", cmd_pi},
    {"profiling", "show stopwatch measurements", cmd_profiling},
    {"profiling_reset", "reset stopwatch measurements", cmd_profiling_reset},
    {"get_voltage", "measure voltage on GP26", cmd_get_voltage},
    {"get_temp", "measure internal temperature", cmd_get_temp},
    {"tm_start", "start telemetry", cmd_tm_start},
    {"tm_stop", "stop telemetry", cmd_tm_stop},
    {"charge_on", "charge the RC circuit", cmd_charge_on},
    {"charge_off", "discharge the RC circuit", cmd_charge_off},
    {"timer_blink_start", "start the timer indicator", cmd_timer_blink_start},
    {"timer_blink_stop", "stop the timer indicator", cmd_timer_blink_stop},
    {"cap_start", "capture ADC samples by timer", cmd_cap_start},
    {"cap_dump", "print the latest timer capture", cmd_cap_dump},
    {"dma_cap", "capture one block via DMA", cmd_dma_cap},
    {"stream_start", "start 8-bit or 16-bit sampling", cmd_stream_start},
    {"stream_stop", "stop sampling and show statistics", cmd_stream_stop},
    {"generate_tone", "generate a test tone on GP20", cmd_generate_tone},
    {"wdt_enable", "enable the watchdog", cmd_wdt_enable},
    {"hang", "trigger a watchdog restart", cmd_hang},
    {"reset_reason", "show the previous reset cause", cmd_reset_reason},
    {"mic_bias", "measure the microphone bias", cmd_mic_bias},
    {"mic_level", "measure RMS and peak microphone levels", cmd_mic_level},
    {0},
};
