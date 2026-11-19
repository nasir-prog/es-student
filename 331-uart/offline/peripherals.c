#include "platform/platform.h"

#include <math.h>
#include <string.h>
#include "systime/systime.h"

#define FIFO_SIZE 32
#define ADC_MAX 4095
#define CAPACITANCE_UF 4.7
#define SUPPLY_V 3.3

typedef struct {
    double start;
    double bit_us;
    uint8_t byte;
    platform_parity_t parity;
} frame_t;

typedef struct {
    uint32_t baud;
    platform_parity_t parity;
    frame_t tx[FIFO_SIZE];
    size_t tx_head;
    size_t tx_count;
    uint8_t rx[128];
    size_t rx_count;
    uint32_t errors;
} port_t;

static port_t ports[] = {{.baud = 115200}, {.baud = 115200}};
static bool internal_loop;
static bool rx_irq;
static bool high;
static double previous_voltage;
static uint64_t line_changed;
static platform_line_mode_t line_mode;

static uint64_t capture_started;
static uint32_t capture_count;
static uint16_t capture[PLATFORM_BLOCK_MAX];

void platform_line_set(platform_line_mode_t mode, bool new_high)
{
    previous_voltage = platform_voltage_read();
    line_changed = systime_us();
    line_mode = mode;
    high = new_high;
}

float platform_voltage_read(void)
{
    // The 1 kΩ external resistor is always in series; the model adds 60 kΩ for pull-up.
    double tau = (high && line_mode == PLATFORM_LINE_OPEN_DRAIN ? 61000 : 1000) * CAPACITANCE_UF;
    double target = high ? SUPPLY_V : 0.0;
    return (float)(target + (previous_voltage - target) *
                   exp(-(double)(systime_us() - line_changed) / tau));
}

static unsigned bit_count(platform_parity_t parity)
{
    return parity == PLATFORM_PARITY_NONE ? 10u : 11u;
}

static unsigned parity_bit(uint8_t byte, platform_parity_t parity)
{
    unsigned bit = parity == PLATFORM_PARITY_ODD;
    for (unsigned i = 0; i < 8; ++i) {
        bit ^= (byte >> i) & 1u;
    }
    return bit;
}

static unsigned line_bit(const frame_t *frame, double time)
{
    if (time < frame->start) {
        return 1;
    }
    double offset = (time - frame->start) / frame->bit_us;
    if (offset >= bit_count(frame->parity)) {
        return 1;
    }
    unsigned index = (unsigned)offset;
    if (index == 0) {
        return 0;
    }
    if (index <= 8) {
        return (frame->byte >> (index - 1)) & 1u;
    }
    return index == 9 && frame->parity != PLATFORM_PARITY_NONE ?
           parity_bit(frame->byte, frame->parity) : 1u;
}

// The receiver samples the line in the center of each bit according to its own clock.
static void receive(port_t *receiver, const frame_t *frame)
{
    const size_t capacity = receiver == &ports[0] && rx_irq ? sizeof(receiver->rx) : FIFO_SIZE;
    if (receiver->rx_count == capacity) {
        receiver->errors |= PLATFORM_UART_OE;
        return;
    }
    double rx_bit_us = 1000000.0 / receiver->baud;
    uint8_t byte = 0;
    for (unsigned i = 0; i < 8; ++i) {
        byte |= (uint8_t)(line_bit(frame, frame->start + (1.5 + i) * rx_bit_us) << i);
    }
    if (receiver->parity != PLATFORM_PARITY_NONE &&
        line_bit(frame, frame->start + 9.5 * rx_bit_us) != parity_bit(byte, receiver->parity)) {
        receiver->errors |= PLATFORM_UART_PE;
    }
    if (!line_bit(frame, frame->start + (bit_count(receiver->parity) - 0.5) * rx_bit_us)) {
        receiver->errors |= PLATFORM_UART_FE;
    }
    receiver->rx[receiver->rx_count++] = byte;
}

static void advance(platform_uart_t uart)
{
    port_t *port = &ports[uart];
    while (port->tx_count) {
        frame_t *frame = &port->tx[port->tx_head];
        if (frame->start + bit_count(frame->parity) * frame->bit_us > systime_us()) {
            break;
        }
        // An external loopback scenario connects TX0 to RX0. Bidirectional
        // scenarios cross TX0 to RX1 and TX1 to RX0.
        extern bool model_external_loopback(void);
        bool loop = uart == PLATFORM_UART0 && (internal_loop || model_external_loopback());
        receive(&ports[loop ? PLATFORM_UART0 : 1 - uart], frame);
        port->tx_head = (port->tx_head + 1) % FIFO_SIZE;
        --port->tx_count;
    }
}

void platform_uart_loop_set(bool internal) { internal_loop = internal; }

void platform_uart_clear(platform_uart_t uart)
{
    advance(PLATFORM_UART0);
    advance(PLATFORM_UART1);
    ports[uart].rx_count = 0;
    ports[uart].errors = 0;
}

static void sample_frame(const frame_t *frame)
{
    if (capture_count == 0) {
        return;
    }
    double end = frame->start + bit_count(frame->parity) * frame->bit_us;
    for (uint32_t i = 0; i < capture_count; ++i) {
        double time = (double)capture_started + 2.0 * i;
        if (time >= frame->start && time < end) {
            capture[i] = line_bit(frame, time) ? ADC_MAX : 0;
        }
    }
}

bool platform_uart_put(platform_uart_t uart, uint8_t byte)
{
    advance(uart);
    port_t *port = &ports[uart];
    if (port->tx_count == FIFO_SIZE) {
        return false;
    }
    size_t index = (port->tx_head + port->tx_count) % FIFO_SIZE;
    double start = systime_us();
    if (port->tx_count) {
        const frame_t *previous = &port->tx[(index + FIFO_SIZE - 1) % FIFO_SIZE];
        start = previous->start + bit_count(previous->parity) * previous->bit_us;
    }
    port->tx[index] = (frame_t){start, 1000000.0 / port->baud, byte, port->parity};
    ++port->tx_count;
    if (uart == PLATFORM_UART0) {
        sample_frame(&port->tx[index]);
    }
    return true;
}

bool platform_uart_busy(platform_uart_t uart)
{
    advance(uart);
    return ports[uart].tx_count != 0;
}

size_t platform_uart_read(platform_uart_t uart, uint8_t *data, size_t size)
{
    port_t *port = &ports[uart];
    size_t count = port->rx_count < size ? port->rx_count : size;
    memcpy(data, port->rx, count);
    memmove(port->rx, port->rx + count, port->rx_count - count);
    port->rx_count -= count;
    return count;
}

uint32_t platform_uart_errors(platform_uart_t uart) { return ports[uart].errors; }

uint32_t platform_uart_baud_set(platform_uart_t uart, uint32_t baud)
{
    // PL011 divider expressed in units of 1/64, with clk_peri = 125 MHz.
    const uint32_t scaled_divider = (4u * 125000000u + baud / 2) / baud;
    const uint32_t actual = 4u * 125000000u / scaled_divider;
    ports[uart].baud = actual;
    return actual;
}

void platform_uart_parity_set(platform_uart_t uart, platform_parity_t parity)
{
    ports[uart].parity = parity;
}

void platform_uart_rx_irq_set(bool on) { rx_irq = on; }

bool platform_block_start(uint32_t count)
{
    if (count == 0 || count > PLATFORM_BLOCK_MAX || capture_count != 0) {
        return false;
    }
    capture_count = count;
    capture_started = systime_us();
    for (uint32_t i = 0; i < count; ++i) {
        capture[i] = ADC_MAX;
    }
    return true;
}

static uint32_t capture_time_us;

bool platform_block_done(void)
{
    if (capture_count == 0 || systime_us() - capture_started < 2 * capture_count) {
        return false;
    }
    capture_time_us = 2 * capture_count;
    capture_count = 0;
    return true;
}

const uint16_t *platform_block_samples(void) { return capture; }
uint32_t platform_block_time_us(void) { return capture_time_us; }
