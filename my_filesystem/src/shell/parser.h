#pragma once
#include <stddef.h>

/**
 * Parses a command line into argv-style tokens.
 * Handles:
 *   - multiple spaces / tabs
 *   - quoted strings: "hello world"
 *
 * @param line       Input string (modified in place)
 * @param argv       Output array of char* tokens
 * @param max_tokens Maximum number of tokens allowed
 * @return           Number of tokens parsed
 */
int parse_line(char* line, char** argv, int max_tokens);

/**
 * Utility: trims trailing newline '\n'
 */
void trim_newline(char* s);
