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

    printf("%-20s %-10s %-12s %-12s %-12s %-6s\n", 
           "MOUNT POINT", "SIZE", "USED", "FREE", "INODES", "USE%");
    printf("--------------------------------------------------------------------------------\n");
    
    for (int i = 0; i < MAX_MOUNTS; i++) {
        if (!vfs->mounts[i].is_active)
            continue;
            
        filesystem_t* fs = vfs->mounts[i].fs;
        uint32_t used_blocks = fs->sb.total_blocks - fs->sb.free_blocks;
        uint32_t used_bytes = used_blocks * fs->sb.block_size;
        uint32_t total_bytes = fs->sb.total_blocks * fs->sb.block_size;
        int use_percent = (fs->sb.total_blocks > 0) 
                          ? (used_blocks * 100 / fs->sb.total_blocks) 
                          : 0;
        
        printf("%-20s %-10u %-12u %-12u %-12u %5d%%\n", 
               vfs->mounts[i].mount_path,
               total_bytes,
               used_bytes,
               fs->sb.free_blocks * fs->sb.block_size,
               fs->sb.free_inodes,
               use_percent);
    }
    printf("\n");
}