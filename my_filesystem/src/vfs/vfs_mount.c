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

int vfs_format(vfs_t* vfs, const char* filename, uint64_t size_bytes) {
    if (!vfs || !filename) return ERROR_INVALID;
    if (size_bytes == 0)   return ERROR_INVALID;

    // check fs isn't already mounted
    for (int i = 0; i < MAX_MOUNTS; i++) {
        if (!vfs->mounts[i].is_active) continue;
        const char* mounted_disk = disk_get_filename(vfs->mounts[i].fs->disk);
        if (strcmp(mounted_disk, filename) == 0)
            return ERROR_BUSY;
    }

    // align size_bytes to block size
    uint64_t aligned = size_bytes;
    if (size_bytes % BLOCK_SIZE != 0)
        aligned = size_bytes + (BLOCK_SIZE - (size_bytes % BLOCK_SIZE));

    disk_t disk;
    int ret = disk_attach(filename, (long long)aligned, true, &disk);
    if (ret != DISK_SUCCESS) return map_disk_error(ret);

    uint32_t total_blocks = (uint32_t)(aligned / BLOCK_SIZE);
    uint32_t total_inodes = (uint32_t)(aligned / BYTES_PER_INODE);

    if (total_inodes % INODES_PER_BLOCK != 0)
        total_inodes += INODES_PER_BLOCK - (total_inodes % INODES_PER_BLOCK);
    if (total_inodes < MIN_INODES)
        total_inodes = MIN_INODES;

    ret = fs_format(disk, total_blocks, total_inodes);
    disk_detach(disk);

    return ret;
}

int vfs_mount(vfs_t* vfs, const char* filename, const char* mount_path) {
    if (!vfs || !filename || !mount_path) return ERROR_INVALID;
    if (vfs->count >= MAX_MOUNTS)         return ERROR_NO_SPACE;

    // first disk must always be mounted on root "/"
    if (vfs->count == 0 && strcmp(mount_path, "/") != 0)
        return ERROR_ROOT_REQUIRED;

    // avoid mounting same disk on different mount points
    for (int i = 0; i < MAX_MOUNTS; i++) {
        if (!vfs->mounts[i].is_active) continue;
        if (strcmp(disk_get_filename(vfs->mounts[i].fs->disk), filename) == 0)
            return ERROR_BUSY;
    }

    char abs_mount[MAX_PATH];
    if (vfs->count > 0) {
        filesystem_t* tmp_fs  = NULL;
        char local_path[MAX_PATH];
        int ret = vfs_resolve_path(vfs, mount_path, &tmp_fs,
                                   local_path, sizeof(local_path),
                                   abs_mount,  sizeof(abs_mount));
        if (ret != SUCCESS) return ERROR_NOT_FOUND;

        // check if mount point is a valid directory
        struct inode st;
        ret = fs_stat(tmp_fs, local_path, &st, NULL, NULL, 0);
        if (ret != SUCCESS || st.type != INODE_TYPE_DIRECTORY)
            return ERROR_INVALID;

        // check if mount_path is already in use
        for (int i = 0; i < MAX_MOUNTS; i++) {
            if (!vfs->mounts[i].is_active) continue;
            if (strcmp(vfs->mounts[i].mount_path, abs_mount) == 0)
                return ERROR_BUSY;
        }
    } else {
        // no fs mounted yet: mount_path is "/"
        strncpy(abs_mount, mount_path, MAX_PATH - 1);
        abs_mount[MAX_PATH - 1] = '\0';
    }

    // open disk and initialize fs
    disk_t disk;
    int ret = disk_attach(filename, 0, false, &disk);
    if (ret != DISK_SUCCESS) return map_disk_error(ret);

    filesystem_t* fs = NULL;
    ret = fs_mount(disk, &fs);
    if (ret != SUCCESS) {
        disk_detach(disk);
        return ret;
    }

    // find first free slot
    for (int i = 0; i < MAX_MOUNTS; i++) {
        if (!vfs->mounts[i].is_active) {
            strncpy(vfs->mounts[i].mount_path, abs_mount, MAX_PATH - 1);
            vfs->mounts[i].mount_path[MAX_PATH - 1] = '\0';
            remove_trailing_slashes(vfs->mounts[i].mount_path);
            vfs->mounts[i].path_len = strlen(vfs->mounts[i].mount_path);
            vfs->mounts[i].fs = fs;
            vfs->mounts[i].is_active = true;
            vfs->count++;
            return SUCCESS;
        }
    }

    fs_unmount(fs);
    return ERROR_NO_SPACE;
}

int vfs_unmount(vfs_t* vfs, const char* mount_path) {
    if (!vfs || !mount_path) return ERROR_INVALID;

    // resolve to absolute before searching the mount table,
    // so relative paths and "." work correctly
    char abs_path[MAX_PATH];
    filesystem_t* tmp_fs = NULL;
    char tmp_local[MAX_PATH];

    int res = vfs_resolve_path(vfs, mount_path, &tmp_fs, tmp_local,
                               sizeof(tmp_local), abs_path, sizeof(abs_path));
    if (res != SUCCESS) return res;

    remove_trailing_slashes(abs_path);

    for (int i = 0; i < MAX_MOUNTS; i++) {
        if (!vfs->mounts[i].is_active) continue;
        if (strcmp(vfs->mounts[i].mount_path, abs_path) != 0) continue;

        // can't unmount root during an active session
        if (strcmp(abs_path, "/") == 0)
            return ERROR_BUSY;
        
        // cannot unmount a filesystem while cwd is inside it
        if (path_starts_with(vfs->cwd, abs_path))
            return ERROR_BUSY;

        res = fs_unmount(vfs->mounts[i].fs);
        if (res != SUCCESS) return res;

        vfs->mounts[i].is_active = false;
        vfs->mounts[i].fs        = NULL;
        vfs->count--;

        return SUCCESS;
    }

    return ERROR_NOT_FOUND;
}

void vfs_destroy(vfs_t* vfs) {
    if (!vfs) return;

    // unmount in reverse order so nested mounts are freed before their parent
    for (int i = MAX_MOUNTS - 1; i >= 0; i--) {
        if (!vfs->mounts[i].is_active) continue;
        fs_unmount(vfs->mounts[i].fs);
        vfs->mounts[i].is_active = false;
        vfs->mounts[i].fs = NULL;
        vfs->count--;
    }
}