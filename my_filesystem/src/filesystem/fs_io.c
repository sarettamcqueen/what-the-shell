#include "fs.h"
#include "fs_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
/**
 * Reads data from an inode's data blocks.
 * Handles direct and indirect blocks.
 */
int read_inode_data(filesystem_t* fs, const struct inode* inode,
                           uint32_t offset, void* buffer, size_t size, size_t* bytes_read) {
    if (!fs || !inode || !buffer || !bytes_read) {
        return ERROR_INVALID;
    }

    // calculate how much data is available
    uint32_t available = (offset < inode->size) ? (inode->size - offset) : 0;
    size_t to_read = (size < available) ? size : available;

    if (to_read == 0) {
        *bytes_read = 0;
        return SUCCESS;
    }

    uint32_t start_block_idx = offset / BLOCK_SIZE;
    uint32_t start_offset = offset % BLOCK_SIZE;
    uint32_t remaining = to_read;
    uint8_t* buf_ptr = (uint8_t*)buffer;

    char block_buffer[BLOCK_SIZE];

    while (remaining > 0) {
        uint32_t block_num = 0;

        // determine which block to read
        if (start_block_idx < 12) {
            // direct block
            block_num = inode->direct[start_block_idx];
        } else {
            // indirect block
            if (inode->indirect == 0) {
                return ERROR_INVALID;
            }

            char indirect_buffer[BLOCK_SIZE];
            if (disk_read_block(fs->disk, inode->indirect, indirect_buffer) != DISK_SUCCESS) {
                return ERROR_IO;
            }

            uint32_t* block_ptrs = (uint32_t*)indirect_buffer;
            uint32_t indirect_idx = start_block_idx - 12;
            
            if (indirect_idx >= BLOCK_SIZE / sizeof(uint32_t)) {
                return ERROR_INVALID;
            }
            
            block_num = block_ptrs[indirect_idx];
        }

        if (block_num == 0) {
            // sparse file (hole) - return zeros
            uint32_t chunk = (remaining < BLOCK_SIZE - start_offset) ? remaining : (BLOCK_SIZE - start_offset);
            memset(buf_ptr, 0, chunk);
            buf_ptr += chunk;
            remaining -= chunk;
        } else {
            // read actual block
            if (disk_read_block(fs->disk, block_num, block_buffer) != DISK_SUCCESS) {
                return ERROR_IO;
            }

            uint32_t chunk = (remaining < BLOCK_SIZE - start_offset) ? remaining : (BLOCK_SIZE - start_offset);
            memcpy(buf_ptr, block_buffer + start_offset, chunk);
            buf_ptr += chunk;
            remaining -= chunk;
        }

        start_block_idx++;
        start_offset = 0;
    }

    *bytes_read = to_read;
    return SUCCESS;
}

/**
 * Writes data to an inode's data blocks.
 * Allocates new blocks as needed.
 */
int write_inode_data(filesystem_t* fs, struct inode* inode, uint32_t inode_num,
                            uint32_t offset, const void* buffer, size_t size, size_t* bytes_written) {
    if (!fs || !inode || !buffer || !bytes_written) {
        return ERROR_INVALID;
    }

    // guard against offset + size overflow
    if (size > UINT32_MAX - offset) {
        return ERROR_INVALID;
    }

    *bytes_written = 0;

    uint32_t start_block_idx = offset / BLOCK_SIZE;
    uint32_t start_offset = offset % BLOCK_SIZE;
    uint32_t remaining = size;
    const uint8_t* buf_ptr = (const uint8_t*)buffer;

    char block_buffer[BLOCK_SIZE];
    bool inode_modified = false;

    while (remaining > 0) {
        uint32_t block_num = 0;
        uint32_t* block_num_ptr = NULL;
        bool needs_indirect_write = false;
        char indirect_buffer[BLOCK_SIZE];

        bool allocated_indirect_block = false;
        uint32_t allocated_indirect_block_num = 0;

        bool allocated_data_block = false;
        uint32_t allocated_data_block_num = 0;

        // determine which block to write
        if (start_block_idx < 12) {
            // direct block — do not take address of packed member
            block_num = inode->direct[start_block_idx];
            block_num_ptr = NULL;
        } else {
            // indirect block
            uint32_t indirect_idx = start_block_idx - 12;

            if (indirect_idx >= BLOCK_SIZE / sizeof(uint32_t)) {
                return ERROR_NO_SPACE;
            }

            if (inode->indirect == 0) {
                int new_block = bitmap_find_first_free(fs->block_bitmap);
                if (new_block < 0) {
                    return ERROR_NO_SPACE;
                }

                if (bitmap_set(fs->block_bitmap, new_block) != SUCCESS) {
                    return ERROR_GENERIC;
                }

                fs->sb.free_blocks--;
                inode->indirect = new_block;
                inode->blocks_used++;
                inode_modified = true;

                allocated_indirect_block = true;
                allocated_indirect_block_num = new_block;

                memset(indirect_buffer, 0, BLOCK_SIZE);
                if (disk_write_block(fs->disk, new_block, indirect_buffer) != DISK_SUCCESS) {
                    bitmap_clear(fs->block_bitmap, new_block);
                    fs->sb.free_blocks++;
                    inode->indirect = 0;
                    inode->blocks_used--;
                    return ERROR_IO;
                }
            }

            if (disk_read_block(fs->disk, inode->indirect, indirect_buffer) != DISK_SUCCESS) {
                return ERROR_IO;
            }

            uint32_t* block_ptrs = (uint32_t*)indirect_buffer;
            block_num = block_ptrs[indirect_idx];
            block_num_ptr = &block_ptrs[indirect_idx];
            needs_indirect_write = true;
        }

        // allocate block if needed
        if (block_num == 0) {
            int new_block = bitmap_find_first_free(fs->block_bitmap);
            if (new_block < 0) {
                if (allocated_indirect_block) {
                    bitmap_clear(fs->block_bitmap, allocated_indirect_block_num);
                    fs->sb.free_blocks++;
                    inode->indirect = 0;
                    inode->blocks_used--;
                }
                return ERROR_NO_SPACE;
            }

            if (bitmap_set(fs->block_bitmap, new_block) != SUCCESS) {
                if (allocated_indirect_block) {
                    bitmap_clear(fs->block_bitmap, allocated_indirect_block_num);
                    fs->sb.free_blocks++;
                    inode->indirect = 0;
                    inode->blocks_used--;
                }
                return ERROR_GENERIC;
            }

            fs->sb.free_blocks--;
            block_num = new_block;
            inode->blocks_used++;
            inode_modified = true;

            if (needs_indirect_write) {
                *block_num_ptr = new_block;
            } else {
                inode->direct[start_block_idx] = new_block;
            }

            allocated_data_block = true;
            allocated_data_block_num = new_block;

            memset(block_buffer, 0, BLOCK_SIZE);

            if (needs_indirect_write) {
                if (disk_write_block(fs->disk, inode->indirect, indirect_buffer) != DISK_SUCCESS) {
                    bitmap_clear(fs->block_bitmap, new_block);
                    fs->sb.free_blocks++;
                    *block_num_ptr = 0;
                    inode->blocks_used--;

                    if (allocated_indirect_block) {
                        bitmap_clear(fs->block_bitmap, allocated_indirect_block_num);
                        fs->sb.free_blocks++;
                        inode->indirect = 0;
                        inode->blocks_used--;
                    }

                    return ERROR_IO;
                }
            }
        } else {
            if (start_offset != 0 || remaining < BLOCK_SIZE) {
                if (disk_read_block(fs->disk, block_num, block_buffer) != DISK_SUCCESS) {
                    return ERROR_IO;
                }
            } else {
                memset(block_buffer, 0, BLOCK_SIZE);
            }
        }

        // write data to block buffer
        uint32_t chunk = (remaining < BLOCK_SIZE - start_offset) ? remaining : (BLOCK_SIZE - start_offset);
        memcpy(block_buffer + start_offset, buf_ptr, chunk);

        // write block to disk
        if (disk_write_block(fs->disk, block_num, block_buffer) != DISK_SUCCESS) {
            if (allocated_data_block) {
                bitmap_clear(fs->block_bitmap, allocated_data_block_num);
                fs->sb.free_blocks++;
                if (needs_indirect_write) {
                    *block_num_ptr = 0;
                    // sync indirect block back to disk after rollback
                    disk_write_block(fs->disk, inode->indirect, indirect_buffer);
                } else {
                    inode->direct[start_block_idx] = 0;
                }
                inode->blocks_used--;
            }

            if (allocated_indirect_block) {
                bitmap_clear(fs->block_bitmap, allocated_indirect_block_num);
                fs->sb.free_blocks++;
                inode->indirect = 0;
                inode->blocks_used--;
            }

            return ERROR_IO;
        }

        buf_ptr += chunk;
        remaining -= chunk;
        *bytes_written += chunk;
        start_block_idx++;
        start_offset = 0;
    }

    // update inode size if needed
    uint32_t end_pos = offset + size;
    if (end_pos > inode->size) {
        inode->size = end_pos;
        inode_modified = true;
    }

    if(inode_modified) {
        // update modification time and write inode back to disk
        inode->modified_time = time(NULL);
        if (inode_write(fs->disk, inode_num, inode) != SUCCESS) {
            return ERROR_IO;
        }
    }

    return SUCCESS;
}

int fs_open(filesystem_t* fs, const char* path, uint32_t flags, open_file_t** out_file) {
    if (!fs || !path || !out_file) {
        return ERROR_INVALID;
    }

    // validate path
    if (!path_is_valid(path)) {
        return ERROR_INVALID;
    }

    uint32_t inode_num;
    int res = fs_path_to_inode(fs, path, &inode_num);

    // create file if doesn't exist and FS_O_CREAT is set
    if (res == ERROR_NOT_FOUND && (flags & FS_O_CREAT)) {
        res = fs_create(fs, path, 0644);
        if (res != SUCCESS) {
            return res;
        }
        res = fs_path_to_inode(fs, path, &inode_num);
    }

    if (res != SUCCESS) {
        return res;
    }

    // read inode
    struct inode inode;
    if (inode_read(fs->disk, inode_num, &inode) != SUCCESS) {
        return ERROR_IO;
    }

    // must be a file
    if (inode.type != INODE_TYPE_FILE) {
        return ERROR_INVALID;
    }

    // truncate if requested
    if (flags & FS_O_TRUNC) {
        // free all data blocks
        for (int i = 0; i < 12; i++) {
            if (inode.direct[i] == 0) continue;
            bitmap_clear(fs->block_bitmap, inode.direct[i]);
            fs->sb.free_blocks++;
            inode.direct[i] = 0;
        }

        if (inode.indirect != 0) {
            char indirect_buffer[BLOCK_SIZE];
            if (disk_read_block(fs->disk, inode.indirect, indirect_buffer) != DISK_SUCCESS) {
                return ERROR_IO;
            }
            uint32_t* block_ptrs = (uint32_t*)indirect_buffer;

            for (uint32_t i = 0; i < BLOCK_SIZE / sizeof(uint32_t); i++) {
                if (block_ptrs[i] == 0) continue;
                bitmap_clear(fs->block_bitmap, block_ptrs[i]);
                fs->sb.free_blocks++;
            }

            bitmap_clear(fs->block_bitmap, inode.indirect);
            fs->sb.free_blocks++;
            inode.indirect = 0;
        }

        inode.size = 0;
        inode.blocks_used = 0;
        inode.modified_time = time(NULL);
        if (inode_write(fs->disk, inode_num, &inode) != SUCCESS) {
            return ERROR_IO;
        }

        save_bitmaps(fs);
        superblock_write(fs->disk, &fs->sb);
    }

    // create file descriptor
    open_file_t* file = (open_file_t*)malloc(sizeof(open_file_t));
    if (!file) {
        return ERROR_GENERIC;
    }

    file->inode_num = inode_num;
    file->inode = inode;
    file->flags = flags;
    file->fs = fs;

    // set offset
    if (flags & FS_O_APPEND) {
        file->offset = inode.size;
    } else {
        file->offset = 0;
    }

    *out_file = file;
    return SUCCESS;
}

int fs_close(open_file_t* file) {
    if (!file) {
        return ERROR_INVALID;
    }

    free(file);
    return SUCCESS;
}

int fs_read(open_file_t* file, void* buffer, size_t size, size_t* bytes_read) {
    if (!file || !buffer || !bytes_read) {
        return ERROR_INVALID;
    }

    // check permissions
    if (!(file->flags & (FS_O_RDONLY | FS_O_RDWR))) {
        return ERROR_PERMISSION;
    }

    int res = read_inode_data(file->fs, &file->inode, file->offset, buffer, size, bytes_read);
    if (res == SUCCESS) {
        file->offset += *bytes_read;

        // update access time
        file->inode.accessed_time = time(NULL);
        inode_write(file->fs->disk, file->inode_num, &file->inode);
    }

    return res;
}

int fs_write(open_file_t* file, const void* buffer, size_t size, size_t* bytes_written) {
    if (!file || !buffer || !bytes_written) {
        return ERROR_INVALID;
    }

    *bytes_written = 0;

    // check permissions
    if (!(file->flags & (FS_O_WRONLY | FS_O_RDWR))) {
        return ERROR_PERMISSION;
    }

    int res = write_inode_data(file->fs, &file->inode, file->inode_num,
                               file->offset, buffer, size, bytes_written);
    if (res != SUCCESS) {
        return res;
    }

    file->offset += *bytes_written;

    // persist updated metadata
    if (save_bitmaps(file->fs) != SUCCESS) {
        return ERROR_IO;
    }

    if (superblock_write(file->fs->disk, &file->fs->sb) != SUCCESS) {
        return ERROR_IO;
    }

    return SUCCESS;
}

int fs_seek(open_file_t* file, uint32_t offset) {
    if (!file) {
        return ERROR_INVALID;
    }
    if (offset > file->inode.size) offset = file->inode.size;

    file->offset = offset;
    return SUCCESS;
}