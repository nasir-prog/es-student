#include "button-task/button-task.h"

#include "platform/platform.h"
#include "systime/systime.h"

#define DEBOUNCE_US 20000

typedef enum {
    RELEASED,
    DEBOUNCE_PRESS,
    PRESSED,
    DEBOUNCE_RELEASE
} button_state_t;

static button_state_t state;
static uint64_t changed_us;
static uint32_t presses;
static button_task_callback_t callback;

void button_task_init(button_task_callback_t on_press)
{
    state = RELEASED;
    presses = 0;
    callback = on_press;
}

void button_task_handle(void)
{
    const bool raw_pressed = platform_button_read();
    const uint64_t now = systime_us();

    switch (state) {
    case RELEASED:
        if (raw_pressed) {
            changed_us = now;
            state = DEBOUNCE_PRESS;
        }
        break;
    case DEBOUNCE_PRESS:
        if (!raw_pressed) {
            state = RELEASED;
        } else if (now - changed_us >= DEBOUNCE_US) {
            state = PRESSED;
            ++presses;
            if (callback) {
                callback();
            }
        }
        break;
    case PRESSED:
        if (!raw_pressed) {
            changed_us = now;
            state = DEBOUNCE_RELEASE;
        }
        break;
    case DEBOUNCE_RELEASE:
        if (raw_pressed) {
            state = PRESSED;
        } else if (now - changed_us >= DEBOUNCE_US) {
            state = RELEASED;
        }
        break;
    }
}

bool button_task_is_pressed(void)
{
    return state == PRESSED || state == DEBOUNCE_RELEASE;
}

uint32_t button_task_get_press_count(void)
{
    return presses;
}
