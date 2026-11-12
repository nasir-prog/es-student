#pragma once

#include <stdbool.h>

void platform_init(void);
void platform_led_set(bool enabled);
bool platform_button_read(void);

float platform_voltage_read(void);
float platform_temperature_read(void);
void platform_charge_set(bool enabled);
