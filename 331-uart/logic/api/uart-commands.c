#include "api/uart-commands.h"

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "block-task/block-task.h"
#include "edge-task/edge-task.h"
#include "uart-task/uart-task.h"

static platform_parity_t uart_parity[2];
static uint8_t block_data[UART_TASK_MAX];
static bool block_irq;

static struct {
    bool active;
    bool rx_ready;
    bool adc_ready;
    uint32_t baud;
    uint32_t count;
    const uint16_t *samples;
    uint8_t data[4];
    size_t received;
    uint32_t errors;
} scope;

static void print_bytes(const uint8_t *data, size_t size)
{
    for (size_t i = 0; i < size; ++i) {
        putchar(isprint(data[i]) ? data[i] : '.');
    }
}

static void print_errors(uint32_t errors)
{
    fputs("err:", stdout);
    if (!errors) {
        fputs(" none", stdout);
    }
    if (errors & PLATFORM_UART_FE) fputs(" FE", stdout);
    if (errors & PLATFORM_UART_PE) fputs(" PE", stdout);
    if (errors & PLATFORM_UART_BE) fputs(" BE", stdout);
    if (errors & PLATFORM_UART_OE) fputs(" OE", stdout);
    putchar('\n');
}

static void print_edge(platform_line_mode_t mode, bool ok, uint32_t rise_us, uint32_t fall_us)
{
    const char *name = mode == PLATFORM_LINE_PUSH_PULL ? "pp" : "od";
    if (ok) {
        printf("edge %s: rise %.1f ms, fall %.1f ms\n", name, rise_us / 1000.0, fall_us / 1000.0);
    } else {
        printf("edge %s: timeout\n", name);
    }
}

void cmd_edge(const command_t *cmd)
{
    if (cmd->argc != 1 || (strcmp(cmd->argv[0], "pp") && strcmp(cmd->argv[0], "od"))) {
        puts("error: usage edge pp|od");
        return;
    }
    platform_line_mode_t mode = strcmp(cmd->argv[0], "pp") == 0 ?
                                PLATFORM_LINE_PUSH_PULL : PLATFORM_LINE_OPEN_DRAIN;
    if (scope.active || !edge_task_start(mode, print_edge)) {
        puts("error: edge is busy");
    }
}

static bool uart_available(void)
{
    if (scope.active || uart_task_active()) {
        puts("error: uart is busy");
        return false;
    }
    return true;
}

void cmd_loop(const command_t *cmd)
{
    if (cmd->argc != 1 || (strcmp(cmd->argv[0], "internal") && strcmp(cmd->argv[0], "pins"))) {
        puts("error: usage loop internal|pins");
    } else if (uart_available()) {
        platform_uart_loop_set(strcmp(cmd->argv[0], "internal") == 0);
        printf("loop: %s\n", cmd->argv[0]);
    }
}

static void print_reply(const char *prefix, const uint8_t *data, size_t size, uint32_t errors)
{
    fputs(prefix, stdout);
    print_bytes(data, size);
    putchar(' ');
    print_errors(errors);
}

static void print_echo(const uint8_t *data, size_t size, uint32_t errors)
{
    print_reply("rx: ", data, size, errors);
}

static void print_link_back(const uint8_t *data, size_t size, uint32_t errors)
{
    print_reply("1->0: ", data, size, errors);
}

static void print_link_forth(const uint8_t *data, size_t size, uint32_t errors)
{
    print_reply("0->1: ", data, size, errors);
    if (size != 0) {
        uart_task_start(PLATFORM_UART1, PLATFORM_UART0, data, size, print_link_back);
    }
}

static void send_argument(const command_t *cmd, const char *usage, platform_uart_t rx,
                          uart_task_callback_t callback)
{
    if (cmd->argc != 1) {
        puts(usage);
        return;
    }
    size_t size = strlen(cmd->argv[0]);
    if (size > 32) {
        puts("error: text longer than 32");
    } else if (uart_available()) {
        uart_task_start(PLATFORM_UART0, rx, (const uint8_t *)cmd->argv[0], size, callback);
    }
}

void cmd_echo(const command_t *cmd)
{
    send_argument(cmd, "error: usage echo <text>", PLATFORM_UART0, print_echo);
}

void cmd_link(const command_t *cmd)
{
    send_argument(cmd, "error: usage link <text>", PLATFORM_UART1, print_link_forth);
}

void cmd_baud_shift(const command_t *cmd)
{
    if (cmd->argc != 1) {
        puts("error: usage baud_shift <percent>");
        return;
    }
    const char *arg = cmd->argv[0];
    char *end;
    errno = 0;
    long percent = strtol(arg, &end, 10);
    if (end == arg || *end) {
        puts("error: usage baud_shift <percent>");
    } else if (errno == ERANGE || percent < -50 || percent > 50) {
        puts("error: percent must be -50..50");
    } else if (uart_available()) {
        uint32_t baud = platform_uart_baud_set(PLATFORM_UART1, PLATFORM_UART_BAUD * (100 + percent) / 100);
        printf("baud_shift: uart1 %" PRIu32 " baud\n", baud);
    }
}

static void print_text(const uint8_t *data, size_t size, uint32_t errors)
{
    printf("text: %zu chars ", size);
    print_bytes(data, size);
    putchar(' ');
    print_errors(errors);
}

void cmd_text(const command_t *cmd)
{
    (void)cmd;
    static const uint8_t message[] = "0123456789ABCDEFGHIJKLMNOPQRSTUV";
    if (uart_available()) {
        uart_task_start(PLATFORM_UART1, PLATFORM_UART0, message, sizeof(message) - 1, print_text);
    }
}

static bool parity_from_text(const char *text, platform_parity_t *parity)
{
    static const char *const names[] = {"none", "even", "odd"};
    for (unsigned i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
        if (strcmp(text, names[i]) == 0) {
            *parity = (platform_parity_t)i;
            return true;
        }
    }
    return false;
}

void cmd_parity(const command_t *cmd)
{
    platform_parity_t rx, tx;
    if (cmd->argc != 2 || !parity_from_text(cmd->argv[0], &rx) ||
        !parity_from_text(cmd->argv[1], &tx)) {
        puts("error: usage parity none|even|odd none|even|odd");
    } else if (uart_available()) {
        uart_parity[0] = rx;
        uart_parity[1] = tx;
        platform_uart_parity_set(PLATFORM_UART0, rx);
        platform_uart_parity_set(PLATFORM_UART1, tx);
        printf("parity: uart0 %s, uart1 %s\n", cmd->argv[0], cmd->argv[1]);
    }
}

static void print_block(const uint8_t *data, size_t size, uint32_t errors)
{
    platform_uart_rx_irq_set(false);
    bool match = size && memcmp(data, block_data, size) == 0;
    printf("block %s: %zu/%u %s, ", block_irq ? "irq" : "poll", size,
           (unsigned)sizeof(block_data), match ? "match" : "mismatch");
    print_errors(errors);
}

void cmd_block(const command_t *cmd)
{
    if (cmd->argc != 1 || (strcmp(cmd->argv[0], "irq") && strcmp(cmd->argv[0], "poll"))) {
        puts("error: usage block irq|poll");
    } else if (uart_available()) {
        block_irq = strcmp(cmd->argv[0], "irq") == 0;
        for (size_t i = 0; i < sizeof(block_data); ++i) {
            block_data[i] = (uint8_t)i;
        }
        platform_uart_rx_irq_set(block_irq);
        uart_task_start(PLATFORM_UART1, PLATFORM_UART0, block_data, sizeof(block_data), print_block);
    }
}

static void restore_ports(void)
{
    for (unsigned i = 0; i < 2; ++i) {
        platform_uart_baud_set((platform_uart_t)i, PLATFORM_UART_BAUD);
        platform_uart_parity_set((platform_uart_t)i, uart_parity[i]);
    }
}

static void finish_scope(void)
{
    if (!scope.rx_ready || !scope.adc_ready) {
        return;
    }
    restore_ports();
    printf("scope: %" PRIu32 " samples, %u Hz, %" PRIu32 " baud, uart1 ",
           scope.count, PLATFORM_SAMPLE_RATE_HZ, scope.baud);
    print_bytes(scope.data, scope.received);
    putchar(' ');
    print_errors(scope.errors);
    for (uint32_t i = 0; i < scope.count; ++i) {
        printf("%u%c", (unsigned)scope.samples[i],
               i % 16 == 15 || i + 1 == scope.count ? '\n' : ' ');
    }
    puts("scope: end");
    scope.active = false;
}

static void scope_uart_ready(const uint8_t *data, size_t size, uint32_t errors)
{
    scope.received = size < sizeof(scope.data) ? size : sizeof(scope.data);
    memcpy(scope.data, data, scope.received);
    scope.errors = errors;
    scope.rx_ready = true;
    finish_scope();
}

static void scope_adc_ready(const uint16_t *data, uint32_t count, uint32_t time_us)
{
    (void)time_us;
    scope.samples = data;
    scope.count = count;
    scope.adc_ready = true;
    finish_scope();
}

void cmd_scope(const command_t *cmd)
{
    const char *arg = cmd->argc == 2 ? cmd->argv[0] : "";
    char *end;
    unsigned long baud = strtoul(arg, &end, 10);
    if (cmd->argc != 2 || *arg < '0' || *arg > '9' || *end ||
        baud < 9600 || baud > PLATFORM_UART_BAUD || strlen(cmd->argv[1]) > sizeof(scope.data)) {
        puts("error: usage scope <baud> <text>");
        return;
    }
    if (!uart_available()) {
        return;
    }
    if (edge_task_active()) {
        puts("error: adc busy");
        return;
    }
    for (unsigned i = 0; i < 2; ++i) {
        platform_uart_baud_set((platform_uart_t)i, (uint32_t)baud);
        platform_uart_parity_set((platform_uart_t)i, PLATFORM_PARITY_NONE);
    }
    if (!block_task_start(PLATFORM_BLOCK_MAX, scope_adc_ready)) {
        restore_ports();
        puts("error: adc busy");
        return;
    }
    scope.active = true;
    scope.adc_ready = false;
    scope.rx_ready = false;
    scope.baud = (uint32_t)baud;
    // Give the ADC time to capture idle-high samples before the start bit.
    uart_task_start_after(PLATFORM_UART0, PLATFORM_UART1, (const uint8_t *)cmd->argv[1],
                          strlen(cmd->argv[1]), 20, scope_uart_ready);
}
