#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "clock.h"
#include "profiling.h"
#include "command.h"
#include "led.h"
#include "log.h"
#include "device.h"
#include "memory.h"

#define LINE_SIZE 32

char line[LINE_SIZE];
uint line_length = 0;
static bool line_overflow;
const uint BLINK_HALF_PERIOD_MS = 500;
static uint64_t last_toggle_us;

void blink(void)
{
    uint64_t now_us = time_us_64();
    if (now_us - last_toggle_us >= BLINK_HALF_PERIOD_MS * 1000u)
    {
        last_toggle_us = now_us;
        led_toggle();
    }
}

// Прикидка: 175 + 110 + 190 + 110 = 585 тактов double на член ряда.
// 1 000 000 * 585 / 125 000 000 = 4,68 с при 125 МГц; при 62,5 МГц — 9,36 с.
const uint CALC_PI_TERMS = 1000000;
volatile double pi_result;

double calc_pi(uint terms)
{
    double sum = 0.0;
    double sign = 1.0;
    for (uint k = 0; k < terms; k++)
    {
        sum += sign / (2.0 * k + 1.0);
        sign = -sign;
    }
    return 4.0 * sum;
}

void cmd_calc_pi(void)
{
    uint64_t start_us = time_us_64();
    pi_result = calc_pi(CALC_PI_TERMS);
    uint64_t spent_us = time_us_64() - start_us;
    printf("pi: %.8f\n", pi_result);
    printf("time: %llu ms\n", (unsigned long long)(spent_us / 1000));
}

void cmd_clk_info(void) { clk_info(); }
void cmd_uptime(void) { uptime(); }
void cmd_clk_sys_low(void) { clk_sys_low(); }
void cmd_clk_sys_default(void) { clk_sys_default(); }

void cmd_main_time_exec(void)
{
    printf("iteration avg %.2f us, max %u us\n",
           profiling_avg_us(), (unsigned)profiling_max_us());
}

void cmd_main_time_reset(void)
{
    profiling_reset_max();
    printf("max reset\n");
}

void cmd_info(void) { device_info(); }
void cmd_version(void) { log_version(); }
void cmd_ping(void) { printf("pong\n"); }
void cmd_mem_info(void) { mem_info(); }
void cmd_fw_info(void) { fw_info(); }
void cmd_dev_info(void) { dev_info(); }
void cmd_boot_info(void) { boot_info(); }

const struct command_t commands[] = {
    { "info", cmd_info },
    { "version", cmd_version },
    { "ping", cmd_ping },
    { "mem_info", cmd_mem_info },
    { "fw_info", cmd_fw_info },
    { "dev_info", cmd_dev_info },
    { "boot_info", cmd_boot_info },
    { "clk_info", cmd_clk_info },
    { "uptime", cmd_uptime },
    { "calc_pi", cmd_calc_pi },
    { "main_time_exec", cmd_main_time_exec },
    { "main_time_reset", cmd_main_time_reset },
    { "clk_sys_low", cmd_clk_sys_low },
    { "clk_sys_default", cmd_clk_sys_default },
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
    profiling_init();
    while (true)
    {
        profiling_iteration();
        blink();
        read_line();
    }
}
