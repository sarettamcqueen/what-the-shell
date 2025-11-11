#pragma once

#include "common.h"
#include "disk.h"
#include "bitmap.h"

// === VALIDATION ===

// checks if a dentry structure is valid
bool dentry_is_valid(const struct dentry* dentry);

// checks if a filename is valid according to filesystem rules
bool dentry_is_valid_name(const char* name);

// === CREATION ===

// creates a dentry structure in memory (not written to disk)
int dentry_create(const char* name, uint32_t inode_num, 
                  uint8_t file_type, struct dentry* out_dentry);

// === DIRECTORY OPERATIONS ===

// finds a dentry by name within a directory (finds first free slot and writes the dentry)
int dentry_find(disk_t disk, uint32_t dir_inode_num, 
                const char* name, struct dentry* out_dentry, 
                uint32_t* out_index);

// adds a new dentry to a directory
int dentry_add(disk_t disk, uint32_t dir_inode_num, 
               const struct dentry* new_dentry,
               struct bitmap* block_bitmap);

// removes a dentry from a directory by marking it as free
int dentry_remove(disk_t disk, uint32_t dir_inode_num, const char* name);

// lists all valid dentries in a directory
int dentry_list(disk_t disk, uint32_t dir_inode_num, 
                struct dentry** out_entries, uint32_t* out_count);

// === UTILITIES ===

// prints a dentry to stdout for debugging
void dentry_print(const struct dentry* dentry);
 