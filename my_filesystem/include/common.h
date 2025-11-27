/**
   Common header file.
 
   This file defines constants, data structures, macros, and utility functions
   shared across the various modules that make up the filesystem.
  
   Contents:
    - Global constants:
        - Sizes and counts
        - Magic number for filesystem validation
        - Path length limits
    - Reserved inodes and blocks:
        - Invalid inode marker (inode 0)
        - Root directory inode (inode 1)
        - Superblock location (block 0)
    - Inode type definitions (free, file, directory)
    - Standard error codes
    - Core filesystem structures:
        - superblock: global filesystem metadata and disk layout
        - inode: descriptor of a file/directory with pointers to data blocks
        - dentry: directory entry mapping a filename to an inode number
    - Utility macros:
        - Block alignment and size calculations
        - Min/max operations
    - Function prototypes:
        - Error code to string conversion
        - Timestamp printing
  
   All structures are marked with `__attribute__((packed))` to prevent
   automatic padding and ensure that their sizes align precisely with
   the expected block layout of the filesystem.
   
   Design notes:
    - Inode 0 is reserved as an invalid marker (never allocated)
    - Inode 1 is the root directory (unlike Unix standard convention where inode 2 is root)
    - Block 0 always contains the superblock
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <time.h>
#include <fcntl.h>
#include "config.h"

// === SIZES AND COUNTS ===
#define INODE_SIZE         128                               
#define INODES_PER_BLOCK   (BLOCK_SIZE / INODE_SIZE)
#define DENTRY_SIZE        256
#define DENTRIES_PER_BLOCK (BLOCK_SIZE / DENTRY_SIZE)                              
#define MAX_PATH           1024
#define MAGIC_NUMBER       0x12345678
#define BYTES_PER_INODE    4096  // 1 inode every 4KB of disk space
#define MIN_INODES         64    // for very small disks
/*
 * NOTE: MIN_INODES set to 64 means that the disk size must be at least 10240 bytes (= 20 blocks)
 * since the inode table requires 16 blocks, and the superblock, the block bitmap, the inode bitmap, and the root inode
 * each require 1 block. This filesystem does not work on disks smaller than 10 KiB!!!
 */

// === RESERVED INODES ===
#define INVALID_INODE_NUM 0     // reserved, never used
#define ROOT_INODE_NUM    1     // root directory

// === FILE TYPES ===
#define INODE_TYPE_FREE      0
#define INODE_TYPE_FILE      1
#define INODE_TYPE_DIRECTORY 2

// === RESERVED BLOCKS ===
#define SUPERBLOCK_BLOCK_NUM 0   // superblock location (fixed)

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

// Filesystem Superblock (108B)
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

// === COMPILE-TIME CHECKS ===
_Static_assert(sizeof(struct inode) == INODE_SIZE, 
               "struct inode must be exactly INODE_SIZE bytes");
_Static_assert((BLOCK_SIZE % INODE_SIZE) == 0, 
               "BLOCK_SIZE must be divisible by INODE_SIZE");
_Static_assert(sizeof(struct dentry) == DENTRY_SIZE, 
               "struct dentry must be exactly DENTRY_SIZE bytes");
_Static_assert((BLOCK_SIZE % DENTRY_SIZE) == 0, 
               "BLOCK_SIZE must be divisible by DENTRY_SIZE");

// === USEFUL MACROS ===
#define ALIGN_TO_BLOCK(size) (((size) + BLOCK_SIZE - 1) & ~(BLOCK_SIZE - 1))    // rounds size up to the next multiple of 512
#define BLOCKS_NEEDED(size) (ALIGN_TO_BLOCK(size) / BLOCK_SIZE)                 // calculates how many 512‑byte blocks are needed to contain size bytes
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

// === USEFUL FUNCTION PROTOTYPES ===
const char* error_string(int error_code);
void print_timestamp(time_t timestamp);