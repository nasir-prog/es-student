#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PLATFORM_UART_BAUD 115200u
#define PLATFORM_UART_FE 0x1u
#define PLATFORM_UART_PE 0x2u
#define PLATFORM_UART_BE 0x4u
#define PLATFORM_UART_OE 0x8u
#define PLATFORM_SAMPLE_RATE_HZ 500000u
#define PLATFORM_BLOCK_MAX 4096u

typedef enum {
    PLATFORM_LINE_PUSH_PULL,
    PLATFORM_LINE_OPEN_DRAIN,
} platform_line_mode_t;

typedef enum {
    PLATFORM_UART0,
    PLATFORM_UART1,
} platform_uart_t;

typedef enum {
    PLATFORM_PARITY_NONE,
    PLATFORM_PARITY_EVEN,
    PLATFORM_PARITY_ODD,
} platform_parity_t;

void platform_init(void);
void platform_led_set(bool enabled);
bool platform_button_read(void);

void platform_line_set(platform_line_mode_t mode, bool high);
float platform_voltage_read(void);

void platform_uart_loop_set(bool internal);
void platform_uart_clear(platform_uart_t uart);
bool platform_uart_put(platform_uart_t uart, uint8_t byte);
bool platform_uart_busy(platform_uart_t uart);
size_t platform_uart_read(platform_uart_t uart, uint8_t *data, size_t size);
uint32_t platform_uart_errors(platform_uart_t uart);
uint32_t platform_uart_baud_set(platform_uart_t uart, uint32_t baud);
void platform_uart_parity_set(platform_uart_t uart, platform_parity_t parity);
void platform_uart_rx_irq_set(bool on);

bool platform_block_start(uint32_t count);
bool platform_block_done(void);
const uint16_t *platform_block_samples(void);
uint32_t platform_block_time_us(void);
