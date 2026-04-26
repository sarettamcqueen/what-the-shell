#include "fs.h"
#include "fs_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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
    fs->sb.free_inodes--;

    // create dentry
    struct dentry new_dentry;
    if (dentry_create(filename, new_inode_num, INODE_TYPE_FILE, &new_dentry) != SUCCESS) {
        status = ERROR_INVALID;
        goto cleanup_inode;
    }

    uint32_t allocated_blocks = 0;
    // add to parent directory
    if (dentry_add(fs->disk, parent_inode_num, &new_dentry, fs->block_bitmap, &allocated_blocks) != SUCCESS) {
        status = ERROR_IO;
        goto cleanup_inode;
    }
    fs->sb.free_blocks -= allocated_blocks;

    // update inode
    new_inode.modified_time = time(NULL);
    new_inode.accessed_time = time(NULL);
    if (inode_write(fs->disk, new_inode_num, &new_inode) != SUCCESS) {
        status = ERROR_IO;
        goto cleanup_remove_parent_dentry;
    }

    // update superblock and save
    save_bitmaps(fs);
    superblock_write(fs->disk, &fs->sb);

    return SUCCESS;

    cleanup_remove_parent_dentry:
        dentry_remove(fs->disk, fs->block_bitmap, &fs->sb, parent_inode_num, filename);
        fs->sb.free_blocks += allocated_blocks;

    cleanup_inode:
        uint32_t freed_blocks = 0;
        inode_free(fs->disk, fs->inode_bitmap, fs->block_bitmap, new_inode_num, &freed_blocks);
        fs->sb.free_inodes++;
        fs->sb.free_blocks += freed_blocks;
        save_bitmaps(fs);
        superblock_write(fs->disk, &fs->sb);

    return status;
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
    dentry_create(filename, existing_inode_num, inode.type, &new_dentry);

    uint32_t allocated_blocks = 0;

    if (dentry_add(fs->disk, parent_inode_num, &new_dentry, fs->block_bitmap, &allocated_blocks) != SUCCESS) {
        return ERROR_IO;
    }
    fs->sb.free_blocks -= allocated_blocks;

    // increment link count
    inode.links_count++;
    inode.modified_time = time(NULL);
    if (inode_write(fs->disk, existing_inode_num, &inode) != SUCCESS) {
        // rollback the dentry
        dentry_remove(fs->disk, fs->block_bitmap, &fs->sb, parent_inode_num, filename);
        fs->sb.free_blocks += allocated_blocks;
        return ERROR_IO;
    }

    save_bitmaps(fs);
    if (superblock_write(fs->disk, &fs->sb) != SUCCESS) {
        return ERROR_IO;
    }

    return SUCCESS;
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

    res = dentry_remove(fs->disk, fs->block_bitmap, &fs->sb, parent_inode_num, filename);
    if (res != SUCCESS) return res;

    // decrement link count
    inode.links_count--;

    // free resources if links_count reaches 0
    if (inode.links_count == 0) {
        uint32_t freed_blocks = 0;

        if (inode_free(fs->disk, fs->inode_bitmap, fs->block_bitmap, inode_num, &freed_blocks) != SUCCESS) {
            return ERROR_IO;
        }

        fs->sb.free_inodes++;
        fs->sb.free_blocks += freed_blocks;
    } else {
        // update inode with decremented link count
        if (inode_write(fs->disk, inode_num, &inode) != SUCCESS) {
            return ERROR_IO;
        }
    }

    // save
    if (save_bitmaps(fs) != SUCCESS) return ERROR_IO;
    if (superblock_write(fs->disk, &fs->sb) != SUCCESS) return ERROR_IO;

    return SUCCESS;
}