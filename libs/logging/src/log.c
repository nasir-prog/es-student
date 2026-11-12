#include "logging/log.h"

#include <stdarg.h>
#include <stdio.h>

void log_message(const char *level, const char *function, int line, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    printf("%s %s:%d ", level, function, line);
    vprintf(format, args);
    putchar('\n');
    va_end(args);
}
