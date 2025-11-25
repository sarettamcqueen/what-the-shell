#include "parser.h"
#include <string.h>
#include <ctype.h>

/*
 * Removes trailing newline
 */
void trim_newline(char* s) {
    size_t len = strlen(s);
    if (len > 0 && s[len - 1] == '\n')
        s[len - 1] = '\0';
}

/*
 * Tokenizer with support for quotes.
 *
 * Example:
 *   input:  write "/path to/file" "hello world"
 *   output: argv = ["write", "/path to/file", "hello world"]
 */
int parse_line(char* line, char** argv, int max_tokens) {
    int argc = 0;
    char* p = line;

    // skip initial blanks
    while (*p && isspace((unsigned char)*p))
        p++;

    while (*p && argc < max_tokens) {
        // handle quoted token
        if (*p == '"') {
            p++; // skip "
            argv[argc++] = p;

            // find matching "
            while (*p && *p != '"')
                p++;

            if (*p == '"') {
                *p = '\0'; // terminate token
                p++;
            }

        } else {
            // unquoted token
            argv[argc++] = p;

            while (*p && !isspace((unsigned char)*p))
                p++;

            if (*p) {
                *p = '\0';
                p++;
            }
        }

        // skip until next non-space or end
        while (*p && isspace((unsigned char)*p))
            p++;
    }

    return argc;
}
