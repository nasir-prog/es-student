#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef void (*button_task_callback_t)(void);

void button_task_init(unsigned index, button_task_callback_t on_press,
                      button_task_callback_t on_hold);
void button_task_handle(void);
bool button_task_is_pressed(unsigned index);
uint32_t button_task_get_press_count(unsigned index);
