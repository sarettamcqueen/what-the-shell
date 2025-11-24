#include "fs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// === PRIVATE HELPERS ===

/**
 * Validates that a given inode is a directory.
 * Helper to reduce code duplication.
 */
static int validate_parent_directory(disk_t disk, uint32_t inode_num) {
    struct inode inode;
    if (inode_read(disk, inode_num, &inode) != SUCCESS) {
        return ERROR_IO;
    }
    if (inode.type != INODE_TYPE_DIRECTORY) {
        return ERROR_INVALID;
    }
    return SUCCESS;
}

/**
 * Reads data from an inode's data blocks.
 * Handles direct and indirect blocks.
 */
static int read_inode_data(filesystem_t* fs, const struct inode* inode,
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
static int write_inode_data(filesystem_t* fs, struct inode* inode, uint32_t inode_num,
                            uint32_t offset, const void* buffer, size_t size, size_t* bytes_written) {
    if (!fs || !inode || !buffer || !bytes_written) {
        return ERROR_INVALID;
    }

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

        // determine which block to write
        if (start_block_idx < 12) {
            // direct block
            block_num = inode->direct[start_block_idx];
            block_num_ptr = &inode->direct[start_block_idx];
        } else {
            // indirect block
            uint32_t indirect_idx = start_block_idx - 12;
            
            if (indirect_idx >= BLOCK_SIZE / sizeof(uint32_t)) {
                return ERROR_NO_SPACE;
            }

            if (inode->indirect == 0) {
                // allocate indirect block
                int new_block = bitmap_find_first_free(fs->block_bitmap);
                if (new_block < 0) {
                    return ERROR_NO_SPACE;
                }
                bitmap_set(fs->block_bitmap, new_block);
                inode->indirect = new_block;
                inode->blocks_used++;
                inode_modified = true;

                // initialize indirect block with zeros
                memset(indirect_buffer, 0, BLOCK_SIZE);
                if (disk_write_block(fs->disk, new_block, indirect_buffer) != DISK_SUCCESS) {
                    bitmap_clear(fs->block_bitmap, new_block);
                    return ERROR_IO;
                }
            }

            // read indirect block
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
                return ERROR_NO_SPACE;
            }
            bitmap_set(fs->block_bitmap, new_block);
            block_num = new_block;
            *block_num_ptr = new_block;
            inode->blocks_used++;
            inode_modified = true;

            // Initialize with zeros
            memset(block_buffer, 0, BLOCK_SIZE);

            if (needs_indirect_write) {
                if (disk_write_block(fs->disk, inode->indirect, indirect_buffer) != DISK_SUCCESS) {
                    bitmap_clear(fs->block_bitmap, new_block);
                    return ERROR_IO;
                }
            }
        } else {
            // read existing block if partial write
            if (start_offset != 0 || remaining < BLOCK_SIZE) {
                if (disk_read_block(fs->disk, block_num, block_buffer) != DISK_SUCCESS) {
                    return ERROR_IO;
                }
            }
        }

        // write data to block buffer
        uint32_t chunk = (remaining < BLOCK_SIZE - start_offset) ? remaining : (BLOCK_SIZE - start_offset);
        memcpy(block_buffer + start_offset, buf_ptr, chunk);

        // write block to disk
        if (disk_write_block(fs->disk, block_num, block_buffer) != DISK_SUCCESS) {
            return ERROR_IO;
        }

        buf_ptr += chunk;
        remaining -= chunk;
        start_block_idx++;
        start_offset = 0;
    }

    // update inode size if needed
    uint32_t end_pos = offset + size;
    if (end_pos > inode->size) {
        inode->size = end_pos;
        inode_modified = true;
    }

    // update modification time
    inode->modified_time = time(NULL);
    inode_modified = true;

    // write inode back to disk
    if (inode_modified) {
        if (inode_write(fs->disk, inode_num, inode) != SUCCESS) {
            return ERROR_IO;
        }
    }

    *bytes_written = size;
    return SUCCESS;
}

/**
 * Loads bitmaps from disk into memory.
 */
static int load_bitmaps(filesystem_t* fs) {
    // calculate bitmap sizes
    size_t block_bitmap_bits = fs->sb.total_blocks;
    size_t inode_bitmap_bits = fs->sb.total_inodes;

    // create bitmaps
    fs->block_bitmap = bitmap_create(block_bitmap_bits);
    fs->inode_bitmap = bitmap_create(inode_bitmap_bits);

    if (!fs->block_bitmap || !fs->inode_bitmap) {
        bitmap_destroy(&fs->block_bitmap);
        bitmap_destroy(&fs->inode_bitmap);
        return ERROR_GENERIC;
    }

    // read block bitmap from disk
    for (uint32_t i = 0; i < fs->sb.block_bitmap_blocks; i++) {
        char buffer[BLOCK_SIZE];
        if (disk_read_block(fs->disk, fs->sb.block_bitmap_start + i, buffer) != DISK_SUCCESS) {
            bitmap_destroy(&fs->block_bitmap);
            bitmap_destroy(&fs->inode_bitmap);
            return ERROR_IO;
        }
        
        size_t bytes_to_copy = BLOCK_SIZE;
        size_t offset = i * BLOCK_SIZE;
        if (offset + bytes_to_copy > fs->block_bitmap->size_bytes) {
            bytes_to_copy = fs->block_bitmap->size_bytes - offset;
        }
        memcpy(fs->block_bitmap->data + offset, buffer, bytes_to_copy);
    }

    // read inode bitmap from disk
    for (uint32_t i = 0; i < fs->sb.inode_bitmap_blocks; i++) {
        char buffer[BLOCK_SIZE];
        if (disk_read_block(fs->disk, fs->sb.inode_bitmap_start + i, buffer) != DISK_SUCCESS) {
            bitmap_destroy(&fs->block_bitmap);
            bitmap_destroy(&fs->inode_bitmap);
            return ERROR_IO;
        }
        
        size_t bytes_to_copy = BLOCK_SIZE;
        size_t offset = i * BLOCK_SIZE;
        if (offset + bytes_to_copy > fs->inode_bitmap->size_bytes) {
            bytes_to_copy = fs->inode_bitmap->size_bytes - offset;
        }
        memcpy(fs->inode_bitmap->data + offset, buffer, bytes_to_copy);
    }

    return SUCCESS;
}

/**
 * Saves bitmaps from memory to disk.
 */
static int save_bitmaps(filesystem_t* fs) {
    if (!fs || !fs->block_bitmap || !fs->inode_bitmap) {
        return ERROR_INVALID;
    }

    // write block bitmap to disk
    for (uint32_t i = 0; i < fs->sb.block_bitmap_blocks; i++) {
        char buffer[BLOCK_SIZE];
        memset(buffer, 0, BLOCK_SIZE);
        
        size_t bytes_to_copy = BLOCK_SIZE;
        size_t offset = i * BLOCK_SIZE;
        if (offset + bytes_to_copy > fs->block_bitmap->size_bytes) {
            bytes_to_copy = fs->block_bitmap->size_bytes - offset;
        }
        memcpy(buffer, fs->block_bitmap->data + offset, bytes_to_copy);
        
        if (disk_write_block(fs->disk, fs->sb.block_bitmap_start + i, buffer) != DISK_SUCCESS) {
            return ERROR_IO;
        }
    }

    // write inode bitmap to disk
    for (uint32_t i = 0; i < fs->sb.inode_bitmap_blocks; i++) {
        char buffer[BLOCK_SIZE];
        memset(buffer, 0, BLOCK_SIZE);
        
        size_t bytes_to_copy = BLOCK_SIZE;
        size_t offset = i * BLOCK_SIZE;
        if (offset + bytes_to_copy > fs->inode_bitmap->size_bytes) {
            bytes_to_copy = fs->inode_bitmap->size_bytes - offset;
        }
        memcpy(buffer, fs->inode_bitmap->data + offset, bytes_to_copy);
        
        if (disk_write_block(fs->disk, fs->sb.inode_bitmap_start + i, buffer) != DISK_SUCCESS) {
            return ERROR_IO;
        }
    }

    return SUCCESS;
}

/**
 * Resolves a path to an inode number.
 * Supports absolute and relative paths.
 */
static int fs_path_to_inode(filesystem_t* fs, const char* path, uint32_t* out_inode_num) {
if (!fs || !path || !out_inode_num) {
        return ERROR_INVALID;
    }

    // validate path
    if (!path_is_valid(path)) {
        return ERROR_INVALID;
    }

    // normalize path to handle ".", "..", and redundant separators
    char* normalized = path_normalize(path);
    if (!normalized) {
        return ERROR_INVALID;
    }

    // handle root
    if (path_is_root(normalized)) {
        *out_inode_num = ROOT_INODE_NUM;
        free(normalized);
        return SUCCESS;
    }

    // parse path
    struct path_components* pc = path_parse(normalized);
    free(normalized);
    
    if (!pc) {
        return ERROR_INVALID;
    }

    // start from root or current directory
    uint32_t current_inode = pc->is_absolute ? ROOT_INODE_NUM : fs->current_dir_inode;

    // traverse components
    for (int i = 0; i < pc->count; i++) {
        const char* component = pc->components[i];

        // skip "." (should already be handled by normalize, but just in case)
        if (strcmp(component, ".") == 0) {
            continue;
        }

        // handle ".."
        if (strcmp(component, "..") == 0) {
            struct dentry parent_entry;
            if (dentry_find(fs->disk, current_inode, "..", &parent_entry, NULL) == SUCCESS) {
                current_inode = parent_entry.inode_num;
            } else {
                // root has no parent
                if (current_inode != ROOT_INODE_NUM) {
                    path_components_free(pc);
                    return ERROR_NOT_FOUND;
                }
            }
            continue;
        }

        // regular lookup
        struct dentry entry;
        if (dentry_find(fs->disk, current_inode, component, &entry, NULL) != SUCCESS) {
            path_components_free(pc);
            return ERROR_NOT_FOUND;
        }

        current_inode = entry.inode_num;
    }

    path_components_free(pc);
    *out_inode_num = current_inode;
    return SUCCESS;
}

/** 
 * Normalizes, splits, and validates parent + name.
 * Helper to reduce code duplication in fs_create and fs_mkdir.
 */
static int fs_prepare_create(filesystem_t* fs, const char* path,
                             char* parent_path, char* name,
                             uint32_t* parent_inode_num) {
    if (!fs || !path || !parent_path || !name || !parent_inode_num)
        return ERROR_INVALID;

    if (!path_is_valid(path))
        return ERROR_INVALID;

    char* normalized = path_normalize(path);
    if (!normalized) return ERROR_INVALID;

    if (path_split(normalized, parent_path, name) != SUCCESS) {
        free(normalized);
        return ERROR_INVALID;
    }
    free(normalized);

    if (!filename_is_valid(name)) return ERROR_INVALID;

    // resolve parent directory
    int res = fs_path_to_inode(fs, parent_path, parent_inode_num);
    if (res != SUCCESS) return res;

    if (validate_parent_directory(fs->disk, *parent_inode_num) != SUCCESS)
        return ERROR_INVALID;

    // check if name already exists
    struct dentry tmp;
    res = (dentry_find(fs->disk, *parent_inode_num, name, &tmp, NULL));
    if (res == SUCCESS) {
        return ERROR_EXISTS;
    } else if (res != ERROR_NOT_FOUND) {
        return res;
    }

    return SUCCESS;
}

// === PUBLIC API ===

int fs_format(disk_t disk, size_t total_blocks, size_t total_inodes) {
    int status = SUCCESS;
    if (!disk) {
        return ERROR_INVALID;
    }

    struct superblock sb;
    int res = superblock_init(disk, &sb, total_blocks, total_inodes);
    if (res != SUCCESS) return res;

    res = superblock_write(disk, &sb);
    if (res != SUCCESS) return ERROR_IO;

    // create temporary filesystem to manage bitmaps in-memory
    filesystem_t temp_fs;
    temp_fs.disk = disk;
    temp_fs.sb   = sb;
    temp_fs.block_bitmap = NULL;
    temp_fs.inode_bitmap = NULL;

    // load empty bitmaps from disk to memory
    res = load_bitmaps(&temp_fs);
    if (res != SUCCESS) return ERROR_IO;

    // mark reserved blocks in block bitmap:
    //  - blocks holding the block bitmap
    //  - blocks holding the inode bitmap
    //  - blocks holding the inode table
    //  - superblock block
    for (uint32_t i = 0; i < sb.block_bitmap_blocks; i++)
        bitmap_set(temp_fs.block_bitmap, sb.block_bitmap_start + i);

    for (uint32_t i = 0; i < sb.inode_bitmap_blocks; i++)
        bitmap_set(temp_fs.block_bitmap, sb.inode_bitmap_start + i);

    for (uint32_t i = 0; i < sb.inode_table_blocks; i++)
        bitmap_set(temp_fs.block_bitmap, sb.inode_table_start + i);

    bitmap_set(temp_fs.block_bitmap, SUPERBLOCK_BLOCK_NUM);

    // mark reserved inodes in inode bitmap
    bitmap_set(temp_fs.inode_bitmap, INVALID_INODE_NUM);

    // allocate root directory inode
    struct inode root_inode;
    uint32_t root_inode_num = 999999;  // sentinel value

    res = inode_alloc(disk, temp_fs.inode_bitmap,
                      INODE_TYPE_DIRECTORY, 0755,
                      &root_inode, &root_inode_num);

    if (res != SUCCESS) {
        status = res;
        goto cleanup_bitmaps;
    }

    sb.free_inodes--;   // root allocated

    if (root_inode_num != ROOT_INODE_NUM) {
        status = ERROR_GENERIC;
        goto cleanup_inode;
    }

    // create dentry for "." (self)
    struct dentry dot_dentry;
    if (dentry_create(".", root_inode_num, INODE_TYPE_DIRECTORY, &dot_dentry) != SUCCESS) {
        status = ERROR_INVALID;
        goto cleanup_inode;
    }
    // create dentry for ".." (parent, which is self for root)
    struct dentry dotdot_dentry;
    if (dentry_create("..", root_inode_num, INODE_TYPE_DIRECTORY, &dotdot_dentry) != SUCCESS) {
        status = ERROR_INVALID;
        goto cleanup_inode;
    }

    // add entries to root directory's data block
    res = dentry_add(disk, root_inode_num, &dot_dentry, temp_fs.block_bitmap);
    if (res != SUCCESS) { status = res; goto cleanup_inode; }

    res = dentry_add(disk, root_inode_num, &dotdot_dentry, temp_fs.block_bitmap);
    if (res != SUCCESS) { status = res; goto cleanup_inode; }

    // set links_count for root and write inode back
    root_inode.links_count = 2;
    res = inode_write(disk, root_inode_num, &root_inode);
    if (res != SUCCESS) { status = res; goto cleanup_inode; }

    // save bitmaps to disk
    res = save_bitmaps(&temp_fs);
    if (res != SUCCESS) { status = res; goto cleanup_inode; }

    // write superblock to disk
    res = superblock_write(disk, &sb);
    if (res != SUCCESS) { status = res; goto cleanup_inode; }

    // success: cleanup memory and return
    printf("Filesystem formatted successfully.\n");
    printf("Root inode %u created and initialized with '.' and '..'\n", root_inode_num);
    goto cleanup_bitmaps;

cleanup_inode:
    printf("[CLEANUP] cleanup_inode triggered\n");
    {
        uint32_t freed_blocks = 0;
        inode_free(disk, temp_fs.inode_bitmap, temp_fs.block_bitmap,
                   root_inode_num, &freed_blocks);
        sb.free_inodes++;
        sb.free_blocks += freed_blocks;
        superblock_write(disk, &sb);
    }

cleanup_bitmaps:
    if (temp_fs.block_bitmap) bitmap_destroy(&temp_fs.block_bitmap);
    if (temp_fs.inode_bitmap) bitmap_destroy(&temp_fs.inode_bitmap);

    return status;
}

int fs_mount(disk_t disk, filesystem_t** out_fs) {
    if (!disk || !out_fs) {
        return ERROR_INVALID;
    }

    filesystem_t* fs = (filesystem_t*)malloc(sizeof(filesystem_t));
    if (!fs) {
        return ERROR_GENERIC;
    }

    fs->disk = disk;
    fs->is_mounted = false;
    fs->block_bitmap = NULL;
    fs->inode_bitmap = NULL;

    // load superblock
    if (superblock_read(disk, &fs->sb) != SUCCESS) {
        free(fs);
        return ERROR_IO;
    }

    // validate superblock
    if (!superblock_is_valid(&fs->sb)) {
        free(fs);
        return ERROR_INVALID;
    }

    // load bitmaps
    if (load_bitmaps(fs) != SUCCESS) {
        free(fs);
        return ERROR_IO;
    }

    // set current directory to root
    fs->current_dir_inode = ROOT_INODE_NUM;
    fs->is_mounted = true;

    // update mount info
    fs->sb.last_mount_time = time(NULL);
    fs->sb.mount_count++;
    
    // release memory if mount fails
    if (superblock_write(disk, &fs->sb) != SUCCESS) {
        bitmap_destroy(&fs->block_bitmap);
        bitmap_destroy(&fs->inode_bitmap);
        free(fs);
        return ERROR_IO;
    }

    *out_fs = fs;
    printf("Filesystem mounted successfully.\n");
    superblock_print(&fs->sb);

    return SUCCESS;
}

int fs_unmount(filesystem_t* fs) {
    int status = SUCCESS;

    if (!fs) {
        return ERROR_INVALID;
    }

    // save bitmap
    if (save_bitmaps(fs) != SUCCESS) {
        status = ERROR_IO;
        goto cleanup;
    }

    // save superblock
    if (superblock_write(fs->disk, &fs->sb) != SUCCESS) {
        status = ERROR_IO;
        goto cleanup;
    }

cleanup:
    // cleanup is always executed
    if (fs->block_bitmap) {
        bitmap_destroy(&fs->block_bitmap);
    }
    if (fs->inode_bitmap) {
        bitmap_destroy(&fs->inode_bitmap);
    }

    fs->is_mounted = false;
    free(fs);

    if (status == SUCCESS) {
        printf("Filesystem unmounted successfully.\n");
    } else {
        printf("Filesystem unmount failed (error %d).\n", status);
    }

    return status;
}

int fs_cd(filesystem_t* fs, const char* path) {
    if (!fs || !path) {
        return ERROR_INVALID;
    }

    // validate path
    if (!path_is_valid(path)) {
        return ERROR_INVALID;
    }

    uint32_t inode_num;
    int res = fs_path_to_inode(fs, path, &inode_num);
    if (res != SUCCESS) return res;

    // check that inode_num is a directory
    struct inode inode;
    if (inode_read(fs->disk, inode_num, &inode) != SUCCESS) {
        return ERROR_IO;
    }

    if (inode.type != INODE_TYPE_DIRECTORY) {
        return ERROR_INVALID;
    }

    // update current directory
    fs->current_dir_inode = inode_num;
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
        for (int i = 0; i < 12 && inode.direct[i] != 0; i++) {
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

            for (uint32_t i = 0; i < BLOCK_SIZE / sizeof(uint32_t) && block_ptrs[i] != 0; i++) {
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

    // check permissions
    if (!(file->flags & (FS_O_WRONLY | FS_O_RDWR))) {
        return ERROR_PERMISSION;
    }

    int res = write_inode_data(file->fs, &file->inode, file->inode_num,
                                  file->offset, buffer, size, bytes_written);
    if (res == SUCCESS) {
        file->offset += *bytes_written;
        
        // save bitmaps if blocks were allocated
        save_bitmaps(file->fs);
        superblock_write(file->fs->disk, &file->fs->sb);
    }

    return res;
}

int fs_seek(open_file_t* file, uint32_t offset) {
    if (!file) {
        return ERROR_INVALID;
    }
    if (offset > file->inode.size) offset = file->inode.size;

    file->offset = offset;
    return SUCCESS;
}

int fs_create(filesystem_t* fs, const char* path, uint16_t permissions) {

    printf("[DEBUG fs_create] path=%s\n", path);

    int status = SUCCESS;
    char parent_path[MAX_PATH];
    char filename[MAX_FILENAME];
    uint32_t parent_inode_num;

    int res = fs_prepare_create(fs, path, parent_path, filename, &parent_inode_num);
    if (res != SUCCESS) return res;

    // allocate inode
    struct inode new_inode;
    uint32_t new_inode_num;
    if (inode_alloc(fs->disk, fs->inode_bitmap, INODE_TYPE_FILE, permissions,
                    &new_inode, &new_inode_num) != SUCCESS) {
        return ERROR_NO_SPACE;
    }

    // create dentry
    struct dentry new_dentry;
    if (dentry_create(filename, new_inode_num, INODE_TYPE_FILE, &new_dentry) != SUCCESS) {
        status = ERROR_INVALID;
        goto cleanup_inode;
    }

    // add to parent directory
    if (dentry_add(fs->disk, parent_inode_num, &new_dentry, fs->block_bitmap) != SUCCESS) {
        status = ERROR_IO;
        goto cleanup_inode;
    }

    // update inode
    new_inode.modified_time = time(NULL);
    new_inode.accessed_time = time(NULL);
    if (inode_write(fs->disk, new_inode_num, &new_inode) != SUCCESS) {
        status = ERROR_IO;
        goto cleanup_remove_parent_dentry;
    }

    // update superblock and save
    save_bitmaps(fs);
    fs->sb.free_inodes--;
    superblock_write(fs->disk, &fs->sb);

    return SUCCESS;

    cleanup_remove_parent_dentry:
        dentry_remove(fs->disk, parent_inode_num, filename);

    cleanup_inode:
        inode_free(fs->disk, fs->inode_bitmap, fs->block_bitmap, new_inode_num, NULL);

    return status;
}

int fs_unlink(filesystem_t* fs, const char* path) {
    if (!fs || !path) {
        return ERROR_INVALID;
    }

    // validate path
    if (!path_is_valid(path)) {
        return ERROR_INVALID;
    }

    // resolve path
    uint32_t inode_num;
    int res = fs_path_to_inode(fs, path, &inode_num);
    if (res != SUCCESS) return res;

    // read inode
    struct inode inode;
    if (inode_read(fs->disk, inode_num, &inode) != SUCCESS) {
        return ERROR_IO;
    }

    // can't unlink directories
    if (inode.type == INODE_TYPE_DIRECTORY) {
        return ERROR_INVALID;
    }

    // decrement link count
    inode.links_count--;

    // free resources if links_count reaches 0
    if (inode.links_count == 0) {
        // free direct blocks
        for (int i = 0; i < 12 && inode.direct[i] != 0; i++) {
            bitmap_clear(fs->block_bitmap, inode.direct[i]);
            fs->sb.free_blocks++;
        }

        // free indirect blocks
        if (inode.indirect != 0) {
            char indirect_buffer[BLOCK_SIZE];
            if (disk_read_block(fs->disk, inode.indirect, indirect_buffer) != DISK_SUCCESS) {
                return ERROR_IO;
            }
            uint32_t* block_ptrs = (uint32_t*)indirect_buffer;

            for (uint32_t i = 0; i < BLOCK_SIZE / sizeof(uint32_t) && block_ptrs[i] != 0; i++) {
                bitmap_clear(fs->block_bitmap, block_ptrs[i]);
                fs->sb.free_blocks++;
            }

            bitmap_clear(fs->block_bitmap, inode.indirect);
            fs->sb.free_blocks++;
        }

        // free inode
        uint32_t freed_blocks = 0;
        inode_free(fs->disk, fs->inode_bitmap, fs->block_bitmap, inode_num, &freed_blocks);
        fs->sb.free_inodes++;
        fs->sb.free_blocks += freed_blocks;
    } else {
        // update inode with decremented link count
        if (inode_write(fs->disk, inode_num, &inode) != SUCCESS) {
            return ERROR_IO;
        }
    }

    char* normalized = path_normalize(path);
    if (!normalized) {
        return ERROR_INVALID;
    }

    // remove dentry from parent
    char parent_path[MAX_PATH];
    char filename[MAX_FILENAME];
    if (path_split(normalized, parent_path, filename) != SUCCESS) {
        free(normalized);
        return ERROR_INVALID;
    }
    free(normalized);

    uint32_t parent_inode_num;
    res = fs_path_to_inode(fs, parent_path, &parent_inode_num);
    if (res != SUCCESS) return res;
    res = dentry_remove(fs->disk, parent_inode_num, filename);
    if (res != SUCCESS) return res;

    // save
    if (save_bitmaps(fs) != SUCCESS) return ERROR_IO;
    if (superblock_write(fs->disk, &fs->sb) != SUCCESS) return ERROR_IO;

    return SUCCESS;
}

int fs_mkdir(filesystem_t* fs, const char* path, uint16_t permissions) {
    int status = SUCCESS;
    char parent_path[MAX_PATH];
    char dirname[MAX_FILENAME];
    uint32_t parent_inode_num;

    int res = fs_prepare_create(fs, path, parent_path, dirname, &parent_inode_num);
    if (res != SUCCESS) return res;

    // allocate inode for directory
    struct inode new_dir_inode;
    uint32_t new_dir_inode_num;
    if (inode_alloc(fs->disk, fs->inode_bitmap, INODE_TYPE_DIRECTORY, permissions,
                   &new_dir_inode, &new_dir_inode_num) != SUCCESS) {
        return ERROR_NO_SPACE;
    }

    // create dentry in parent directory
    struct dentry new_dentry;
    if (dentry_create(dirname, new_dir_inode_num, INODE_TYPE_DIRECTORY, &new_dentry) != SUCCESS) {
        status = ERROR_INVALID;
        goto cleanup_inode;
    }

    if (dentry_add(fs->disk, parent_inode_num, &new_dentry, fs->block_bitmap) != SUCCESS) {
        status = ERROR_IO;
        goto cleanup_inode;
    }

    // add "." and ".." entries
    struct dentry dot, dotdot;

    if (dentry_create(".", new_dir_inode_num, INODE_TYPE_DIRECTORY, &dot) != SUCCESS) {
        status = ERROR_INVALID;
        goto cleanup_remove_parent_dentry;
    }

    if (dentry_add(fs->disk, new_dir_inode_num, &dot, fs->block_bitmap) != SUCCESS) {
        status = ERROR_IO;
        goto cleanup_remove_parent_dentry;
    }

    if (dentry_create("..", parent_inode_num, INODE_TYPE_DIRECTORY, &dotdot) != SUCCESS) {
        status = ERROR_INVALID;
        goto cleanup_remove_parent_dentry;
    }

    if (dentry_add(fs->disk, new_dir_inode_num, &dotdot, fs->block_bitmap) != SUCCESS) {
        status = ERROR_IO;
        goto cleanup_remove_parent_dentry;
    }

    // update new directory link count (for "." reference)
    if (inode_read(fs->disk, new_dir_inode_num, &new_dir_inode) != SUCCESS) {
        status = ERROR_IO;
        goto cleanup_remove_parent_dentry;
    }
    new_dir_inode.links_count = 2;
    new_dir_inode.modified_time = time(NULL);
    if (inode_write(fs->disk, new_dir_inode_num, &new_dir_inode) != SUCCESS) {
        status = ERROR_IO;
        goto cleanup_remove_parent_dentry;
    }

    // update parent link count (for ".." reference)
    struct inode parent_inode;
    if (inode_read(fs->disk, parent_inode_num, &parent_inode) != SUCCESS) {
        status = ERROR_IO;
        goto cleanup_remove_parent_dentry;
    }
    parent_inode.links_count++;
    parent_inode.modified_time = time(NULL);
    if (inode_write(fs->disk, parent_inode_num, &parent_inode) != SUCCESS) {
        status = ERROR_IO;
        goto cleanup_remove_parent_dentry;
    }

    // update superblock
    fs->sb.free_inodes--;
    if (superblock_write(fs->disk, &fs->sb) != SUCCESS) {
        status = ERROR_IO;
        goto cleanup_revert_parent_link;
    }
    save_bitmaps(fs);

    return SUCCESS;

    cleanup_revert_parent_link:
        // revert parent link count increment
        if (inode_read(fs->disk, parent_inode_num, &parent_inode) == SUCCESS) {
            parent_inode.links_count--;
            inode_write(fs->disk, parent_inode_num, &parent_inode);
        }

    cleanup_remove_parent_dentry:
        // remove dentry from parent directory (rollback)
        dentry_remove(fs->disk, parent_inode_num, dirname);
        
    cleanup_inode: {
        // free inode and its blocks
        uint32_t freed_blocks = 0;
        inode_free(fs->disk, fs->inode_bitmap, fs->block_bitmap, new_dir_inode_num, &freed_blocks);
        fs->sb.free_inodes++;
        fs->sb.free_blocks += freed_blocks;
        superblock_write(fs->disk, &fs->sb);
        save_bitmaps(fs);
    }
        
    return status;
}

int fs_rmdir(filesystem_t* fs, const char* path) {
    if (!fs || !path) {
        return ERROR_INVALID;
    }

    // validate path
    if (!path_is_valid(path)) {
        return ERROR_INVALID;
    }

    // can't remove root
    if (path_is_root(path)) {
        return ERROR_INVALID;
    }

    // resolve
    uint32_t target_inode_num;
    int res = fs_path_to_inode(fs, path, &target_inode_num);
    if (res != SUCCESS) return res;

    // read inode
    struct inode target_inode;
    if (inode_read(fs->disk, target_inode_num, &target_inode) != SUCCESS) {
        return ERROR_IO;
    }

    // must be directory
    if (target_inode.type != INODE_TYPE_DIRECTORY) {
        return ERROR_INVALID;
    }

    // check if empty (only . and .. should exist)
    struct dentry* entries;
    uint32_t count;
    if (dentry_list(fs->disk, target_inode_num, &entries, &count) != SUCCESS) {
        return ERROR_IO;
    }

    uint32_t non_special = 0;
    for (uint32_t i = 0; i < count; i++) {
        if (strcmp(entries[i].name, ".") != 0 && strcmp(entries[i].name, "..") != 0) {
            non_special++;
        }
    }
    free(entries);

    if (non_special > 0) {
        return ERROR_GENERIC;  // not empty
    }

    // free data blocks (direct)
    for (int i = 0; i < 12 && target_inode.direct[i] != 0; i++) {
        bitmap_clear(fs->block_bitmap, target_inode.direct[i]);
        fs->sb.free_blocks++;
    }

    // free indirect blocks
    if (target_inode.indirect != 0) {
        char indirect_buffer[BLOCK_SIZE];
        if (disk_read_block(fs->disk, target_inode.indirect, indirect_buffer) == DISK_SUCCESS) {
            uint32_t* block_ptrs = (uint32_t*)indirect_buffer;
            for (uint32_t i = 0; i < BLOCK_SIZE / sizeof(uint32_t); i++) {
                if (block_ptrs[i] != 0) {
                    bitmap_clear(fs->block_bitmap, block_ptrs[i]);
                    fs->sb.free_blocks++;
                }
            }
        }
        bitmap_clear(fs->block_bitmap, target_inode.indirect);
        fs->sb.free_blocks++;
    }

    char* normalized = path_normalize(path);
    if (!normalized) {
        return ERROR_INVALID;
    }

    char parent_path[MAX_PATH];
    char dirname[MAX_FILENAME];
    if (path_split(normalized, parent_path, dirname) != SUCCESS) {
        free(normalized);
        return ERROR_INVALID;
    }
    free(normalized);

    // free inode
    uint32_t freed_blocks = 0;
    inode_free(fs->disk, fs->inode_bitmap, fs->block_bitmap, target_inode_num, &freed_blocks);
    fs->sb.free_inodes++;
    fs->sb.free_blocks += freed_blocks;

    // remove from parent
    uint32_t parent_inode_num;
    res = fs_path_to_inode(fs, parent_path, &parent_inode_num);
    if (res != SUCCESS) return res;
    dentry_remove(fs->disk, parent_inode_num, dirname);

    // decrement parent link count
    struct inode parent_inode;
    if (inode_read(fs->disk, parent_inode_num, &parent_inode) != SUCCESS) {
        return ERROR_IO;
    }
    parent_inode.links_count--;
    parent_inode.modified_time = time(NULL);
    if (inode_write(fs->disk, parent_inode_num, &parent_inode) != SUCCESS) {
        return ERROR_IO;
    }

    // save
    save_bitmaps(fs);
    superblock_write(fs->disk, &fs->sb);

    return SUCCESS;
}

int fs_link(filesystem_t* fs, const char* existing_path, const char* new_path) {
    if (!fs || !existing_path || !new_path) {
        return ERROR_INVALID;
    }

    // validate paths
    if (!path_is_valid(existing_path) || !path_is_valid(new_path)) {
        return ERROR_INVALID;
    }

    // resolve existing file
    uint32_t existing_inode_num;
    int res = fs_path_to_inode(fs, existing_path, &existing_inode_num);
    if (res != SUCCESS) return res;

    // read inode
    struct inode inode;
    if (inode_read(fs->disk, existing_inode_num, &inode) != SUCCESS) {
        return ERROR_IO;
    }

    // can't hard link directories
    if (inode.type == INODE_TYPE_DIRECTORY) {
        return ERROR_INVALID;
    }

    char* normalized = path_normalize(new_path);
    if (!normalized) {
        return ERROR_INVALID;
    }

    // split new path
    char parent_path[MAX_PATH];
    char filename[MAX_FILENAME];
    if (path_split(normalized, parent_path, filename) != SUCCESS) {
        free(normalized);
        return ERROR_INVALID;
    }
    free(normalized);

    // validate filename
    if (!filename_is_valid(filename)) {
        return ERROR_INVALID;
    }

    // resolve parent
    uint32_t parent_inode_num;
    res = fs_path_to_inode(fs, parent_path, &parent_inode_num);
    if (res != SUCCESS) return res;

    // validate parent is a directory
    if (validate_parent_directory(fs->disk, parent_inode_num) != SUCCESS) {
        return ERROR_INVALID;
    }

    // check if new path exists
    if (dentry_find(fs->disk, parent_inode_num, filename, NULL, NULL) == SUCCESS) {
        return ERROR_EXISTS;
    }

    // create new dentry pointing to same inode
    struct dentry new_dentry;
    dentry_create(filename, existing_inode_num, INODE_TYPE_FILE, &new_dentry);

    if (dentry_add(fs->disk, parent_inode_num, &new_dentry, fs->block_bitmap) != SUCCESS) {
        return ERROR_IO;
    }

    // increment link count
    inode.links_count++;
    inode.modified_time = time(NULL);
    inode_write(fs->disk, existing_inode_num, &inode);

    save_bitmaps(fs);

    return SUCCESS;
}

int fs_list(filesystem_t* fs, const char* path, struct dentry** out_entries, uint32_t* out_count) {
    if (!fs || !path || !out_entries || !out_count) {
        return ERROR_INVALID;
    }

    // validate path
    if (!path_is_valid(path)) {
        return ERROR_INVALID;
    }

    // resolve
    uint32_t inode_num;
    int res = fs_path_to_inode(fs, path, &inode_num);
    if (res != SUCCESS) return res;

    // read inode
    struct inode inode;
    if (inode_read(fs->disk, inode_num, &inode) != SUCCESS) {
        return ERROR_IO;
    }

    // must be directory
    if (inode.type != INODE_TYPE_DIRECTORY) {
        return ERROR_INVALID;
    }

    return dentry_list(fs->disk, inode_num, out_entries, out_count);
}

int fs_inode_to_path(filesystem_t* fs, uint32_t inode_num, char* out_path, size_t out_size) {
    if (!fs || !out_path || out_size == 0)
        return ERROR_INVALID;

    if (inode_num == ROOT_INODE_NUM) {
        // root is trivial
        snprintf(out_path, out_size, "/");
        return SUCCESS;
    }

    /*  
        NOTE: we store intermediate path components here while climbing up the directory tree.
        The maximum depth is set to 64: it is unlikely for a valid filesystem path to contain
        more than 64 nested directories in normal usage.
        If deeper directory hierarchies are required, this value can be safely increased.
        Using a fixed-size array avoids dynamic allocations and simplifies error handling.
    */
    char components[64][MAX_FILENAME];
    int depth = 0;

    uint32_t current = inode_num;

    while (current != ROOT_INODE_NUM) {
        // read parent
        struct dentry parent;
        if (dentry_find(fs->disk, current, "..", &parent, NULL) != SUCCESS)
            return ERROR_IO;

        uint32_t parent_inode = parent.inode_num;

        // find name of "current" inside parent directory
        uint32_t count = 0;
        struct dentry* list = NULL;

        if (dentry_list(fs->disk, parent_inode, &list, &count) != SUCCESS)
            return ERROR_IO;

        int found = 0;
        for (uint32_t i = 0; i < count; i++) {
            // skip . and ..
            if (strcmp(list[i].name, ".") == 0 || strcmp(list[i].name, "..") == 0)
                continue;

            if (list[i].inode_num == current) {
                strncpy(components[depth], list[i].name, MAX_FILENAME);
                components[depth][MAX_FILENAME-1] = '\0';
                found = 1;
                break;
            }
        }

        free(list);

        if (!found)
            return ERROR_NOT_FOUND;

        depth++;
        current = parent_inode;
    }

    // rebuild full absolute path
    size_t offset = 0;
    out_path[0] = '\0';

    for (int i = depth - 1; i >= 0; i--) {
        size_t len = strlen(components[i]);
        if (offset + len + 2 >= out_size)
            return ERROR_NO_SPACE;

        out_path[offset++] = '/';
        memcpy(&out_path[offset], components[i], len);
        offset += len;
        out_path[offset] = '\0';
    }

    if (offset == 0) {
        // should never happen, but fallback
        snprintf(out_path, out_size, "/");
    }

    return SUCCESS;
}

int fs_stat(filesystem_t* fs, const char* path, struct inode* out_inode) {
    if (!fs || !path || !out_inode) {
        return ERROR_INVALID;
    }

    // validate path
    if (!path_is_valid(path)) {
        return ERROR_INVALID;
    }

    uint32_t inode_num;
    int res = fs_path_to_inode(fs, path, &inode_num);
    if (res != SUCCESS) return res;

    return inode_read(fs->disk, inode_num, out_inode);
}

void fs_print_stats(filesystem_t* fs) {
    if (!fs) {
        return;
    }

    printf("\n=== Filesystem Statistics ===\n");
    superblock_print(&fs->sb);
    printf("Mounted: %s\n", fs->is_mounted ? "Yes" : "No");
    printf("Current directory inode: %u\n", fs->current_dir_inode);
}