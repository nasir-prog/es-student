#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "command.h"
#include "led.h"
#include "log.h"
#include "device.h"
#include "memory.h"

#define LINE_SIZE 32

char line[LINE_SIZE];
uint line_length = 0;
static bool line_overflow;
static const uint BUTTON_PIN = 15;
static const uint DEBOUNCE_MS = 20;

void cmd_enable(void)
{
    led_set(true);
    LOG_INF("led %s\n", led_is_on() ? "on" : "off");
}

void cmd_disable(void)
{
    led_set(false);
    LOG_INF("led %s\n", led_is_on() ? "on" : "off");
}

void cmd_info(void) { device_info(); }
void cmd_version(void) { log_version(); }
void cmd_ping(void) { printf("pong\n"); }
void cmd_mem_info(void) { mem_info(); }
void cmd_fw_info(void) { fw_info(); }
void cmd_dev_info(void) { dev_info(); }
void cmd_boot_info(void) { boot_info(); }

const struct command_t commands[] = {
    { "enable", cmd_enable },
    { "disable", cmd_disable },
    { "info", cmd_info },
    { "version", cmd_version },
    { "ping", cmd_ping },
    { "mem_info", cmd_mem_info },
    { "fw_info", cmd_fw_info },
    { "dev_info", cmd_dev_info },
    { "boot_info", cmd_boot_info },
};

const uint command_count = sizeof(commands) / sizeof(commands[0]);

void handle_command(const char *command)
{
    for (uint i = 0; i < command_count; i++)
    {
        if (strcmp(command, commands[i].name) == 0)
        {
            if (commands[i].handler != NULL)
            {
                commands[i].handler();
            }
            return;
        }
    }
    LOG_ERR("unknown command: %s\n", command);
}

void read_line(void)
{
    int symbol = getchar_timeout_us(0);
    if (symbol == PICO_ERROR_TIMEOUT)
    {
        return;
    }
    if (symbol == '\r' || symbol == '\n')
    {
        putchar('\n');
        line[line_length] = '\0';
        if (line_overflow)
        {
            LOG_ERR("command too long\n");
        }
        else if (line_length > 0)
        {
            LOG_DBG("got %s\n", line);
            handle_command(line);
        }
        line_length = 0;
        line_overflow = false;
        return;
    }
    /* После переполнения отбрасываем всю строку, а не выполняем её префикс. */
    if (line_overflow)
    {
        return;
    }
    if (symbol == '\b' || symbol == 127)
    {
        if (line_length > 0)
        {
            line_length--;
            printf("\b \b");
        }
        return;
    }
    if (line_length + 1 < LINE_SIZE)
    {
        line[line_length++] = (char)symbol;
        putchar(symbol);
    }
    else
    {
        line_overflow = true;
    }
}

int main(void)
{
    stdio_init_all();
    led_init();
    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);

    bool previous = gpio_get(BUTTON_PIN);
    bool candidate = previous;
    uint32_t changed_at = to_ms_since_boot(get_absolute_time());
    while (true)
    {
        /* Антидребезг без sleep: приём USB продолжается в каждом проходе. */
        bool current = gpio_get(BUTTON_PIN);
        uint32_t now = to_ms_since_boot(get_absolute_time());
        if (current != candidate)
        {
            candidate = current;
            changed_at = now;
        }
        if (candidate != previous && (uint32_t)(now - changed_at) >= DEBOUNCE_MS)
        {
            previous = candidate;
            if (!previous)
            {
                led_toggle();
                LOG_INF("led %s\n", led_is_on() ? "on" : "off");
            }
        }
        read_line();
        tight_loop_contents();
    }
}
