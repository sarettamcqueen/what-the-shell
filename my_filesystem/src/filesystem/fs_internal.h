#pragma once

#include "fs.h"

int load_bitmaps(filesystem_t* fs);
int save_bitmaps(filesystem_t* fs);

int fs_path_to_inode(filesystem_t* fs, const char* path, uint32_t* out_inode_num);

/**
 * Reconstructs the absolute filesystem path of a file/directory from its inode number.
 * 
 * @param fs The filesystem
 * @param inode_num Inode number of the directory whose path is requested
 * @param out_path Output buffer where the absolute path will be written
 * @param out_size Size of the output buffer in bytes
 * @return SUCCESS or error code
 */
int fs_inode_to_path(filesystem_t* fs, uint32_t inode_num, char* out_path, size_t out_size);

int validate_parent_directory(disk_t disk, uint32_t inode_num);
int fs_prepare_create(filesystem_t* fs, const char* path,
                      char* parent_path, char* name,
                      uint32_t* parent_inode_num);

int read_inode_data(filesystem_t* fs, const struct inode* inode,
                    uint32_t offset, void* buffer, size_t size, size_t* bytes_read);
int write_inode_data(filesystem_t* fs, struct inode* inode, uint32_t inode_num,
                     uint32_t offset, const void* buffer, size_t size, size_t* bytes_written);