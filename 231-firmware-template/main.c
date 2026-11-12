#include "firmware.h"

#include "api/api.h"
#include "button-task/button-task.h"
#include "led-task/led-task.h"
#include "logging/log.h"
#include "pi-task/pi-task.h"
#include "platform/platform.h"
#include "profiling/profiling.h"
#include "stdio-text-protocol/stdio-text-protocol.h"

enum {
    WATCH_LOOP,
    WATCH_COMMAND,
    WATCH_LED,
    WATCH_BUTTON,
    WATCH_PI
};

int main(void)
{
    platform_init();
    stdio_text_protocol_init();
    led_task_init();
    button_task_init(led_task_next_state);

    profiling_stopwatch_init(WATCH_LOOP, "loop");
    profiling_stopwatch_init(WATCH_COMMAND, "command");
    profiling_stopwatch_init(WATCH_LED, "led");
    profiling_stopwatch_init(WATCH_BUTTON, "button");
    profiling_stopwatch_init(WATCH_PI, "pi");
    LOG_INF("%s %s", FIRMWARE_NAME, FIRMWARE_VERSION);

    for (;;) {
        profiling_start(WATCH_LOOP);
        profiling_start(WATCH_COMMAND);
        const command_t *command = stdio_text_protocol_handle();
        if (command) {
            api_handle(command);
        }
        profiling_stop(WATCH_COMMAND);

        profiling_start(WATCH_LED);
        led_task_handle();
        profiling_stop(WATCH_LED);

        profiling_start(WATCH_BUTTON);
        button_task_handle();
        profiling_stop(WATCH_BUTTON);

        profiling_start(WATCH_PI);
        pi_task_handle();
        profiling_stop(WATCH_PI);
        profiling_stop(WATCH_LOOP);
    }
}
