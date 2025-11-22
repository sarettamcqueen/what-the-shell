#pragma once

#include "common.h"
#include "disk.h"
#include "superblock.h"
#include "inode.h"
#include "dentry.h"
#include "bitmap.h"
#include "path.h"
#include <stdbool.h>

// === FILESYSTEM CONTEXT ===

/**
 * Represents a mounted filesystem instance.
 * Holds all the metadata and state required to perform filesystem operations.
 */
typedef struct filesystem {
    disk_t disk;                      // disk emulator handle
    struct superblock sb;             // in-memory copy of superblock
    struct bitmap* block_bitmap;      // in-memory bitmap for data blocks
    struct bitmap* inode_bitmap;      // in-memory bitmap for inodes
    bool is_mounted;                  // mount status
    uint32_t current_dir_inode;       // current working directory (for shell)
} filesystem_t;

// === OPEN FILE DESCRIPTOR ===

/**
 * Represents an open file with a cursor position for read/write operations.
 */
typedef struct open_file {
    uint32_t inode_num;               // inode number
    struct inode inode;               // in-memory copy of the inode
    uint32_t offset;                  // current read/write position
    uint32_t flags;                   // open flags (read/write/append)
    filesystem_t* fs;                 // reference to filesystem
} open_file_t;

// === OPEN FLAGS ===
#define FS_O_RDONLY    0x01           // open for reading only
#define FS_O_WRONLY    0x02           // open for writing only
#define FS_O_RDWR      0x03           // open for reading and writing
#define FS_O_CREAT     0x08           // create file if it doesn't exist
#define FS_O_APPEND    0x10           // append writes to end of file
#define FS_O_TRUNC     0x20           // truncate file upon opening

// === FILESYSTEM LIFECYCLE ===

/**
 * Formats a disk with a new filesystem.
 * Initializes superblock, bitmaps, inode table, and root directory.
 * 
 * @param disk The disk to format
 * @param total_blocks Total number of blocks on the disk
 * @param total_inodes Total number of inodes to allocate
 * @return SUCCESS or error code
 */
int fs_format(disk_t disk, size_t total_blocks, size_t total_inodes);

/**
 * Mounts an existing filesystem from disk.
 * Loads superblock and bitmaps into memory.
 * 
 * @param disk The disk containing the filesystem
 * @param out_fs Pointer to receive the filesystem handle
 * @return SUCCESS or error code
 */
int fs_mount(disk_t disk, filesystem_t** out_fs);

/**
 * Unmounts a filesystem, writes back metadata, and frees all resources.
 * 
 * @param fs The filesystem to unmount
 * @return SUCCESS or error code
 */
int fs_unmount(filesystem_t* fs);

// === DIRECTORY NAVIGATION ===

/**
 * Changes the current working directory of the mounted filesystem.
 * 
 * @param fs Pointer to the mounted filesystem instance
 * @param path Path to the new directory (absolute or relative)
 * @return SUCCESS or error code
 */
int fs_cd(filesystem_t* fs, const char* path);

// === FILE OPERATIONS ===

/**
 * Opens a file and returns a file descriptor.
 * 
 * @param fs The filesystem
 * @param path Path to the file
 * @param flags Open flags (O_RDONLY, O_WRONLY, O_CREAT, etc.)
 * @param out_file Pointer to receive the open file descriptor
 * @return SUCCESS or error code
 */
int fs_open(filesystem_t* fs, const char* path, uint32_t flags, open_file_t** out_file);

/**
 * Closes an open file descriptor.
 * 
 * @param file The file to close
 * @return SUCCESS or error code
 */
int fs_close(open_file_t* file);

/**
 * Reads data from an open file.
 * 
 * @param file The open file
 * @param buffer Buffer to receive data
 * @param size Number of bytes to read
 * @param bytes_read Pointer to receive actual bytes read
 * @return SUCCESS or error code
 */
int fs_read(open_file_t* file, void* buffer, size_t size, size_t* bytes_read);

/**
 * Writes data to an open file.
 * 
 * @param file The open file
 * @param buffer Data to write
 * @param size Number of bytes to write
 * @param bytes_written Pointer to receive actual bytes written
 * @return SUCCESS or error code
 */
int fs_write(open_file_t* file, const void* buffer, size_t size, size_t* bytes_written);

/**
 * Moves the file cursor to a specific position.
 * 
 * @param file The open file
 * @param offset New cursor position
 * @return SUCCESS or error code
 */
int fs_seek(open_file_t* file, uint32_t offset);

// === FILE/DIRECTORY CREATION AND DELETION ===

/**
 * Creates a new file.
 * 
 * @param fs The filesystem
 * @param path Path for the new file
 * @param permissions Permission bits
 * @return SUCCESS or error code
 */
int fs_create(filesystem_t* fs, const char* path, uint16_t permissions);

/**
 * Deletes a file (unlink).
 * Decrements hard link count; removes file only if count reaches 0.
 * 
 * @param fs The filesystem
 * @param path Path to the file
 * @return SUCCESS or error code
 */
int fs_unlink(filesystem_t* fs, const char* path);

/**
 * Creates a new directory.
 * 
 * @param fs The filesystem
 * @param path Path for the new directory
 * @param permissions Permission bits
 * @return SUCCESS or error code
 */
int fs_mkdir(filesystem_t* fs, const char* path, uint16_t permissions);

/**
 * Removes an empty directory.
 * 
 * @param fs The filesystem
 * @param path Path to the directory
 * @return SUCCESS or error code
 */
int fs_rmdir(filesystem_t* fs, const char* path);

// === HARD LINK SUPPORT ===

/**
 * Creates a hard link to an existing file.
 * Both paths will point to the same inode.
 * 
 * @param fs The filesystem
 * @param existing_path Path to existing file
 * @param new_path Path for the new hard link
 * @return SUCCESS or error code
 */
int fs_link(filesystem_t* fs, const char* existing_path, const char* new_path);

// === DIRECTORY LISTING ===

/**
 * Lists all entries in a directory.
 * 
 * @param fs The filesystem
 * @param path Path to the directory
 * @param out_entries Pointer to receive array of dentries
 * @param out_count Pointer to receive entry count
 * @return SUCCESS or error code
 */
int fs_list(filesystem_t* fs, const char* path, struct dentry** out_entries, uint32_t* out_count);

// === FILE/DIRECTORY INFORMATION ===

/**
 * Retrieves information about a file or directory.
 * 
 * @param fs The filesystem
 * @param path Path to the file/directory
 * @param out_inode Pointer to receive inode data
 * @return SUCCESS or error code
 */
int fs_stat(filesystem_t* fs, const char* path, struct inode* out_inode);

// === UTILITIES ===

/**
 * Prints filesystem statistics.
 * 
 * @param fs The filesystem
 */
void fs_print_stats(filesystem_t* fs);
