#pragma once

#include "common.h"
#include "disk.h"

// load superblock from disk
int superblock_read(disk_t disk, struct superblock* sb);

// writes superblock on disk 
int superblock_write(disk_t disk, const struct superblock* sb);

// initializes and saves a new superblock (format)
int superblock_init(disk_t disk, struct superblock* sb, size_t total_blocks, size_t total_inodes);

// prints superblock info
void superblock_print(const struct superblock* sb);

// checks magic number
int is_superblock_valid(const struct superblock* sb);
