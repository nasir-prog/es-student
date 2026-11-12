#include "firmware.h"

#include "api/api.h"
#include "block-task/block-task.h"
#include "button-task/button-task.h"
#include "led-task/led-task.h"
#include "logging/log.h"
#include "pi-task/pi-task.h"
#include "platform/platform.h"
#include "profiling/profiling.h"
#include "stdio-text-protocol/stdio-text-protocol.h"

#include "stream-task/stream-task.h"
#include "telemetry-task/telemetry-task.h"

enum {
    WATCH_LOOP,
    WATCH_COMMAND,
    WATCH_LED,
    WATCH_BUTTON,
    WATCH_PI,
    WATCH_TELEMETRY,
    WATCH_BLOCK,
    WATCH_STREAM
};

int main(void)
{
    platform_init();
    stdio_text_protocol_init();
    led_task_init();
    button_task_init(led_task_next_state);
    telemetry_task_init(api_print_telemetry);
    stream_task_init(api_send_frame);

    profiling_stopwatch_init(WATCH_LOOP, "loop");
    profiling_stopwatch_init(WATCH_COMMAND, "command");
    profiling_stopwatch_init(WATCH_LED, "led");
    profiling_stopwatch_init(WATCH_BUTTON, "button");
    profiling_stopwatch_init(WATCH_PI, "pi");
    profiling_stopwatch_init(WATCH_TELEMETRY, "telemetry");
    profiling_stopwatch_init(WATCH_BLOCK, "block");
    profiling_stopwatch_init(WATCH_STREAM, "stream");
    LOG_INF("%s %s", FIRMWARE_NAME, FIRMWARE_VERSION);

    while (1) {
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
        profiling_start(WATCH_TELEMETRY);
        telemetry_task_handle();
        profiling_stop(WATCH_TELEMETRY);
        profiling_start(WATCH_BLOCK);
        block_task_handle();
        profiling_stop(WATCH_BLOCK);

        profiling_start(WATCH_STREAM);
        stream_task_handle();
        profiling_stop(WATCH_STREAM);
        profiling_stop(WATCH_LOOP);
        platform_watchdog_feed();
    }
}
