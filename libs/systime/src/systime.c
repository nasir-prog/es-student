#include "systime/systime.h"

#include "pico/time.h"

uint64_t systime_us(void)
{
    return time_us_64();
}

uint64_t systime_ms(void)
{
    return systime_us() / 1000;
}

static bool period_elapsed(uint64_t *last_us, uint64_t period_us, uint64_t now)
{
    const uint64_t elapsed = now - *last_us;
    if (elapsed < period_us) {
        return false;
    }
    if (elapsed - period_us > period_us) {
        *last_us = now;
    } else {
        *last_us += period_us;
    }
    return true;
}

bool systime_period_elapsed(uint64_t *last_us, uint64_t period_us)
{
    return period_elapsed(last_us, period_us, systime_us());
}
