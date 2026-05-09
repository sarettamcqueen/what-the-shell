#include "common.h"
#include "disk.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

int map_disk_error(int disk_error) {
    switch (disk_error) {
        case DISK_SUCCESS:
            return SUCCESS;
        case DISK_ERROR_NOT_FOUND:
            return ERROR_NOT_FOUND;
        case DISK_ERROR_INVALID_BLOCK:
            return ERROR_INVALID;
        case DISK_ERROR_IO:
            return ERROR_IO;
        case DISK_ERROR_NO_SPACE:
            return ERROR_NO_SPACE;
        case DISK_ERROR_NOT_ATTACHED:
            return ERROR_IO;
        default:
            return ERROR_GENERIC;
    }
}

const char* error_string(int error_code) {
    switch (error_code) {
        case SUCCESS:               return "Success";
        case ERROR_GENERIC:         return "Generic error";
        case ERROR_NOT_FOUND:       return "File or directory not found";
        case ERROR_EXISTS:          return "File or directory already exists";
        case ERROR_NO_SPACE:        return "No space left on device";
        case ERROR_INVALID:         return "Invalid argument";
        case ERROR_IO:              return "I/O error";
        case ERROR_PERMISSION:      return "Permission denied";
        case ERROR_ROOT_REQUIRED:   return "First mount must be on root directory '/'";
        case ERROR_BUSY:            return "Device or resource busy";
        default:                    return "Unknown error";
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