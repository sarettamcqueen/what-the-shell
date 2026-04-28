#pragma once

#include <stddef.h>
#include "vfs.h"
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
int shell_dispatch(vfs_t* vfs, int argc, char** argv);
