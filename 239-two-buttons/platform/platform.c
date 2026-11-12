#include "platform/platform.h"

#include "pico/stdlib.h"

static const unsigned button_pins[] = {15, 14};

void platform_init(void)
{
    stdio_init_all();
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    for (unsigned i = 0; i < sizeof(button_pins) / sizeof(button_pins[0]); ++i) {
        gpio_init(button_pins[i]);
        gpio_pull_up(button_pins[i]);
    }
}

void platform_led_set(bool enabled)
{
    gpio_put(PICO_DEFAULT_LED_PIN, enabled);
}

bool platform_button_read(unsigned button)
{
    return !gpio_get(button_pins[button]);
}
