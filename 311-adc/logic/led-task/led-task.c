#include "led-task/led-task.h"

#include "platform/platform.h"
#include "systime/systime.h"

static led_state_t state;
static uint32_t blink_period_ms = 1000;
static uint64_t last_toggle_us;
static bool level;

void led_task_init(void)
{
    led_task_set_state(LED_STATE_BLINK);
}

void led_task_handle(void)
{
    switch (state) {
    case LED_STATE_BLINK:
        if (systime_period_elapsed(&last_toggle_us, (uint64_t)blink_period_ms * 500)) {
            level = !level;
            platform_led_set(level);
        }
        break;
    case LED_STATE_OFF:
    case LED_STATE_ON:
        break;
    }
}

void led_task_set_state(led_state_t new_state)
{
    state = new_state;
    level = state == LED_STATE_ON || state == LED_STATE_BLINK;
    last_toggle_us = systime_us();
    platform_led_set(level);
}

led_state_t led_task_get_state(void)
{
    return state;
}

void led_task_set_period_ms(uint32_t period_ms)
{
    blink_period_ms = period_ms;
    last_toggle_us = systime_us();
}

uint32_t led_task_get_period_ms(void)
{
    return blink_period_ms;
}

void led_task_next_state(void)
{
    led_task_set_state((state + 1) % (LED_STATE_BLINK + 1));
}
