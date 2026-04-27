#include "vfs.h"
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

int vfs_mount(vfs_t* vfs, const char* mount_path, filesystem_t* fs) {
    if (!vfs || !mount_path || !fs) return ERROR_INVALID;
    if (vfs->count >= MAX_MOUNTS) return ERROR_NO_SPACE;

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
            
            // Opzionale: Se la CWD dell'utente era dentro il disco smontato, lo riportiamo alla root di emergenza
            if (path_starts_with(vfs->cwd, normalized_path)) {
                strcpy(vfs->cwd, "/");
            }
            
            return SUCCESS;
        }
    }
    return ERROR_NOT_FOUND; // Nessun disco montato in quel path
}