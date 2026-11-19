#include "trace-output.h"

#undef printf
#undef puts
#undef fputs
#undef putchar

#include <stdbool.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>

// Add virtual time to every line from the firmware, including partial writes.
static bool line_start = true;
static uint64_t now_us;

void trace_set_time(uint64_t time_us)
{
    now_us = time_us;
}

void trace_finish_step(void)
{
    fputs("\n@@END@@\n", stdout);
    line_start = true;
}

static int trace_character(int character)
{
    if (line_start) {
        fprintf(stdout, "@@US %" PRIu64 "@@ ", now_us);
    }
    line_start = character == '\n';
    return putc(character, stdout);
}

int trace_putchar(int character)
{
    return trace_character(character);
}

int trace_fputs(const char *string, FILE *stream)
{
    if (stream != stdout) {
        return fputs(string, stream);
    }
    for (; *string; ++string) {
        if (trace_character((unsigned char)*string) == EOF) {
            return EOF;
        }
    }
    return 0;
}

int trace_puts(const char *string)
{
    return trace_fputs(string, stdout) == EOF ? EOF : trace_character('\n');
}

int trace_printf(const char *format, ...)
{
    char text[512];
    va_list args;
    va_start(args, format);
    int length = vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    if (length < 0 || (size_t)length >= sizeof(text) || trace_fputs(text, stdout) == EOF) {
        return -1;
    }
    return length;
}
