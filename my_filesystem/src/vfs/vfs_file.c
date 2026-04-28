#include "vfs.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

int vfs_create(vfs_t* vfs, const char* path, uint16_t permissions) {
    if (!vfs || !path) return ERROR_INVALID;

    filesystem_t* fs = NULL;
    char local_path[MAX_PATH];
    
    int res = vfs_resolve_path(vfs, path, &fs, local_path, sizeof(local_path), NULL, 0);
    if (res != SUCCESS) return res;
    
    return fs_create(fs, local_path, permissions);
}

int vfs_rm(vfs_t* vfs, const char* path) {
    if (!vfs || !path) return ERROR_INVALID;

    filesystem_t* fs = NULL;
    char local_path[MAX_PATH];
    
    int res = vfs_resolve_path(vfs, path, &fs, local_path, sizeof(local_path), NULL, 0);
    if (res != SUCCESS) return res;
    
    return fs_unlink(fs, local_path); 
}

int vfs_link(vfs_t* vfs, const char* existing_path, const char* new_path) {
    if (!vfs || !existing_path || !new_path) {
        return ERROR_INVALID;
    }

    filesystem_t* fs_existing = NULL;
    char local_existing[MAX_PATH];
    
    filesystem_t* fs_new = NULL;
    char local_new[MAX_PATH];
    
    // resolve source path
    int res = vfs_resolve_path(vfs, existing_path, &fs_existing, local_existing, sizeof(local_existing), NULL, 0);
    if (res != SUCCESS) return res;
    
    // resolve dest path
    res = vfs_resolve_path(vfs, new_path, &fs_new, local_new, sizeof(local_new), NULL, 0);
    if (res != SUCCESS) return res;
    
    // can't create hard links between different disks
    if (fs_existing != fs_new) return ERROR_INVALID;
    
    // same disk, call local filesystem
    return fs_link(fs_existing, local_existing, local_new);
}

/*
int vfs_cp(vfs_t* vfs, const char* src_path, const char* dst_path) {
    if (!vfs || !src_path || !dst_path) return ERROR_INVALID;

    // get source metadata to preserve permissions
    struct inode src_inode;
    int res = vfs_stat(vfs, src_path, &src_inode, NULL, NULL, 0);
    if (res != SUCCESS) return res;

    if (src_inode.type == INODE_TYPE_DIRECTORY) {
        // recursive directory copy is not supported in this version!
        return ERROR_INVALID; 
    }

    //create the destination file with same permissions
    res = vfs_create(vfs, dst_path, src_inode.permissions);
    if (res != SUCCESS && res != ERROR_EXISTS) return res; // if dest file already exists, go on and overwrite it

    open_file_t* f_src = NULL;
    open_file_t* f_dst = NULL;
    
    res = vfs_open(vfs, src_path, FS_O_RDONLY, &f_src);
    if (res != SUCCESS) return res;
    
    res = vfs_open(vfs, dst_path, FS_O_WRONLY | FS_O_TRUNC, &f_dst);
    if (res != SUCCESS) {
        vfs_close(f_src);
        return res;
    }

    // perform the data transfer using a block-sized buffer
    char buffer[BLOCK_SIZE];
    size_t bytes_read = 0;
    size_t bytes_written = 0;

    while (vfs_read(f_src, buffer, sizeof(buffer), &bytes_read) == SUCCESS && bytes_read > 0) {
        res = vfs_write(f_dst, buffer, bytes_read, &bytes_written);
        if (res != SUCCESS || bytes_written != bytes_read) {
            vfs_close(f_src);
            vfs_close(f_dst);
            return ERROR_NO_SPACE;
        }
    }

    vfs_close(f_src);
    vfs_close(f_dst);

    return SUCCESS;
}

int vfs_mv(vfs_t* vfs, const char* src_path, const char* dst_path) {
    if (!vfs || !src_path || !dst_path) return ERROR_INVALID;

    filesystem_t* fs_src = NULL;
    char local_src[MAX_PATH];
    
    filesystem_t* fs_dst = NULL;
    char local_dst[MAX_PATH];
    
    // resolve both paths to determine if they are on the same filesystem
    int res = vfs_resolve_path(vfs, src_path, &fs_src, local_src, sizeof(local_src), NULL, 0);
    if (res != SUCCESS) return res;
    
    res = vfs_resolve_path(vfs, dst_path, &fs_dst, local_dst, sizeof(local_dst), NULL, 0);
    if (res != SUCCESS) return res;

    // check if we are on the same disk
    if (fs_src == fs_dst) {
        // CASE A: same device link-level move 
        // this is fast and works for both files and directories
        return fs_rename(fs_src, local_src, local_dst); 
    } else {
        // CASE B: cross-device move (directories cannot be moved across filesystems)
        // copy the data to the new disk and then delete the original
        struct inode src_inode;
        vfs_stat(vfs, src_path, &src_inode, NULL, NULL, 0);
        if (src_inode.type == INODE_TYPE_DIRECTORY) return ERROR_INVALID; // or ERROR_CROSS_DEVICE
        res = vfs_cp(vfs, src_path, dst_path);
        if (res == SUCCESS) {
            return vfs_rm(vfs, src_path);
        }
        return res;
    }
}
*/

int vfs_cp(vfs_t* vfs, const char* src_path, const char* dst_path) {
    if (!vfs || !src_path || !dst_path) return ERROR_INVALID;

    // get source metadata
    struct inode src_inode;
    int res = vfs_stat(vfs, src_path, &src_inode, NULL, NULL, 0);
    if (res != SUCCESS) return res;

    if (src_inode.type == INODE_TYPE_DIRECTORY) {
        return ERROR_INVALID; 
    }

    // check dest
    struct inode dst_inode;
    int dst_exists = vfs_stat(vfs, dst_path, &dst_inode, NULL, NULL, 0);
    
    if (dst_exists == SUCCESS) {
        // can't overwrite existing directory with a file
        if (dst_inode.type == INODE_TYPE_DIRECTORY) {
            return ERROR_INVALID;
        }

        // solve paths to see if it's same file
        filesystem_t* fs_src = NULL;
        char local_src[MAX_PATH];
        filesystem_t* fs_dst = NULL;
        char local_dst[MAX_PATH];
        
        vfs_resolve_path(vfs, src_path, &fs_src, local_src, sizeof(local_src), NULL, 0);
        vfs_resolve_path(vfs, dst_path, &fs_dst, local_dst, sizeof(local_dst), NULL, 0);

        if (fs_src == fs_dst && strcmp(local_src, local_dst) == 0) {
            // avoid same file copy
            return ERROR_INVALID;
        }
    }

    // create the destination file with same permissions
    res = vfs_create(vfs, dst_path, src_inode.permissions);
    if (res != SUCCESS && res != ERROR_EXISTS) return res; 

    open_file_t* f_src = NULL;
    open_file_t* f_dst = NULL;
    
    res = vfs_open(vfs, src_path, FS_O_RDONLY, &f_src);
    if (res != SUCCESS) return res;
    
    res = vfs_open(vfs, dst_path, FS_O_WRONLY | FS_O_TRUNC, &f_dst);
    if (res != SUCCESS) {
        vfs_close(f_src);
        return res;
    }

    // data transfer
    char buffer[BLOCK_SIZE];
    size_t bytes_read = 0;
    size_t bytes_written = 0;

    while (vfs_read(f_src, buffer, sizeof(buffer), &bytes_read) == SUCCESS && bytes_read > 0) {
        res = vfs_write(f_dst, buffer, bytes_read, &bytes_written);
        if (res != SUCCESS || bytes_written != bytes_read) {
            vfs_close(f_src);
            vfs_close(f_dst);
            return ERROR_NO_SPACE;
        }
    }

    vfs_close(f_src);
    vfs_close(f_dst);

    return SUCCESS;
}

int vfs_mv(vfs_t* vfs, const char* src_path, const char* dst_path) {
    if (!vfs || !src_path || !dst_path) return ERROR_INVALID;

    filesystem_t* fs_src = NULL;
    char local_src[MAX_PATH];
    filesystem_t* fs_dst = NULL;
    char local_dst[MAX_PATH];
    
    // get source metadata first
    struct inode src_inode;
    uint32_t src_inode_num;
    int res = vfs_stat(vfs, src_path, &src_inode, &src_inode_num, NULL, 0);
    if (res != SUCCESS) return res;

    // resolve paths
    res = vfs_resolve_path(vfs, src_path, &fs_src, local_src, sizeof(local_src), NULL, 0);
    if (res != SUCCESS) return res;

    res = vfs_resolve_path(vfs, dst_path, &fs_dst, local_dst, sizeof(local_dst), NULL, 0);
    if (res != SUCCESS) return res;

    // get dest metadata
    struct inode dst_inode;
    uint32_t dst_inode_num = 0;
    int dst_exists = fs_stat(fs_dst, local_dst, &dst_inode, &dst_inode_num, NULL, 0);

    // avoid self-move (can use inode numbers now)
    if (dst_exists == SUCCESS && fs_src == fs_dst && src_inode_num == dst_inode_num) {
        return SUCCESS; // already there
    }

    // handle dest overwrite
    if (dst_exists == SUCCESS) {
        if (dst_inode.type == INODE_TYPE_DIRECTORY) {
            if (src_inode.type != INODE_TYPE_DIRECTORY) return ERROR_INVALID;
            res = vfs_rmdir(vfs, dst_path);
            if (res != SUCCESS) return res;
        } else {
            res = vfs_rm(vfs, dst_path);
            if (res != SUCCESS) return res;
        }
    }

    if (fs_src == fs_dst) {
        return fs_rename(fs_src, local_src, local_dst); 
    } else {
        if (src_inode.type == INODE_TYPE_DIRECTORY) return ERROR_INVALID;
        
        res = vfs_cp(vfs, src_path, dst_path);
        if (res != SUCCESS) return res;
        
        res = vfs_rm(vfs, src_path);
        if (res != SUCCESS) {
            vfs_rm(vfs, dst_path);
            return res;
        }
        return SUCCESS;
    }
}