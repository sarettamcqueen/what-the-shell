#pragma once

#include "vfs.h"

// ==========================================
// FILESYSTEM LIFECYCLE
// ==========================================

/**
 * @brief Formats a new disk image with the custom filesystem.
 * Usage: format <diskname> <size_in_bytes>
 */
int cmd_format(vfs_t* vfs, int argc, char** argv);

/**
 * @brief Mounts a disk image to a specific path in the VFS.
 * Usage: mount <disk.img> <mount_path>
 */
int cmd_mount(vfs_t* vfs, int argc, char** argv);

/**
 * @brief Unmounts a filesystem from the specified VFS path.
 * Usage: unmount <mount_path>
 */
int cmd_unmount(vfs_t* vfs, int argc, char** argv);


// ==========================================
// DIRECTORIES
// ==========================================

/**
 * @brief Creates a new directory.
 * Usage: mkdir <dir>
 */
int cmd_mkdir(vfs_t* vfs, int argc, char** argv);

/**
 * @brief Removes an empty directory.
 * Usage: rmdir <dir>
 */
int cmd_rmdir(vfs_t* vfs, int argc, char** argv);

/**
 * @brief Changes the current working directory.
 * Usage: cd <path>
 */
int cmd_cd(vfs_t* vfs, int argc, char** argv);

/**
 * @brief Prints the absolute path of the current working directory.
 * Usage: pwd
 */
int cmd_pwd(vfs_t* vfs, int argc, char** argv);


// ==========================================
// FILES (CREATION & I/O)
// ==========================================

/**
 * @brief Creates an empty file.
 * Usage: touch <file>
 */
int cmd_touch(vfs_t* vfs, int argc, char** argv);

/**
 * @brief Deletes a file.
 * Usage: rm <file>
 */
int cmd_rm(vfs_t* vfs, int argc, char** argv);

/**
 * @brief Prints the contents of a file to standard output.
 * Usage: cat <file>
 */
int cmd_cat(vfs_t* vfs, int argc, char** argv);

/**
 * @brief Overwrites a file with the provided text.
 * Usage: write <file> "text"
 */
int cmd_write(vfs_t* vfs, int argc, char** argv);

/**
 * @brief Appends text to the end of an existing file.
 * Usage: append <file> "text"
 */
int cmd_append(vfs_t* vfs, int argc, char** argv);


// ==========================================
// FILES (MANIPULATION)
// ==========================================

/**
 * @brief Deep copies a file to a new destination (supports cross-disk).
 * Usage: cp <src> <dest>
 */
int cmd_cp(vfs_t* vfs, int argc, char** argv);

/**
 * @brief Moves or renames a file or directory.
 * Usage: mv <src> <dest>
 */
int cmd_mv(vfs_t* vfs, int argc, char** argv);

/**
 * @brief Changes the permissions of a file or directory.
 * Usage: chmod <octal_mode> <path>
 */
int cmd_chmod(vfs_t* vfs, int argc, char** argv);


// ==========================================
// LISTING & LINKS
// ==========================================

/**
 * @brief Lists the contents of a directory.
 * Usage: ls [path]
 */
int cmd_ls(vfs_t* vfs, int argc, char** argv);

/**
 * @brief Creates a hard link between two files on the same disk.
 * Usage: ln <src> <dest>
 */
int cmd_ln(vfs_t* vfs, int argc, char** argv);


// ==========================================
// METADATA & DIAGNOSTICS
// ==========================================

/**
 * @brief Displays detailed metadata (inode structure) of a file or directory.
 * Usage: stat <path>
 */
int cmd_stat(vfs_t* vfs, int argc, char** argv);

/**
 * @brief Displays space usage and statistics for all mounted filesystems.
 * Usage: df
 */
int cmd_df(vfs_t* vfs, int argc, char** argv);