#include "vfs.h"
#include "path.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int vfs_resolve_path(vfs_t* vfs, const char* path, filesystem_t** out_fs, 
                     char* out_local_path, size_t local_size,
                     char* out_abs_path, size_t abs_size) {
    if (!vfs || !path || !out_fs || !out_local_path || local_size == 0)
        return ERROR_INVALID;

    // normalize input path first
    char* normalized = path_normalize(path);
    if (!normalized)
        return ERROR_INVALID;

    // if path is relative, prepend cwd
    char abs_path[MAX_PATH];
    if (!path_is_absolute(normalized)) {
        int written = snprintf(abs_path, sizeof(abs_path), "%s/%s",
                               strcmp(vfs->cwd, "/") == 0 ? "" : vfs->cwd,
                               normalized);
        free(normalized);
        if (written < 0 || written >= (int)sizeof(abs_path))
            return ERROR_INVALID;

        normalized = path_normalize(abs_path);
        if (!normalized)
            return ERROR_INVALID;
    }
    // find the longest matching mountpoint
        int best_idx = -1;
        size_t best_len = 0;

        for (int i = 0; i < MAX_MOUNTS; i++) {
            if (!vfs->mounts[i].is_active)
                continue;

            size_t mlen = vfs->mounts[i].path_len;
            bool is_root = path_is_root(vfs->mounts[i].mount_path);

            // mountpoint must be a prefix of the path
            if (strncmp(normalized, vfs->mounts[i].mount_path, mlen) != 0)
                continue;

            /* the character after the prefix must be '/' or '\0'
             * (to avoid matching "/mnt/disk" against "/mnt/disk2").
             * EXCEPTION: if mountpoint is "/", it's always a valid match */
            if (!is_root && normalized[mlen] != '\0' && normalized[mlen] != '/')
                continue;

            if (mlen > best_len) {
                best_len = mlen;
                best_idx = i;
            }
        }

        if (best_idx < 0) {
            free(normalized);
            return ERROR_NOT_FOUND;  // no fs mounted
        }

        *out_fs = vfs->mounts[best_idx].fs;

        // compute local path: strip the mountpoint prefix
        const char* local;
        bool is_best_root = path_is_root(vfs->mounts[best_idx].mount_path);
        
        if (is_best_root) {
            local = normalized; // don't strip slash if root is the best match ("/file.txt" remains "/file.txt")
        } else {
            local = normalized + best_len; // "/mnt/usb/file.txt" becomes "/file.txt"
        }

        // local path must always start with '/'
        if (*local == '\0') {
            strncpy(out_local_path, "/", local_size - 1);
            out_local_path[local_size - 1] = '\0';
        } else {
            strncpy(out_local_path, local, local_size - 1);
            out_local_path[local_size - 1] = '\0';
        }

        // save absolute path if needed
        if (out_abs_path && abs_size > 0) {
            strncpy(out_abs_path, normalized, abs_size - 1);
            out_abs_path[abs_size - 1] = '\0';
        }

        free(normalized);
        return SUCCESS;
    }

int vfs_getcwd(vfs_t* vfs, char* buffer, size_t size) {
    if (!vfs || !buffer || size == 0) return ERROR_INVALID;

    strncpy(buffer, vfs->cwd, size - 1);
    buffer[size - 1] = '\0';
    return SUCCESS;
}