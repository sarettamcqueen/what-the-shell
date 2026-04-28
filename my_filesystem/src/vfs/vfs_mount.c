#include "vfs.h"
#include "fs.h"
#include "disk.h"
#include <string.h>
#include <stdio.h>

void vfs_init(vfs_t* vfs) {
    vfs->count = 0;
    strcpy(vfs->cwd, "/"); // start from root
    
    for (int i = 0; i < MAX_MOUNTS; i++) {
        vfs->mounts[i].is_active = false;
        vfs->mounts[i].fs = NULL;
    }
}

int vfs_check_format(vfs_t* vfs, const char* filename) {
    if (!vfs || !filename) return ERROR_INVALID;

    // check if disk is mounted
    for (uint32_t i = 0; i < vfs->count; i++) {
        const char* mounted_disk = disk_get_filename(vfs->mounts[i].fs->disk);
        if (strcmp(mounted_disk, filename) == 0) {
            return ERROR_BUSY;
        }
    }
    
    return SUCCESS;
}

int vfs_check_mount(vfs_t* vfs, const char* mount_path, const char* disk_filename) {
    if (!vfs || !mount_path || !disk_filename) return ERROR_INVALID;

    // first disk must always be mounted on root "/"
    if (vfs->count == 0 && strcmp(mount_path, "/") != 0) {
        return ERROR_ROOT_REQUIRED;
    }

    if (vfs->count > 0) {
        // check if mount_path is already in use
        for (uint32_t i = 0; i < vfs->count; i++) {
            if (strcmp(vfs->mounts[i].mount_path, mount_path) == 0) {
                return ERROR_EXISTS;
            }
            // avoid mounting same disk on different mount points
            const char* existing_disk = disk_get_filename(vfs->mounts[i].fs->disk);
            if (strcmp(existing_disk, disk_filename) == 0) {
                return ERROR_INVALID; 
            }
        }

        // check if mount point is a valid directory
        filesystem_t* target_fs = NULL;
        char local_path[MAX_PATH];
        char abs_path[MAX_PATH];
        int ret = vfs_resolve_path(vfs, mount_path, &target_fs, local_path, sizeof(local_path), abs_path, sizeof(abs_path));
        
        if (ret != SUCCESS) return ERROR_NOT_FOUND;

        struct inode st;
        uint32_t target_inode;
        ret = fs_stat(target_fs, local_path, &st, &target_inode, NULL, 0);
        if (ret != SUCCESS || st.type != INODE_TYPE_DIRECTORY) {
            return ERROR_INVALID;
        }
    }

    return SUCCESS;
}

int vfs_mount(vfs_t* vfs, const char* mount_path, filesystem_t* fs) {
    if (!vfs || !mount_path || !fs) return ERROR_INVALID;
    if (vfs->count >= MAX_MOUNTS) return ERROR_NO_SPACE;

    // first disk must always be mounted on root "/"
    if (vfs->count == 0 && strcmp(mount_path, "/") != 0) {
        return ERROR_ROOT_REQUIRED;
    }

    // check if path is already in use (to avoid mounting two disks on same point)
    for (int i = 0; i < MAX_MOUNTS; i++) {
        if (vfs->mounts[i].is_active && strcmp(vfs->mounts[i].mount_path, mount_path) == 0) {
            return ERROR_INVALID; // path already in use
        }
    }

    // find first free slot
    for (int i = 0; i < MAX_MOUNTS; i++) {
        if (!vfs->mounts[i].is_active) {
            // fill the slot
            strncpy(vfs->mounts[i].mount_path, mount_path, MAX_PATH - 1);
            vfs->mounts[i].mount_path[MAX_PATH - 1] = '\0';
            
            // remove final slashes, except for root "/"
            remove_trailing_slashes(vfs->mounts[i].mount_path);
            
            vfs->mounts[i].path_len = strlen(vfs->mounts[i].mount_path);
            vfs->mounts[i].fs = fs;
            vfs->mounts[i].is_active = true;
            vfs->count++;
            return SUCCESS;
        }
    }
    
    return ERROR_NO_SPACE;
}

int vfs_unmount(vfs_t* vfs, const char* mount_path) {
    if (!vfs || !mount_path) return ERROR_INVALID;

    char normalized_path[MAX_PATH];
    strncpy(normalized_path, mount_path, MAX_PATH - 1);
    normalized_path[MAX_PATH - 1] = '\0';
    remove_trailing_slashes(normalized_path);

    for (int i = 0; i < MAX_MOUNTS; i++) {
        if (vfs->mounts[i].is_active && strcmp(vfs->mounts[i].mount_path, normalized_path) == 0) {
            // found it, we free the slot
            int res = fs_unmount(vfs->mounts[i].fs);
            if (res != SUCCESS) return res;

            vfs->mounts[i].is_active = false;
            vfs->mounts[i].fs = NULL;
            vfs->count--;
            
            if (path_starts_with(vfs->cwd, normalized_path)) {
                strcpy(vfs->cwd, "/");
            }
            
            return SUCCESS;
        }
    }
    return ERROR_NOT_FOUND; // Nessun disco montato in quel path
}