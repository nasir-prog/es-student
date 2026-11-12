#include "api/api-commands.h"

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include "button-task/button-task.h"
#include "device/device.h"
#include "firmware.h"
#include "led-task/led-task.h"
#include "pi-task/pi-task.h"
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
    for (unsigned index = 0; index < 2; ++index) {
        printf("button %u: %s, presses %" PRIu32 "\n", index + 1,
               button_task_is_pressed(index) ? "pressed" : "released",
               button_task_get_press_count(index));
    }
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
    {0},
};
