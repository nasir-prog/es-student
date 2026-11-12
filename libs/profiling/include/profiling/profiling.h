#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifndef PROFILING_ENABLED
#define PROFILING_ENABLED 1
#endif

typedef struct {
    const char *name;
    uint64_t count;
    double mean_us;
    uint32_t max_us;
} profiling_result_t;

#if PROFILING_ENABLED
void profiling_stopwatch_init(unsigned number, const char *name);
void profiling_start(unsigned number);
void profiling_stop(unsigned number);
bool profiling_get(unsigned number, profiling_result_t *result);
void profiling_reset(void);
#else
#define profiling_stopwatch_init(number, name) ((void)0)
#define profiling_start(number) ((void)0)
#define profiling_stop(number) ((void)0)
#define profiling_reset() ((void)0)
#endif
