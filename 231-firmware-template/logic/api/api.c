#include "api/api.h"

#include <stdio.h>
#include <string.h>
#include "api/api-commands.h"

void api_handle(const command_t *command)
{
    if (command->truncated) {
        puts("error: command longer than 63 characters or 4 arguments");
        return;
    }
    if (strcmp(command->name, "help") == 0) {
        printf("%-18s %s\n", "help", "list of commands");
        for (const api_command_t *entry = api_commands; entry->name; ++entry) {
            printf("%-18s %s\n", entry->name, entry->help);
        }
        return;
    }
    for (const api_command_t *entry = api_commands; entry->name; ++entry) {
        if (strcmp(command->name, entry->name) == 0) {
            entry->callback(command);
            return;
        }
    }
    printf("error: unknown command '%s', try help\n", command->name);
}
