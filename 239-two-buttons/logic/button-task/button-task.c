#include "button-task/button-task.h"

#include "platform/platform.h"
#include "systime/systime.h"

#define DEBOUNCE_US 20000
#define HOLD_US 1000000

typedef enum {
    RELEASED,
    DEBOUNCE_PRESS,
    PRESSED,
    DEBOUNCE_RELEASE
} button_state_t;

typedef struct {
    button_state_t state;
    uint64_t changed_us;
    uint64_t pressed_us;
    uint32_t presses;
    bool hold_reported;
    button_task_callback_t on_press;
    button_task_callback_t on_hold;
} button_t;

static button_t buttons[2];

void button_task_init(unsigned index, button_task_callback_t on_press,
                      button_task_callback_t on_hold)
{
    buttons[index] = (button_t){.on_press = on_press, .on_hold = on_hold};
}

static void update_button(button_t *button, bool raw_pressed, uint64_t now)
{
    switch (button->state) {
    case RELEASED:
        if (raw_pressed) {
            button->changed_us = now;
            button->state = DEBOUNCE_PRESS;
        }
        break;
    case DEBOUNCE_PRESS:
        if (!raw_pressed) {
            button->state = RELEASED;
        } else if (now - button->changed_us >= DEBOUNCE_US) {
            button->state = PRESSED;
            button->pressed_us = now;
            button->hold_reported = false;
            ++button->presses;
            if (!button->on_hold && button->on_press) {
                button->on_press();
            }
        }
        break;
    case PRESSED:
        if (button->on_hold && !button->hold_reported && now - button->pressed_us >= HOLD_US) {
            button->hold_reported = true;
            button->on_hold();
        }
        if (!raw_pressed) {
            button->changed_us = now;
            button->state = DEBOUNCE_RELEASE;
        }
        break;
    case DEBOUNCE_RELEASE:
        if (raw_pressed) {
            button->state = PRESSED;
        } else if (now - button->changed_us >= DEBOUNCE_US) {
            button->state = RELEASED;
            if (button->on_hold && !button->hold_reported && button->on_press) {
                button->on_press();
            }
        }
        break;
    }
}

void button_task_handle(void)
{
    const uint64_t now = systime_us();
    for (unsigned index = 0; index < sizeof(buttons) / sizeof(buttons[0]); ++index) {
        update_button(&buttons[index], platform_button_read(index), now);
    }
}

bool button_task_is_pressed(unsigned index)
{
    return buttons[index].state == PRESSED || buttons[index].state == DEBOUNCE_RELEASE;
}

uint32_t button_task_get_press_count(unsigned index)
{
    return buttons[index].presses;
}
