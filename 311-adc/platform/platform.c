#include "platform/platform.h"

#include "hardware/adc.h"
#include "pico/stdlib.h"

#define BUTTON_PIN 15
#define ADC_PIN 26
#define ADC_CHANNEL 0
#define TEMP_CHANNEL 4
#define ADC_VREF_V 3.3f
#define ADC_LEVELS 4096
#define CHARGE_PIN 22

void platform_init(void)
{
    stdio_init_all();
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    gpio_init(BUTTON_PIN);
    gpio_pull_up(BUTTON_PIN);
    gpio_init(CHARGE_PIN);
    gpio_put(CHARGE_PIN, false);
    gpio_set_dir(CHARGE_PIN, GPIO_OUT);
    adc_init();
    adc_gpio_init(ADC_PIN);
    adc_set_temp_sensor_enabled(true);
}

void platform_led_set(bool enabled)
{
    gpio_put(PICO_DEFAULT_LED_PIN, enabled);
}

bool platform_button_read(void)
{
    return !gpio_get(BUTTON_PIN);
}

float platform_voltage_read(void)
{
    adc_select_input(ADC_CHANNEL);
    return adc_read() * ADC_VREF_V / ADC_LEVELS;
}

float platform_temperature_read(void)
{
    adc_select_input(TEMP_CHANNEL);
    const float voltage = adc_read() * ADC_VREF_V / ADC_LEVELS;
    return 27.0f - (voltage - 0.706f) / 0.001721f;
}

void platform_charge_set(bool enabled)
{
    gpio_put(CHARGE_PIN, enabled);
}
