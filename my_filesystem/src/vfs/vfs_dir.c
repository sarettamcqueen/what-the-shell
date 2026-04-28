#include "vfs.h"
#include <stdio.h>
#include <string.h>

int vfs_mkdir(vfs_t* vfs, const char* path, uint16_t permissions) {
    if (!vfs || !path) return ERROR_INVALID;

    filesystem_t* fs = NULL;
    char local_path[MAX_PATH];
    
    int res = vfs_resolve_path(vfs, path, &fs, local_path, sizeof(local_path), NULL, 0);
    if (res != SUCCESS) return res;
    
    return fs_mkdir(fs, local_path, permissions);
}

int vfs_rmdir(vfs_t* vfs, const char* path) {
    if (!vfs || !path) return ERROR_INVALID;

    filesystem_t* fs = NULL;
    char local_path[MAX_PATH];
    
    int res = vfs_resolve_path(vfs, path, &fs, local_path, sizeof(local_path), NULL, 0);
    if (res != SUCCESS) return res;
    
    return fs_rmdir(fs, local_path);
}

int vfs_cd(vfs_t* vfs, const char* path) {
    if (!vfs || !path) return ERROR_INVALID;

    filesystem_t* target_fs = NULL;
    char local_path[MAX_PATH];
    char abs_path[MAX_PATH];
    
    // resolve path
    int res = vfs_resolve_path(vfs, path, &target_fs, local_path, sizeof(local_path),
                                abs_path, sizeof(abs_path));

    if (res != SUCCESS) return res;

    // special case for root: no need to check if it's a directory
    if (strcmp(abs_path, "/") == 0) {
        strcpy(vfs->cwd, "/");
        return SUCCESS;
    }
    
    // check if directory exists on disk
    struct inode dir_inode;
    res = fs_stat(target_fs, local_path, &dir_inode, NULL, NULL, 0);
    if (res != SUCCESS) return res;
    
    if (dir_inode.type != INODE_TYPE_DIRECTORY) return ERROR_INVALID;

    // update the vfs's global cwd field
    strncpy(vfs->cwd, abs_path, MAX_PATH - 1);
    vfs->cwd[MAX_PATH - 1] = '\0';

    return SUCCESS;
}

int vfs_ls(vfs_t* vfs, const char* path, struct dentry** out_entries, uint32_t* out_count) {
    if (!vfs || !path || !out_entries || !out_count) return ERROR_INVALID;

    filesystem_t* target_fs = NULL;
    char local_path[MAX_PATH];
    
    int res = vfs_resolve_path(vfs, path, &target_fs, local_path, sizeof(local_path), NULL, 0);
    if (res != SUCCESS) return res;
    
    return fs_list(target_fs, local_path, out_entries, out_count);
}