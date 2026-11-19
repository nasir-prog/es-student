#pragma once

#include <stdint.h>
#include <stdio.h>

void trace_set_time(uint64_t time_us);
void trace_finish_step(void);
int trace_printf(const char *format, ...) __attribute__((format(printf, 1, 2)));
int trace_puts(const char *string);
int trace_fputs(const char *string, FILE *stream);
int trace_putchar(int character);

// This header is preincluded only in the native model.
#define printf trace_printf
#define puts trace_puts
#define fputs trace_fputs
#define putchar trace_putchar
