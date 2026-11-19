#include "platform/platform.h"

#include "hardware/adc.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/sync.h"
#include "hardware/uart.h"
#include "pico/stdlib.h"

#define BUTTON_PIN 15
#define LINE_PIN 22
#define ADC_PIN 26
#define ADC_CHANNEL 0
#define UART0_TX_PIN 0
#define UART0_RX_PIN 1
#define UART1_TX_PIN 8
#define UART1_RX_PIN 9
#define UART_BAUD 115200
#define UART_FIFO_SIZE 32
#define RX_BUFFER_SIZE 128

static volatile uint8_t rx_buffer[RX_BUFFER_SIZE];
static volatile size_t rx_count;
static volatile uint32_t rx_errors[2];
static int adc_dma;
static uint16_t samples[PLATFORM_BLOCK_MAX];
static volatile bool block_active;
static volatile bool block_ready;
static uint32_t started_us;
static volatile uint32_t elapsed_us;

static uart_inst_t *uart_of(platform_uart_t uart)
{
    return uart == PLATFORM_UART0 ? uart0 : uart1;
}

static void on_uart0_rx(void)
{
    while (uart_is_readable(uart0)) {
        uint32_t word = uart_get_hw(uart0)->dr;
        rx_errors[PLATFORM_UART0] |= (word >> 8) & 0xfu;
        if (rx_count < RX_BUFFER_SIZE) {
            rx_buffer[rx_count++] = (uint8_t)word;
        }
    }
}

static void on_adc_done(void)
{
    dma_channel_acknowledge_irq0(adc_dma);
    adc_run(false);
    elapsed_us = time_us_32() - started_us;
    block_active = false;
    block_ready = true;
}

void platform_init(void)
{
    stdio_init_all();
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    gpio_init(BUTTON_PIN);
    gpio_pull_up(BUTTON_PIN);

    gpio_init(LINE_PIN);
    platform_line_set(PLATFORM_LINE_PUSH_PULL, false);
    adc_init();
    adc_gpio_init(ADC_PIN);

    uart_init(uart0, UART_BAUD);
    uart_init(uart1, UART_BAUD);
    platform_uart_loop_set(false);
    gpio_set_function(UART1_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART1_RX_PIN, GPIO_FUNC_UART);
    irq_set_exclusive_handler(UART0_IRQ, on_uart0_rx);
    irq_set_enabled(UART0_IRQ, true);

    adc_dma = dma_claim_unused_channel(true);
    dma_channel_set_irq0_enabled(adc_dma, true);
    irq_set_exclusive_handler(DMA_IRQ_0, on_adc_done);
    irq_set_enabled(DMA_IRQ_0, true);
}

void platform_led_set(bool enabled)
{
    gpio_put(PICO_DEFAULT_LED_PIN, enabled);
}

bool platform_button_read(void)
{
    return !gpio_get(BUTTON_PIN);
}

void platform_line_set(platform_line_mode_t mode, bool high)
{
    if (mode == PLATFORM_LINE_PUSH_PULL) {
        gpio_disable_pulls(LINE_PIN);
        gpio_put(LINE_PIN, high);
        gpio_set_dir(LINE_PIN, GPIO_OUT);
    } else {
        gpio_pull_up(LINE_PIN);
        gpio_put(LINE_PIN, false);
        gpio_set_dir(LINE_PIN, high ? GPIO_IN : GPIO_OUT);
    }
}

float platform_voltage_read(void)
{
    adc_select_input(ADC_CHANNEL);
    return adc_read() * (3.3f / 4096.0f);
}

void platform_uart_loop_set(bool internal)
{
    uint function = internal ? GPIO_FUNC_NULL : GPIO_FUNC_UART;
    gpio_set_function(UART0_TX_PIN, function);
    gpio_set_function(UART0_RX_PIN, function);
    if (internal) {
        hw_set_bits(&uart_get_hw(uart0)->cr, UART_UARTCR_LBE_BITS);
    } else {
        hw_clear_bits(&uart_get_hw(uart0)->cr, UART_UARTCR_LBE_BITS);
    }
}

void platform_uart_clear(platform_uart_t uart)
{
    uart_inst_t *port = uart_of(uart);
    uint32_t irq_state = save_and_disable_interrupts();
    for (size_t i = 0; i < UART_FIFO_SIZE && uart_is_readable(port); ++i) {
        (void)uart_get_hw(port)->dr;
    }
    uart_get_hw(port)->rsr = 0;
    rx_errors[uart] = 0;
    if (uart == PLATFORM_UART0) {
        rx_count = 0;
    }
    restore_interrupts(irq_state);
}

bool platform_uart_put(platform_uart_t uart, uint8_t byte)
{
    uart_inst_t *port = uart_of(uart);
    if (!uart_is_writable(port)) {
        return false;
    }
    uart_get_hw(port)->dr = byte;
    return true;
}

bool platform_uart_busy(platform_uart_t uart)
{
    return (uart_get_hw(uart_of(uart))->fr & UART_UARTFR_BUSY_BITS) != 0;
}

size_t platform_uart_read(platform_uart_t uart, uint8_t *data, size_t size)
{
    uart_inst_t *port = uart_of(uart);
    size_t count = 0;
    uint32_t irq_state = save_and_disable_interrupts();
    rx_errors[uart] |= uart_get_hw(port)->rsr & 0xfu;
    if (uart == PLATFORM_UART0) {
        while (count < size && count < rx_count) {
            data[count] = rx_buffer[count];
            ++count;
        }
        rx_count = 0;
    }
    while (count < size && uart_is_readable(port)) {
        uint32_t word = uart_get_hw(port)->dr;
        rx_errors[uart] |= (word >> 8) & 0xfu;
        data[count++] = (uint8_t)word;
    }
    restore_interrupts(irq_state);
    return count;
}

uint32_t platform_uart_errors(platform_uart_t uart)
{
    return rx_errors[uart] | (uart_get_hw(uart_of(uart))->rsr & 0xfu);
}

uint32_t platform_uart_baud_set(platform_uart_t uart, uint32_t baud)
{
    return uart_set_baudrate(uart_of(uart), baud);
}

void platform_uart_parity_set(platform_uart_t uart, platform_parity_t parity)
{
    static const uart_parity_t formats[] = {UART_PARITY_NONE, UART_PARITY_EVEN, UART_PARITY_ODD};
    uart_set_format(uart_of(uart), 8, 1, formats[parity]);
}

void platform_uart_rx_irq_set(bool on)
{
    uart_set_irq_enables(uart0, on, false);
}

bool platform_block_start(uint32_t count)
{
    if (block_active || block_ready || count == 0 || count > PLATFORM_BLOCK_MAX) {
        return false;
    }
    adc_fifo_drain();
    adc_select_input(ADC_CHANNEL);
    adc_fifo_setup(true, true, 1, false, false);
    adc_set_clkdiv(0);
    dma_channel_config config = dma_channel_get_default_config(adc_dma);
    channel_config_set_transfer_data_size(&config, DMA_SIZE_16);
    channel_config_set_read_increment(&config, false);
    channel_config_set_write_increment(&config, true);
    channel_config_set_dreq(&config, DREQ_ADC);
    block_ready = false;
    block_active = true;
    dma_channel_configure(adc_dma, &config, samples, &adc_hw->fifo, count, true);
    started_us = time_us_32();
    adc_run(true);
    return true;
}

bool platform_block_done(void)
{
    if (!block_ready) {
        return false;
    }
    // The final conversion can outlast the DMA interrupt by one sample period.
    if (!(adc_hw->cs & ADC_CS_READY_BITS)) {
        return false;
    }
    adc_fifo_drain();
    adc_fifo_setup(false, false, 0, false, false);
    block_ready = false;
    return true;
}

const uint16_t *platform_block_samples(void) { return samples; }
uint32_t platform_block_time_us(void) { return elapsed_us; }
