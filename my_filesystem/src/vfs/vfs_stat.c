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

    printf("%-15s | %-10s %-10s %-10s | %-8s %-8s %-8s %-5s | %-8s %-8s %-8s %-5s\n", 
           "MOUNT POINT", 
           "SIZE (B)", "USED (B)", "FREE (B)", 
           "BLOCKS", "USED", "FREE", "USE%", 
           "INODES", "USED", "FREE", "IUSE%");
           
    printf("----------------|------------------------------------|---------------------------------------|---------------------------------------\n");
    
    for (int i = 0; i < MAX_MOUNTS; i++) {
        if (!vfs->mounts[i].is_active)
            continue;
            
        filesystem_t* fs = vfs->mounts[i].fs;
        
        uint32_t total_blocks = fs->sb.total_blocks;
        uint32_t free_blocks  = fs->sb.free_blocks;
        uint32_t used_blocks  = total_blocks - free_blocks;
        
        uint32_t blk_size     = fs->sb.block_size;
        uint32_t total_bytes  = total_blocks * blk_size;
        uint32_t used_bytes   = used_blocks * blk_size;
        uint32_t free_bytes   = free_blocks * blk_size;
        
        uint32_t total_inodes = fs->sb.total_inodes;
        uint32_t free_inodes  = fs->sb.free_inodes;
        uint32_t used_inodes  = total_inodes - free_inodes;
        
        int blk_pct = (total_blocks > 0) ? (used_blocks * 100 / total_blocks) : 0;
        int ino_pct = (total_inodes > 0) ? (used_inodes * 100 / total_inodes) : 0;

        printf("%-15s | %-10u %-10u %-10u | %-8u %-8u %-8u %3d%% | %-8u %-8u %-8u %3d%%\n", 
               vfs->mounts[i].mount_path,
               total_bytes, used_bytes, free_bytes,
               total_blocks, used_blocks, free_blocks, blk_pct,
               total_inodes, used_inodes, free_inodes, ino_pct);
    }
    printf("\n");
}