/**
   Virtual File System (VFS) header file.
 
   This file defines the high-level abstraction layer that manages multiple
   mounted filesystems. It provides a unified directory tree, routing requests
   to the appropriate physical filesystem based on the mount points.
  
   Contents:
    - Global constants for VFS
    - VFS core structures:
        - vfs_mount: tracks a single mounted filesystem and its prefix.
        - vfs: the global state holding all mounts and the unified CWD.
    - Function prototypes divided by category:
        - Initialization & Mount Lifecycle
        - Path Resolution & Navigation
        - File I/O (delegated through open_file_t)
        - Directory Operations
        - File Manipulation & Cross-Disk Operations
        - Statistics
 */

#pragma once

#include "common.h"
#include "fs.h"
#include <stdbool.h>
#include <stddef.h>

// === VFS CONSTANTS ===
#define MAX_MOUNTS 8  // max number of filesystems that can be mounted simultaneously

// === VFS STRUCTURES ===

/**
 * Represents a single mount point within the Virtual File System.
 */
typedef struct vfs_mount {
    filesystem_t* fs;                 // pointer to the underlying filesystem instance
    size_t path_len;                  // cached length of the mount path for faster prefix matching
    bool is_active;                   // true if this slot is currently in use
    char mount_path[MAX_PATH];        // absolute path where the fs is mounted (e.g., "/mnt/usb")
} vfs_mount_t;

/**
 * Represents the global Virtual File System state.
 * Manages the mount table and the unified current working directory.
 */
typedef struct vfs {
    uint32_t count;                   // number of currently active mounts
    vfs_mount_t mounts[MAX_MOUNTS];   // table of all mounted filesystems
    char cwd[MAX_PATH];               // global unified Current Working Directory
} vfs_t;


// === INITIALIZATION & MOUNT LIFECYCLE ===

/**
 * Initializes the VFS structure, clearing the mount table and setting CWD to "/".
 */
void vfs_init(vfs_t* vfs);

/**
 * Unmounts all active filesystems in reverse order and frees their resources.
 * Must be called on shell exit to avoid memory leaks and ensure all data is
 * flushed to disk.
 */
void vfs_destroy(vfs_t* vfs);

/**
 * Formats a disk image with a new filesystem of the given size.
 * Aligns size_bytes to the nearest block boundary before formatting.
 * Fails with ERROR_BUSY if the disk is currently mounted in the VFS.
 */
int vfs_format(vfs_t* vfs, const char* filename, uint64_t size_bytes);


/**
 * Mounts a disk image at the specified path in the VFS.
 * Resolves mount_path to absolute before registering it in the mount table,
 * so relative paths and "." are handled correctly at lookup time.
 * The first mount must always be on "/". Fails with ERROR_BUSY if the same
 * disk is already mounted, ERROR_EXISTS if the mount point is already in use,
 * and ERROR_NOT_FOUND if the mount point directory does not exist.
 */
int vfs_mount(vfs_t* vfs, const char* filename, const char* mount_path);


/**
 * Unmounts the filesystem at the specified path, flushing its state to disk.
 * Resolves mount_path to absolute before searching the mount table, so
 * relative paths and "." work correctly. Returns ERROR_BUSY if the path is
 * "/" or if the current working directory is inside the filesystem being
 * unmounted.
 */
int vfs_unmount(vfs_t* vfs, const char* mount_path);


// === PATH RESOLUTION & NAVIGATION ===

/**
 * Resolves a global VFS path to its underlying filesystem and local path.
 * Optionally reconstructs the normalized absolute VFS path.
 */
int vfs_resolve_path(vfs_t* vfs, const char* path, filesystem_t** out_fs, 
                     char* out_local_path, size_t local_size,
                     char* out_abs_path, size_t abs_size);

/**
 * Retrieves the unified absolute path of the current working directory.
 */
int vfs_getcwd(vfs_t* vfs, char* buffer, size_t size);

/**
 * Changes the unified current working directory across the VFS.
 */
int vfs_cd(vfs_t* vfs, const char* path);


// === FILE I/O ===

/**
 * Opens a file resolving its VFS path, returning a file descriptor.
 */
int vfs_open(vfs_t* vfs, const char* path, uint32_t flags, open_file_t** out_file);

/**
 * Closes an open file descriptor.
 */
int vfs_close(open_file_t* file);

/**
 * Reads data from an open file.
 */
int vfs_read(open_file_t* file, void* buffer, size_t size, size_t* bytes_read);

/**
 * Writes data to an open file.
 */
int vfs_write(open_file_t* file, const void* buffer, size_t size, size_t* bytes_written);

/**
 * Moves the file cursor to a specific position.
 */
int vfs_seek(open_file_t* file, uint32_t offset);


// === DIRECTORY OPERATIONS ===

/**
 * Creates a new directory at the resolved VFS path.
 */
int vfs_mkdir(vfs_t* vfs, const char* path, uint16_t permissions);

/**
 * Removes an empty directory at the resolved VFS path.
 */
int vfs_rmdir(vfs_t* vfs, const char* path);

/**
 * Lists all entries in a directory at the resolved VFS path.
 */
int vfs_ls(vfs_t* vfs, const char* path, struct dentry** out_entries, uint32_t* out_count);


// === FILE MANIPULATION & CROSS-DISK OPS ===

/**
 * Creates a new file at the resolved VFS path.
 */
int vfs_create(vfs_t* vfs, const char* path, uint16_t permissions);

/**
 * Deletes a file (unlink) at the resolved VFS path.
 */
int vfs_rm(vfs_t* vfs, const char* path);

/**
 * Creates a hard link between two paths (must reside on the same underlying filesystem).
 */
int vfs_link(vfs_t* vfs, const char* existing_path, const char* new_path);

int vfs_chmod(vfs_t* vfs, const char* path, uint16_t permissions);

/**
 * Deep copies a file from a source path to a destination path (supports cross-disk).
 */
int vfs_cp(vfs_t* vfs, const char* src_path, const char* dst_path);

/**
 * Moves or renames a file/directory. Uses fast rename if on the same disk,
 * or deep copy + delete if cross-disk.
 */
int vfs_mv(vfs_t* vfs, const char* src_path, const char* dst_path);


// === METADATA & STATISTICS ===

/**
 * Retrieves metadata for a file or directory across the VFS.
 */
int vfs_stat(vfs_t* vfs, const char* path, struct inode* out_inode, 
             uint32_t* out_inode_num, char* out_abs_path, size_t abs_size);

/**
 * Prints the space usage and statistics for all actively mounted filesystems.
 */
void vfs_df(vfs_t* vfs);