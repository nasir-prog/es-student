#include "stdio-text-protocol/stdio-text-protocol.h"

#include <stdio.h>
#include "pico/stdio.h"

static char line[64];
static unsigned length;
static bool overflow;
static command_t command;

void stdio_text_protocol_init(void)
{
    length = 0;
    overflow = false;
}

static const command_t *complete_command(void)
{
    line[length] = '\0';
    const bool truncated = overflow;
    stdio_text_protocol_init();
    command = (command_t){.truncated = truncated};

    char *cursor = line;
    while (*cursor != '\0') {
        if (*cursor == ' ') {
            ++cursor;
            continue;
        }
        if (!command.name) {
            command.name = cursor;
        } else if (command.argc < 4) {
            command.argv[command.argc++] = cursor;
        } else {
            command.truncated = true;
            break;
        }
        while (*cursor != ' ' && *cursor != '\0') {
            ++cursor;
        }
        if (*cursor == ' ') {
            *cursor++ = '\0';
        }
    }
    return command.name || command.truncated ? &command : NULL;
}

const command_t *stdio_text_protocol_handle(void)
{
    const int ch = getchar_timeout_us(0);
    if (ch == '\r' || ch == '\n') {
        if (length == 0) {
            return NULL;
        }
        puts("");
        return complete_command();
    }
    if (ch == 0x08 || ch == 0x7f) {
        if (length != 0) {
            --length;
            overflow = false;
            fputs("\b \b", stdout);
        }
    } else if (ch >= 0x20 && ch <= 0x7e) {
        if (length < sizeof(line) - 1) {
            line[length++] = (char)ch;
            putchar(ch);
        } else {
            overflow = true;
        }
    }
    return NULL;
}
