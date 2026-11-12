#include "pi-task/pi-task.h"

#include "systime/systime.h"

#define PI_TERMS_PER_CALL 64

static pi_state_t state;
static uint32_t total_terms;
static uint32_t completed;
static double sum;
static uint64_t started_us;
static pi_result_t result;

bool pi_task_start(uint32_t terms)
{
    if (state == PI_RUNNING || terms == 0) {
        return false;
    }
    state = PI_RUNNING;
    total_terms = terms;
    completed = 0;
    sum = 0;
    started_us = systime_us();
    return true;
}

void pi_task_handle(void)
{
    if (state != PI_RUNNING) {
        return;
    }

    uint32_t remaining = total_terms - completed;
    uint32_t portion = remaining < PI_TERMS_PER_CALL ? remaining : PI_TERMS_PER_CALL;
    for (uint32_t i = 0; i < portion; ++i, ++completed) {
        double term = 1.0 / (2.0 * completed + 1.0);
        sum += (completed & 1) ? -term : term;
    }
    if (completed == total_terms) {
        result.value = 4.0 * sum;
        result.time_ms = (systime_us() - started_us) / 1000;
        state = PI_READY;
    }
}

pi_state_t pi_task_get_result(pi_result_t *out)
{
    if (state == PI_READY) {
        *out = result;
    }
    return state;
}
