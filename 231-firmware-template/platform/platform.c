#include "platform/platform.h"

#include "pico/stdlib.h"

#define BUTTON_PIN 15

void platform_init(void)
{
    stdio_init_all();
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    gpio_init(BUTTON_PIN);
    gpio_pull_up(BUTTON_PIN);
}

void platform_led_set(bool enabled)
{
    gpio_put(PICO_DEFAULT_LED_PIN, enabled);
}

bool platform_button_read(void)
{
    return !gpio_get(BUTTON_PIN);
}
