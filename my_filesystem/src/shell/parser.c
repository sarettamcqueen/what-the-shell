#include "parser.h"
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

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

    while (*p && isspace((unsigned char)*p)) {
        p++;
    }

    while (*p) {
        if (argc >= max_tokens - 1) {
            return -1;
        }

        if (*p == '"') {
            bool closed = false;
            p++;  // skip opening quote

            char* token_start = p;
            char* out = p;

            while (*p) {
                if (*p == '\\' && (p[1] == '"' || p[1] == '\\')) {
                    *out++ = p[1];
                    p += 2;
                } else if (*p == '"') {
                    p++;
                    closed = true;
                    break;
                } else {
                    *out++ = *p++;
                }
            }

            if (!closed) {
                return -1;
            }

            *out = '\0';
            argv[argc++] = token_start;
        } else {
            char* token_start = p;

            while (*p && !isspace((unsigned char)*p)) {
                p++;
            }

            if (*p) {
                *p = '\0';
                p++;
            }

            argv[argc++] = token_start;
        }

        while (*p && isspace((unsigned char)*p)) {
            p++;
        }
    }

    argv[argc] = NULL;
    return argc;
}
