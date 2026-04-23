#include "fs.h"
#include "fs_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int fs_stat(filesystem_t* fs, const char* path, struct inode* out_inode,
            uint32_t* out_inode_num, char* out_abs_path, size_t out_abs_path_size) {
    if (!fs || !path || !out_inode)
        return ERROR_INVALID;

    if (!path_is_valid(path))
        return ERROR_INVALID;

    uint32_t inode_num;
    int res = fs_path_to_inode(fs, path, &inode_num);
    if (res != SUCCESS) return res;

    res = inode_read(fs->disk, inode_num, out_inode);
    if (res != SUCCESS) return res;

    if (out_inode_num)
        *out_inode_num = inode_num;

    if (out_abs_path && out_abs_path_size > 0) {
        if (path_is_absolute(path)) {
            char* normalized = path_normalize(path);
            if (normalized) {
                strncpy(out_abs_path, normalized, out_abs_path_size - 1);
                out_abs_path[out_abs_path_size - 1] = '\0';
                free(normalized);
            }
        } else {
            char cwd[MAX_PATH];
            fs_inode_to_path(fs, fs->current_dir_inode, cwd, sizeof(cwd));

            char tmp[MAX_PATH];
            int written = snprintf(tmp, sizeof(tmp), "%s/%s",
                                   strcmp(cwd, "/") == 0 ? "" : cwd, path);
            if (written < 0 || written >= (int)sizeof(tmp))
                tmp[sizeof(tmp) - 1] = '\0';  // truncate

            char* normalized = path_normalize(tmp);
            if (normalized) {
                strncpy(out_abs_path, normalized, out_abs_path_size - 1);
                out_abs_path[out_abs_path_size - 1] = '\0';
                free(normalized);
            }
        }
    }

    return SUCCESS;
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