#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "trace-output.h"

#include "api/api.h"
#include "block-task/block-task.h"
#include "button-task/button-task.h"
#include "device/device.h"
#include "edge-task/edge-task.h"
#include "led-task/led-task.h"
#include "pi-task/pi-task.h"
#include "platform/platform.h"
#include "uart-task/uart-task.h"

#define TICK_US 50u

static uint64_t now_us;
static int input = -1;
static bool external_loopback;

bool model_external_loopback(void) { return external_loopback; }

int getchar_timeout_us(uint32_t timeout_us)
{
    assert(timeout_us == 0);
    int byte = input;
    input = -1;
    return byte;
}

uint64_t time_us_64(void) { return now_us; }
void platform_led_set(bool enabled) { (void)enabled; }
bool platform_button_read(void) { return false; }

void device_get_info(device_info_t *info)
{
    *info = (device_info_t){
        .board = "pico",
        .serial = "0000000000000000",
        .sdk_version = "2.3.0",
    };
}

static void handle_input(void)
{
    const command_t *command = stdio_text_protocol_handle();
    if (command) {
        api_handle(command);
    }
}

int main(int argc, char *argv[])
{
    assert(argc == 2);
    external_loopback = strcmp(argv[1], "jumper") == 0;
    stdio_text_protocol_init();
    led_task_init();
    button_task_init(led_task_next_state);

    uint64_t listen_us;
    char hex[513];
    while (scanf("%" SCNu64 " %512s", &listen_us, hex) == 2) {
        if (strcmp(hex, "-") != 0) {
            for (size_t i = 0; hex[i]; i += 2) {
                const char byte[] = {hex[i], hex[i + 1], '\0'};
                input = (int)strtoul(byte, NULL, 16);
                handle_input();
            }
        }
        for (uint64_t elapsed = 0; elapsed < listen_us; elapsed += TICK_US) {
            now_us += TICK_US;
            trace_set_time(now_us);
            edge_task_handle();
            uart_task_handle();
            block_task_handle();
            pi_task_handle();
        }
        trace_finish_step();
    }
    return 0;
}
