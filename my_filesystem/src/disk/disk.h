#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <sys/types.h>
#include "config.h"

// error codes
#define DISK_SUCCESS 0
#define DISK_ERROR -1
#define DISK_ERROR_NOT_FOUND -2
#define DISK_ERROR_ALREADY_ATTACHED -3
#define DISK_ERROR_NOT_ATTACHED -4
#define DISK_ERROR_INVALID_BLOCK -5
#define DISK_ERROR_IO -6
#define DISK_ERROR_NO_SPACE -7

typedef struct disk_emulator* disk_t;   // opaque pointer 

// === PUBLIC FUNCTIONS ===

// attachment
int disk_attach(const char* filename, size_t size, bool create_new, disk_t* disk);
int disk_detach(disk_t disk);

// I/O operations - block level
int disk_read_block(disk_t disk, int block_num, void* buffer);
int disk_write_block(disk_t disk, int block_num, const void* buffer);

// I/O Operations - raw level (for specific operations needing offset)
int disk_read(disk_t disk, off_t offset, void* buffer, size_t size);
int disk_write(disk_t disk, off_t offset, const void* buffer, size_t size);

// status and info management
int disk_get_size(disk_t disk);
int disk_get_blocks(disk_t disk);
int disk_get_block_size(disk_t disk);
bool disk_is_attached(disk_t disk);
const char* disk_get_filename(disk_t disk);

// synchronization
int disk_sync(disk_t disk);

// utilities
void disk_print_info(disk_t disk);
const char* disk_error_string(int error_code);