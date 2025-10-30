#include "common.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

const char* error_string(int error_code) {
    switch (error_code) {
        case SUCCESS:           return "Success";
        case ERROR_GENERIC:     return "Generic error";
        case ERROR_NOT_FOUND:   return "File or directory not found";
        case ERROR_EXISTS:      return "File or directory already exists";
        case ERROR_NO_SPACE:    return "No space left on device";
        case ERROR_INVALID:     return "Invalid argument";
        case ERROR_IO:          return "I/O error";
        case ERROR_PERMISSION:  return "Permission denied";
        default:                return "Unknown error";
    }
}

void print_timestamp(time_t timestamp) {
    if (timestamp == 0) {
        printf("never");
        return;
    }
    
    struct tm* tm_info = localtime(&timestamp);
    char buffer[26];
    strftime(buffer, 26, "%Y-%m-%d %H:%M:%S", tm_info);
    printf("%s", buffer);
}

bool is_valid_filename(const char* filename) {
    if (!filename || *filename == '\0') {
        return false;
    }
    
    size_t len = strlen(filename);
    if (len >= MAX_FILENAME) {
        return false;
    }
    
    if (strcmp(filename, ".") == 0 || strcmp(filename, "..") == 0) {
        return false;
    }
    
    if (strchr(filename, '/')) {
        return false;
    }
    
    for (const char* p = filename; *p; p++) {
        if (iscntrl(*p)) {
            return false;
        }
    }
    
    return true;
}