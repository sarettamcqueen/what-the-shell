#include "fs.h"
#include "fs_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/**
 * Loads bitmaps from disk into memory.
 */
int load_bitmaps(filesystem_t* fs) {
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
int save_bitmaps(filesystem_t* fs) {
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

    uint32_t allocated_blocks = 0;

    // add entries to root directory's data block
    res = dentry_add(disk, root_inode_num, &dot_dentry, temp_fs.block_bitmap, &allocated_blocks);
    if (res != SUCCESS) { status = res; goto cleanup_inode; }
    sb.free_blocks -= allocated_blocks;

    allocated_blocks = 0;
    res = dentry_add(disk, root_inode_num, &dotdot_dentry, temp_fs.block_bitmap, &allocated_blocks);
    if (res != SUCCESS) { status = res; goto cleanup_inode; }
    sb.free_blocks -= allocated_blocks;

    // read root inode from disk since it's been modified by previous dentry_add
    res = inode_read(disk, root_inode_num, &root_inode);
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
    disk_detach(fs->disk); // detach before freeing fs
    free(fs);

    if (status == SUCCESS) {
        printf("Filesystem unmounted successfully.\n");
    } else {
        printf("Filesystem unmount failed (error %d).\n", status);
    }

    return status;
}