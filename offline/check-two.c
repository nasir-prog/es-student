#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "button-task/button-task.h"
#include "led-task/led-task.h"

static bool inputs[2];
static uint64_t clock_us;

uint64_t time_us_64(void) { return clock_us; }
bool platform_button_read(unsigned index) { return inputs[index]; }
void platform_led_set(bool level) { (void)level; }

static void sample_after(uint64_t delay)
{
    clock_us += delay;
    button_task_handle();
}

static void set_button(unsigned index, bool pressed)
{
    inputs[index] = pressed;
    sample_after(0);
    sample_after(20000);
}

int main(void)
{
    led_task_init();
    button_task_init(0, led_task_next_state, led_task_blink);
    button_task_init(1, led_task_next_period, NULL);
    set_button(0, true);
    assert(led_task_get_state() == LED_STATE_BLINK);
    set_button(0, false);
    assert(led_task_get_state() == LED_STATE_OFF);

    set_button(0, true);
    set_button(1, true);
    assert(led_task_get_period_ms() == 250);
    set_button(1, false);
    set_button(1, true);
    assert(led_task_get_period_ms() == 500);
    set_button(1, false);
    set_button(1, true);
    assert(led_task_get_period_ms() == 1000);
    set_button(1, false);

    sample_after(1000000);
    assert(led_task_get_state() == LED_STATE_BLINK);
    sample_after(1000000);
    set_button(0, false);
    assert(led_task_get_state() == LED_STATE_BLINK);
    assert(button_task_get_press_count(0) == 2);
    assert(button_task_get_press_count(1) == 3);
    puts("Учебная модель 2.3.9 и 2.3.9д: обе кнопки, короткое и долгое нажатия проверены.");
    return 0;
}
