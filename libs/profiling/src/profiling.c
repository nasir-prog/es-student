#include "profiling/profiling.h"

#include "systime/systime.h"

#if PROFILING_ENABLED
#define STOPWATCH_COUNT 16

typedef struct {
    const char *name;
    uint64_t start_us;
    uint64_t count;
    uint64_t total_us;
    uint32_t max_us;
} stopwatch_t;

static stopwatch_t watches[STOPWATCH_COUNT];

void profiling_stopwatch_init(unsigned number, const char *name)
{
    watches[number] = (stopwatch_t){.name = name};
}

void profiling_start(unsigned number)
{
    watches[number].start_us = systime_us();
}

void profiling_stop(unsigned number)
{
    stopwatch_t *watch = &watches[number];
    const uint32_t elapsed = (uint32_t)(systime_us() - watch->start_us);
    ++watch->count;
    watch->total_us += elapsed;
    if (elapsed > watch->max_us) {
        watch->max_us = elapsed;
    }
}

bool profiling_get(unsigned number, profiling_result_t *result)
{
    if (number >= STOPWATCH_COUNT || !watches[number].name) {
        return false;
    }
    const stopwatch_t *watch = &watches[number];
    *result = (profiling_result_t){
        .name = watch->name,
        .count = watch->count,
        .mean_us = watch->count ? (double)watch->total_us / (double)watch->count : 0,
        .max_us = watch->max_us,
    };
    return true;
}

void profiling_reset(void)
{
    for (unsigned index = 0; index < STOPWATCH_COUNT; ++index) {
        watches[index].count = 0;
        watches[index].total_us = 0;
        watches[index].max_us = 0;
    }
}
#endif
