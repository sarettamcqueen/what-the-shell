#include "fs.h"
#include "fs_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/**
 * Resolves a path to an inode number.
 * Supports absolute and relative paths.
 */
int fs_path_to_inode(filesystem_t* fs, const char* path, uint32_t* out_inode_num) {
if (!fs || !path || !out_inode_num) {
        return ERROR_INVALID;
    }

    // validate path
    if (!path_is_valid(path)) {
        return ERROR_INVALID;
    }

    // normalize path to handle ".", "..", and redundant separators
    char* normalized = path_normalize(path);
    if (!normalized) {
        return ERROR_INVALID;
    }

    // handle root
    if (path_is_root(normalized)) {
        *out_inode_num = ROOT_INODE_NUM;
        free(normalized);
        return SUCCESS;
    }

    // parse path
    struct path_components* pc = path_parse(normalized);
    free(normalized);
    
    if (!pc) {
        return ERROR_INVALID;
    }

    // start from root or current directory
    uint32_t current_inode = pc->is_absolute ? ROOT_INODE_NUM : fs->current_dir_inode;

    // traverse components
    for (int i = 0; i < pc->count; i++) {
        const char* component = pc->components[i];

        // skip "." (should already be handled by normalize, but just in case)
        if (strcmp(component, ".") == 0) {
            continue;
        }

        // handle ".."
        if (strcmp(component, "..") == 0) {
            struct dentry parent_entry;
            if (dentry_find(fs->disk, current_inode, "..", &parent_entry, NULL) == SUCCESS) {
                current_inode = parent_entry.inode_num;
            } else {
                // root has no parent
                if (current_inode != ROOT_INODE_NUM) {
                    path_components_free(pc);
                    return ERROR_NOT_FOUND;
                }
            }
            continue;
        }

        // regular lookup
        struct dentry entry;
        if (dentry_find(fs->disk, current_inode, component, &entry, NULL) != SUCCESS) {
            path_components_free(pc);
            return ERROR_NOT_FOUND;
        }

        current_inode = entry.inode_num;
    }

    path_components_free(pc);
    *out_inode_num = current_inode;
    return SUCCESS;
}

int fs_inode_to_path(filesystem_t* fs, uint32_t inode_num, char* out_path, size_t out_size) {
    if (!fs || !out_path || out_size == 0)
        return ERROR_INVALID;

    if (inode_num == ROOT_INODE_NUM) {
        snprintf(out_path, out_size, "/");
        return SUCCESS;
    }

    // NOTE: we store intermediate path components here while climbing up the directory tree.
    // The maximum depth is set to 64: it is unlikely for a valid filesystem path to contain
    // more than 64 nested directories in normal usage.
    // If deeper directory hierarchies are required, this value can be safely increased.
    // Using a fixed-size array avoids dynamic allocations and simplifies error handling.
    // For files, components[0] is pre-filled with the filename before entering the loop.
    char components[64][MAX_FILENAME];
    int depth = 0;

    uint32_t current = inode_num;

    // check if the starting inode is a file (not a directory)
    // Files don't have "." and ".." entries, so we need to find
    // the file's name inside its parent before starting the climb.
    struct inode starting_inode;
    if (inode_read(fs->disk, inode_num, &starting_inode) != SUCCESS)
        return ERROR_IO;

    if (starting_inode.type != INODE_TYPE_DIRECTORY) {
        // it's a file: since fs_path_to_inode already
        // works, we flip the problem: find parent via dentry reverse lookup
        // using fs->current_dir_inode as a hint, or walk from root.

        uint32_t total_inodes = fs->sb.total_inodes;
        uint32_t found_parent = 0;
        int found_name = 0;

        for (uint32_t candidate = ROOT_INODE_NUM;
             candidate <= total_inodes && !found_name;
             candidate++) {

            if (!bitmap_get(fs->inode_bitmap, candidate))
                continue;

            struct inode candidate_inode;
            if (inode_read(fs->disk, candidate, &candidate_inode) != SUCCESS)
                continue;

            if (candidate_inode.type != INODE_TYPE_DIRECTORY)
                continue;

            uint32_t count = 0;
            struct dentry* list = NULL;
            if (dentry_list(fs->disk, candidate, &list, &count) != SUCCESS)
                continue;

            for (uint32_t i = 0; i < count; i++) {
                if (strcmp(list[i].name, ".") == 0 || strcmp(list[i].name, "..") == 0)
                    continue;
                if (list[i].inode_num == inode_num) {
                    strncpy(components[depth], list[i].name, MAX_FILENAME);
                    components[depth][MAX_FILENAME - 1] = '\0';
                    depth++;
                    found_parent = candidate;
                    found_name = 1;
                    break;
                }
            }
            free(list);
        }

        if (!found_name)
            return ERROR_NOT_FOUND;

        // continue climbing from found_parent
        current = found_parent;
    }

    // climb up the directory tree
    while (current != ROOT_INODE_NUM) {
        struct dentry parent;
        if (dentry_find(fs->disk, current, "..", &parent, NULL) != SUCCESS)
            return ERROR_IO;

        uint32_t parent_inode = parent.inode_num;

        uint32_t count = 0;
        struct dentry* list = NULL;
        if (dentry_list(fs->disk, parent_inode, &list, &count) != SUCCESS)
            return ERROR_IO;

        int found = 0;
        for (uint32_t i = 0; i < count; i++) {
            if (strcmp(list[i].name, ".") == 0 || strcmp(list[i].name, "..") == 0)
                continue;
            if (list[i].inode_num == current) {
                strncpy(components[depth], list[i].name, MAX_FILENAME);
                components[depth][MAX_FILENAME - 1] = '\0';
                found = 1;
                break;
            }
        }

        free(list);

        if (!found)
            return ERROR_NOT_FOUND;

        depth++;
        current = parent_inode;
    }

    // rebuild full absolute path
    size_t offset = 0;
    out_path[0] = '\0';

    for (int i = depth - 1; i >= 0; i--) {
        size_t len = strlen(components[i]);
        if (offset + len + 2 >= out_size)
            return ERROR_NO_SPACE;

        out_path[offset++] = '/';
        memcpy(&out_path[offset], components[i], len);
        offset += len;
        out_path[offset] = '\0';
    }

    if (offset == 0)
        snprintf(out_path, out_size, "/");

    return SUCCESS;
}

int fs_getcwd(filesystem_t* fs, char* buffer, size_t size) {
    if (!fs || !buffer || size == 0)
        return ERROR_INVALID;

    return fs_inode_to_path(fs, fs->current_dir_inode, buffer, size);
}