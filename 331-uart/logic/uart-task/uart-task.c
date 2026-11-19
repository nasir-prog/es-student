#include "uart-task/uart-task.h"

#include <string.h>
#include "systime/systime.h"

#define QUIET_US 1000u

typedef enum { UART_IDLE, UART_SEND, UART_QUIET } uart_state_t;

static uart_state_t state;
static platform_uart_t tx_uart;
static platform_uart_t rx_uart;
static uint8_t tx_data[UART_TASK_MAX];
static size_t tx_size;
static size_t tx_sent;
static uint64_t quiet_start;
static uint64_t send_after;
static uart_task_callback_t callback;

bool uart_task_active(void)
{
    return state != UART_IDLE;
}

bool uart_task_start(platform_uart_t tx, platform_uart_t rx, const uint8_t *data, size_t size,
                     uart_task_callback_t on_done)
{
    return uart_task_start_after(tx, rx, data, size, 0, on_done);
}

bool uart_task_start_after(platform_uart_t tx, platform_uart_t rx, const uint8_t *data,
                           size_t size, uint32_t delay_us, uart_task_callback_t on_done)
{
    if (uart_task_active() || size == 0 || size > sizeof(tx_data) || !data || !on_done) {
        return false;
    }
    platform_uart_clear(rx);
    tx_uart = tx;
    rx_uart = rx;
    memcpy(tx_data, data, size);
    tx_size = size;
    tx_sent = 0;
    send_after = systime_us() + delay_us;
    callback = on_done;
    state = UART_SEND;
    return true;
}

void uart_task_handle(void)
{
    switch (state) {
    case UART_SEND:
        if (systime_us() < send_after) {
            break;
        }
        while (tx_sent < tx_size && platform_uart_put(tx_uart, tx_data[tx_sent])) {
            ++tx_sent;
        }
        if (tx_sent == tx_size && !platform_uart_busy(tx_uart)) {
            quiet_start = systime_us();
            state = UART_QUIET;
        }
        break;
    case UART_QUIET:
        if (systime_us() - quiet_start >= QUIET_US) {
            uint8_t received[UART_TASK_MAX];
            size_t size = platform_uart_read(rx_uart, received, sizeof(received));
            uint32_t errors = platform_uart_errors(rx_uart);
            state = UART_IDLE;
            callback(received, size, errors);
        }
        break;
    case UART_IDLE:
        break;
    }
}
