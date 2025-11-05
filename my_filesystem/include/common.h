/**
   Common header file.
 
   This file defines constants, data structures, macros, and some utility functions
   shared across the various modules that make up the filesystem.
  
   Contents:
    - Global constants (block size, limits, standard error codes)
    - Inode type definitions (free, file, directory)
    - Core filesystem structures:
        - superblock: global filesystem metadata
        - inode: descriptor of a file/directory
        - dentry: directory entry mapping a name to an inode
    - Utility macros for block alignment and size calculations
    - Function prototypes for common operations (error handling,
      timestamp printing, and filename validation)
  
   All structures are marked with `__attribute__((packed))` to prevent
   automatic padding and ensure that their sizes align precisely with
   the expected block layout of the filesystem.
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <time.h>
#include <fcntl.h>
#include "config.h"

// === GLOBAL CONSTANTS ===
#define INODE_SIZE 128                                  // TODO: add a compile-time check that sizeof(struct inode) == INODE_SIZE
#define INODES_PER_BLOCK (BLOCK_SIZE / INODE_SIZE)      // TODO: add a compile-time check that ((BLOCK_SIZE % INODE_SIZE) == 0                               
#define MAX_PATH 1024
#define MAGIC_NUMBER 0x12345678

// === FILE TYPES ===
#define INODE_TYPE_FREE      0
#define INODE_TYPE_FILE      1
#define INODE_TYPE_DIRECTORY 2

// === COMMON ERROR CODES ===
#define SUCCESS           0
#define ERROR_GENERIC    -1
#define ERROR_NOT_FOUND  -2
#define ERROR_EXISTS     -3
#define ERROR_NO_SPACE   -4
#define ERROR_INVALID    -5
#define ERROR_IO         -6
#define ERROR_PERMISSION -7

// === SHARED STRUCTURES ===

// Filesystem Superblock (104B)
struct superblock {
    uint32_t magic_number;         // magic number for FS validation

    uint32_t total_blocks;         // total number of blocks on the disk
    uint32_t total_inodes;         // total number of inodes on the disk

    uint32_t free_blocks;          // free blocks count
    uint32_t free_inodes;          // free inodes count

    uint32_t block_size;           // size of a block in bytes
    uint32_t inode_size;           // size of an inode in bytes

    uint32_t block_bitmap_start;   // first block of data block bitmap (usually 1)
    uint32_t block_bitmap_blocks;  // number of blocks for data block bitmap
    uint32_t inode_bitmap_start;   // first block of inode bitmap (usually 2)
    uint32_t inode_bitmap_blocks;  // number of blocks for inode bitmap
    uint32_t inode_table_start;    // first block of inode table (usually 3)
    uint32_t inode_table_blocks;   // number of blocks for inode table
    uint32_t first_data_block;     // first block of data area

    time_t   created_time;         // filesystem creation timestamp
    time_t   last_mount_time;      // last mount timestamp
    uint32_t mount_count;

    uint32_t reserved[8];          // reserved for future expansions
} __attribute__((packed));


// Inode (128B --> 1 block contains exactly 4 inodes)
struct inode {
    uint8_t  type;              // type: file/directory/free
    uint8_t  pad1;              // padding or future flags
    uint32_t size;              // size in bytes
    uint32_t blocks_used;       // number of blocks needed by the file
    uint32_t direct[12];        // direct pointers to data blocks
    uint32_t indirect;          // indirect pointer

    time_t   created_time;      // inode creation timestamp
    time_t   modified_time;     // inode modification timestamp
    time_t   accessed_time;     // last access timestamp

    uint16_t permissions;       // (rwxrwxrwx) bitmask
    uint16_t links_count;       // number of hard links
    uint16_t pad2;              // padding

    uint32_t reserved[9];       // more padding
} __attribute__((packed));

// Directory entry (256B per entry --> 1 block contains exactly 2 dentries)
/*  
    NOTE: in real filesystems, directory entries do not have a fixed length. 
    A rec_len field is used to see after how many bytes the next entry starts. 
    Its presence also allows efficient file deletion.

*/
struct dentry {
    uint32_t inode_num;             // inode number
    uint8_t  name_len;              // filename length 
    uint8_t  file_type;         
    char     name[MAX_FILENAME];    // filename string
} __attribute__((packed));

// === USEFUL MACROS ===
#define ALIGN_TO_BLOCK(size) (((size) + BLOCK_SIZE - 1) & ~(BLOCK_SIZE - 1))    // rounds size up to the next multiple of 512
#define BLOCKS_NEEDED(size) (ALIGN_TO_BLOCK(size) / BLOCK_SIZE)                 // calculates how many 512‑byte blocks are needed to contain size bytes
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

// === USEFUL FUNCTION PROTOTYPES ===
const char* error_string(int error_code);
void print_timestamp(time_t timestamp);
bool is_valid_filename(const char* filename);