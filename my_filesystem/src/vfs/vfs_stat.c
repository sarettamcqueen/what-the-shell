#include "vfs.h"
#include <stdio.h>

int vfs_stat(vfs_t* vfs, const char* path, struct inode* out_inode,
            uint32_t* out_inode_num, char* out_abs_path, size_t abs_size) {
    if (!vfs || !path || !out_inode) return ERROR_INVALID;

    filesystem_t* target_fs = NULL;
    char local_path[MAX_PATH];
    
    // vfs resolves path and fills out_abs_path for the shell to use
    int res = vfs_resolve_path(vfs, path, &target_fs, local_path, sizeof(local_path),
                                out_abs_path, abs_size);
    if (res != SUCCESS) return res;
    
    // no need for fs_stat to compute abs_path since we already did it
    return fs_stat(target_fs, local_path, out_inode, out_inode_num, NULL, 0);
}

void vfs_df(vfs_t* vfs) {
    if (!vfs || vfs->count == 0) {
        printf("No filesystems mounted.\n");
        return;
    }

    /* --- riga 1: spazio su disco --- */
    char blk_header[32];
    snprintf(blk_header, sizeof(blk_header), "%u-blocks", (unsigned)BLOCK_SIZE);

    printf("%-20s  %10s  %10s  %10s  %5s  %s\n",
        "Filesystem", blk_header, "Used", "Free", "Use%", "Mounted on");

    for (int i = 0; i < MAX_MOUNTS; i++) {
        if (!vfs->mounts[i].is_active) continue;

        filesystem_t* fs = vfs->mounts[i].fs;

        uint32_t total_blocks = fs->sb.total_blocks;
        uint32_t free_blocks  = fs->sb.free_blocks;
        uint32_t used_blocks  = total_blocks - free_blocks;

        int use_pct = (total_blocks > 0)
                      ? (int)(used_blocks * 100 / total_blocks)
                      : 0;

        const char* dev = disk_get_filename(fs->disk);

        printf("%-20s  %10u  %10u  %10u  %4d%%  %s\n",
               dev,
               total_blocks, used_blocks, free_blocks,
               use_pct,
               vfs->mounts[i].mount_path);
    }

    /* --- riga 2: inode --- */
    printf("\n");
    printf("%-20s  %10s  %10s  %10s  %5s  %s\n",
           "Filesystem", "Inodes", "IUsed", "IFree", "IUse%", "Mounted on");

    for (int i = 0; i < MAX_MOUNTS; i++) {
        if (!vfs->mounts[i].is_active) continue;

        filesystem_t* fs = vfs->mounts[i].fs;

        uint32_t total_inodes = fs->sb.total_inodes;
        uint32_t free_inodes  = fs->sb.free_inodes;
        uint32_t used_inodes  = total_inodes - free_inodes;

        int ino_pct = (total_inodes > 0)
                      ? (int)(used_inodes * 100 / total_inodes)
                      : 0;

        const char* dev = disk_get_filename(fs->disk);

        printf("%-20s  %10u  %10u  %10u  %4d%%  %s\n",
               dev,
               total_inodes, used_inodes, free_inodes,
               ino_pct,
               vfs->mounts[i].mount_path);
    }
    printf("\n");
}