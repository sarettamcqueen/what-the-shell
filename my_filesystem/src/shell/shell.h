#pragma once

#include <stddef.h>
#include "fs.h"
#include "parser.h"

#define SHELL_EXIT 999

/**
 * Runs the interactive shell.
 * Handles input, parsing, and command dispatch.
 */
void shell_run(void);

/**
 * Dispatches a parsed command to the correct handler.
 */
int shell_dispatch(filesystem_t** current_fs, int argc, char** argv);
