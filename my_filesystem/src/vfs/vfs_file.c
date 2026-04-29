#include "vfs.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

// solves final destination path:
// - if dst exists and it's a directory -> dst/basename(src)
// - else -> dst is absolute path
// returns a normalized absolute path in out_final_dst
static int resolve_dst(vfs_t* vfs, const char* src, const char* dst,
                        char* out_final_dst, size_t size) {
    filesystem_t* dst_fs = NULL;
    char local_path[MAX_PATH];
    char abs_dst[MAX_PATH];

    int ret = vfs_resolve_path(vfs, dst, &dst_fs, local_path, sizeof(local_path),
                                abs_dst, sizeof(abs_dst));

    if (ret == SUCCESS && dst_fs != NULL) {
        struct inode st;
        if (fs_stat(dst_fs, local_path, &st, NULL, NULL, 0) == SUCCESS
                && st.type == INODE_TYPE_DIRECTORY) {

            char* base = path_get_basename(src);
            if (!base) return ERROR_INVALID;

            size_t dst_len = strlen(abs_dst);
            int written;
            if (dst_len > 0 && abs_dst[dst_len - 1] == '/')
                written = snprintf(out_final_dst, size, "%s%s", abs_dst, base);
            else
                written = snprintf(out_final_dst, size, "%s/%s", abs_dst, base);

            free(base);
            if (written < 0 || written >= (int)size) return ERROR_INVALID;
            return SUCCESS;
        }
    }

    // dst is not a directory (or doesn't exist): use abs_dst
    strncpy(out_final_dst, abs_dst, size - 1);
    out_final_dst[size - 1] = '\0';
    return SUCCESS;
}

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

int vfs_cp(vfs_t* vfs, const char* src_path, const char* dst_path) {
    if (!vfs || !src_path || !dst_path) return ERROR_INVALID;

    char final_dst[MAX_PATH];
    int res = resolve_dst(vfs, src_path, dst_path, final_dst, sizeof(final_dst));
    if (res != SUCCESS) return res;

    struct inode src_inode;
    res = vfs_stat(vfs, src_path, &src_inode, NULL, NULL, 0);
    if (res != SUCCESS) return res;

    if (src_inode.type == INODE_TYPE_DIRECTORY)
        return ERROR_INVALID;

    // check if dst already exists
    struct inode dst_inode;
    int dst_exists = vfs_stat(vfs, final_dst, &dst_inode, NULL, NULL, 0);

    if (dst_exists == SUCCESS) {
        if (dst_inode.type == INODE_TYPE_DIRECTORY)
            return ERROR_INVALID;

        // avoid self copy
        filesystem_t* fs_src = NULL; char local_src[MAX_PATH];
        filesystem_t* fs_dst = NULL; char local_dst[MAX_PATH];
        vfs_resolve_path(vfs, src_path,  &fs_src, local_src, sizeof(local_src), NULL, 0);
        vfs_resolve_path(vfs, final_dst, &fs_dst, local_dst, sizeof(local_dst), NULL, 0);
        if (fs_src == fs_dst && strcmp(local_src, local_dst) == 0)
            return ERROR_INVALID;
    }

    res = vfs_create(vfs, final_dst, src_inode.permissions);
    if (res != SUCCESS && res != ERROR_EXISTS) return res;

    open_file_t* f_src = NULL;
    open_file_t* f_dst = NULL;

    res = vfs_open(vfs, src_path,  FS_O_RDONLY, &f_src);
    if (res != SUCCESS) return res;

    res = vfs_open(vfs, final_dst, FS_O_WRONLY | FS_O_TRUNC, &f_dst);
    if (res != SUCCESS) { vfs_close(f_src); return res; }

    char buffer[BLOCK_SIZE];
    size_t bytes_read = 0, bytes_written = 0;

    while (vfs_read(f_src, buffer, sizeof(buffer), &bytes_read) == SUCCESS
            && bytes_read > 0) {
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

    // resolve both raw paths to absolute before resolve_dst, to detect self-move (mv a a)
    char abs_src[MAX_PATH], abs_dst_raw[MAX_PATH];
    filesystem_t* tmp_fs = NULL;
    char tmp_local[MAX_PATH];

    int res = vfs_resolve_path(vfs, src_path, &tmp_fs, tmp_local,
                               sizeof(tmp_local), abs_src, sizeof(abs_src));
    if (res != SUCCESS) return res;

    res = vfs_resolve_path(vfs, dst_path, &tmp_fs, tmp_local,
                           sizeof(tmp_local), abs_dst_raw, sizeof(abs_dst_raw));
    if (res != SUCCESS) return res;

    // self-move: silent
    if (strcmp(abs_src, abs_dst_raw) == 0) return SUCCESS;

    // resolve dst: if dst is an existing directory -> dst/basename(src)
    char final_dst[MAX_PATH];
    res = resolve_dst(vfs, src_path, dst_path, final_dst, sizeof(final_dst));
    if (res != SUCCESS) return res;

    struct inode src_inode;
    uint32_t src_inode_num;
    res = vfs_stat(vfs, src_path, &src_inode, &src_inode_num, NULL, 0);
    if (res != SUCCESS) return res;

    filesystem_t* fs_src = NULL; char local_src[MAX_PATH];
    filesystem_t* fs_dst = NULL; char local_dst[MAX_PATH];

    res = vfs_resolve_path(vfs, src_path,  &fs_src, local_src, sizeof(local_src), NULL, 0);
    if (res != SUCCESS) return res;
    res = vfs_resolve_path(vfs, final_dst, &fs_dst, local_dst, sizeof(local_dst), NULL, 0);
    if (res != SUCCESS) return res;

    // check if dst already exists
    struct inode dst_inode;
    uint32_t dst_inode_num = 0;
    int dst_exists = fs_stat(fs_dst, local_dst, &dst_inode, &dst_inode_num, NULL, 0);

    // self-move after basename resolution (eg mv file.txt dir/ where dir contains file.txt)
    if (dst_exists == SUCCESS && fs_src == fs_dst && src_inode_num == dst_inode_num)
        return SUCCESS;

    // remove dst if it already exists
    if (dst_exists == SUCCESS) {
        if (dst_inode.type == INODE_TYPE_DIRECTORY) {
            if (src_inode.type != INODE_TYPE_DIRECTORY) return ERROR_INVALID;
            res = vfs_rmdir(vfs, final_dst);
        } else {
            res = vfs_rm(vfs, final_dst);
        }
        if (res != SUCCESS) return res;
    }

    if (fs_src == fs_dst) {
        return fs_rename(fs_src, local_src, local_dst);
    } else {
        if (src_inode.type == INODE_TYPE_DIRECTORY) return ERROR_INVALID;
        res = vfs_cp(vfs, src_path, final_dst);
        if (res != SUCCESS) return res;
        res = vfs_rm(vfs, src_path);
        if (res != SUCCESS) {
            vfs_rm(vfs, final_dst); // rollback
            return res;
        }
        return SUCCESS;
    }
}