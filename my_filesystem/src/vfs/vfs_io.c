#include "vfs.h"

int vfs_open(vfs_t* vfs, const char* path, uint32_t flags, open_file_t** out_file) {
    if (!vfs || !path || !out_file) return ERROR_INVALID;

    filesystem_t* target_fs = NULL;
    char local_path[MAX_PATH];
    
    // resolve path to determine which filesystem contains the file
    int res = vfs_resolve_path(vfs, path, &target_fs, local_path, sizeof(local_path), NULL, 0);
    if (res != SUCCESS) return res;
    
    return fs_open(target_fs, local_path, flags, out_file);
}

/*  read, write, seek and close don't even need path or vfs_t arguments as
    the open_file_t struct already has everything we need */
int vfs_read(open_file_t* file, void* buffer, size_t size, size_t* bytes_read) {
    return fs_read(file, buffer, size, bytes_read);
}

int vfs_write(open_file_t* file, const void* buffer, size_t size, size_t* bytes_written) {
    return fs_write(file, buffer, size, bytes_written);
}

int vfs_seek(open_file_t* file, uint32_t offset) {
    return fs_seek(file, offset);
}

int vfs_close(open_file_t* file) {
    return fs_close(file);
}