#include "fs.h"
#include "fs_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/**
 * Validates that a given inode is a directory.
 * Helper to reduce code duplication.
 */
int validate_parent_directory(disk_t disk, uint32_t inode_num) {
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
 * Normalizes, splits, and validates parent + name.
 * Helper to reduce code duplication in fs_create and fs_mkdir.
 */
int fs_prepare_create(filesystem_t* fs, const char* path,
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
    fs->sb.free_inodes--;

    // create dentry in parent directory
    struct dentry new_dentry;
    if (dentry_create(dirname, new_dir_inode_num, INODE_TYPE_DIRECTORY, &new_dentry) != SUCCESS) {
        status = ERROR_INVALID;
        goto cleanup_inode;
    }

    uint32_t parent_dentry_blocks = 0;
    if (dentry_add(fs->disk, parent_inode_num, &new_dentry, fs->block_bitmap, &parent_dentry_blocks) != SUCCESS) {
        status = ERROR_IO;
        goto cleanup_inode;
    }
    fs->sb.free_blocks -= parent_dentry_blocks;

    // add "." and ".." entries
    struct dentry dot, dotdot;

    if (dentry_create(".", new_dir_inode_num, INODE_TYPE_DIRECTORY, &dot) != SUCCESS) {
        status = ERROR_INVALID;
        goto cleanup_remove_parent_dentry;
    }

    uint32_t dot_blocks = 0;
    if (dentry_add(fs->disk, new_dir_inode_num, &dot, fs->block_bitmap, &dot_blocks) != SUCCESS) {
        status = ERROR_IO;
        goto cleanup_remove_parent_dentry;
    }
    fs->sb.free_blocks -= dot_blocks;

    if (dentry_create("..", parent_inode_num, INODE_TYPE_DIRECTORY, &dotdot) != SUCCESS) {
        status = ERROR_INVALID;
        goto cleanup_remove_parent_dentry;
    }

    uint32_t dotdot_blocks = 0;
    if (dentry_add(fs->disk, new_dir_inode_num, &dotdot, fs->block_bitmap, &dotdot_blocks) != SUCCESS) {
        status = ERROR_IO;
        goto cleanup_remove_parent_dentry;
    }
    fs->sb.free_blocks -= dotdot_blocks;

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
        dentry_remove(fs->disk, fs->block_bitmap, &fs->sb, parent_inode_num, dirname);

        // restore only blocks allocated in the parent directory
        fs->sb.free_blocks += parent_dentry_blocks;

    cleanup_inode: {
        // free inode and its blocks
        uint32_t freed_blocks = 0;
        inode_free(fs->disk, fs->inode_bitmap, fs->block_bitmap, new_dir_inode_num, &freed_blocks);
        fs->sb.free_inodes++;
        fs->sb.free_blocks += freed_blocks;
        save_bitmaps(fs);
        superblock_write(fs->disk, &fs->sb);
    }

    return status;
}

int fs_rmdir(filesystem_t* fs, const char* path) {
    if (!fs || !path) {
        return ERROR_INVALID;
    }

    if (!path_is_valid(path)) {
        return ERROR_INVALID;
    }

    if (path_is_root(path)) {
        return ERROR_INVALID;
    }

    uint32_t target_inode_num;
    int res = fs_path_to_inode(fs, path, &target_inode_num);
    if (res != SUCCESS) return res;

    struct inode target_inode;
    if (inode_read(fs->disk, target_inode_num, &target_inode) != SUCCESS) {
        return ERROR_IO;
    }

    if (target_inode.type != INODE_TYPE_DIRECTORY) {
        return ERROR_INVALID;
    }

    // check if empty (only . and .. allowed)
    struct dentry* entries = NULL;
    uint32_t count = 0;
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
        return ERROR_GENERIC;
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

    uint32_t parent_inode_num;
    res = fs_path_to_inode(fs, parent_path, &parent_inode_num);
    if (res != SUCCESS) return res;

    // remove from parent directory
    res = dentry_remove(fs->disk, fs->block_bitmap, &fs->sb, parent_inode_num, dirname);
    if (res != SUCCESS) return res;

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

    // free target directory inode and its blocks
    uint32_t freed_blocks = 0;
    if (inode_free(fs->disk, fs->inode_bitmap, fs->block_bitmap,
                   target_inode_num, &freed_blocks) != SUCCESS) {
        return ERROR_IO;
    }

    fs->sb.free_inodes++;
    fs->sb.free_blocks += freed_blocks;

    if (save_bitmaps(fs) != SUCCESS) {
        return ERROR_IO;
    }

    if (superblock_write(fs->disk, &fs->sb) != SUCCESS) {
        return ERROR_IO;
    }

    return SUCCESS;
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