#pragma once

#include "stdio-text-protocol/stdio-text-protocol.h"

void cmd_edge(const command_t *cmd);
void cmd_loop(const command_t *cmd);
void cmd_echo(const command_t *cmd);
void cmd_link(const command_t *cmd);
void cmd_baud_shift(const command_t *cmd);
void cmd_text(const command_t *cmd);
void cmd_parity(const command_t *cmd);
void cmd_block(const command_t *cmd);
void cmd_scope(const command_t *cmd);
