#include "commands.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* fs_error_to_string(int code) {
    switch (code) {
        case SUCCESS:            return "Success";
        case ERROR_INVALID:      return "Invalid argument or malformed path";
        case ERROR_NOT_FOUND:    return "Path not found";
        case ERROR_EXISTS:       return "File or directory already exists";
        case ERROR_PERMISSION:   return "Permission denied";
        case ERROR_NO_SPACE:     return "No space left on device";
        case ERROR_IO:           return "Disk I/O error";
        default:                 return "Unknown error";
    }
}

void print_fs_error(const char* cmd, int code, const char* path) {
    const char* msg = fs_error_to_string(code);

    if (path)
        printf("%s: cannot operate on '%s': %s\n", cmd, path, msg);
    else
        printf("%s: %s\n", cmd, msg);
}

static const char* inode_type_to_string(uint8_t type) {
    switch (type) {
        case INODE_TYPE_FILE: return "file";
        case INODE_TYPE_DIRECTORY: return "directory";
        default: return "unknown";
    }
}

// format <diskfile> <blocks> (used standalone, not inside mounted shell typically)
int cmd_format(int argc, char** argv) {
    if (argc != 3) {
        printf("Usage: format <disk.img> <num_blocks>\n");
        return 0;
    }

    const char* filename = argv[1];
    int blocks = atoi(argv[2]);

    disk_t disk;
    if (disk_attach(filename, blocks * BLOCK_SIZE, true, &disk) != DISK_SUCCESS) {
        printf("format: cannot attach %s\n", filename);
        return 0;
    }

    if (fs_format(disk, blocks, 256) != SUCCESS) {
        printf("format: failed to format '%s'\n", filename);
        disk_detach(disk);
        return 0;
    }
    printf("Filesystem '%s' formatted (%d bytes)\n", filename, blocks * BLOCK_SIZE);

    disk_detach(disk);
    return 0;
}

// mount
int cmd_mount(int argc, char** argv, filesystem_t** fs_p) {
    if (argc != 2) {
        printf("Usage: mount <disk.img>\n");
        return 0;
    }

    if (*fs_p != NULL) {
        printf("mount: a filesystem is already mounted\n");
        return 0;
    }

    char* filename = argv[1];
    disk_t disk;
    if (disk_attach(filename, 0, false, &disk) != DISK_SUCCESS) {
        printf("mount: cannot open disk '%s'\n", filename);
        return 0;
    }

    filesystem_t* fs = NULL;
    if (fs_mount(disk, &fs) != SUCCESS) {
        printf("mount: failed to mount '%s'\n", filename);
        disk_detach(disk);
        return 0;
    }

    *fs_p = fs;
    printf("Mounted %s\n", filename);
    return 0;
}

// unmount
int cmd_unmount(filesystem_t** fs_p) {
    if (*fs_p == NULL) {
        printf("unmount: no filesystem mounted\n");
        return 0;
    }

    filesystem_t* fs = *fs_p;

    if (fs_unmount(fs) != SUCCESS) {
        printf("unmount: failed\n");
        return 0;
    }

    *fs_p = NULL;
    printf("Filesystem unmounted.\n");
    return 0;
}

// pwd
int cmd_pwd(filesystem_t* fs, int argc, char** argv) {
    if (argc != 1) {
        printf("Usage: pwd\n");
        return ERROR_INVALID;
    }
    char path[MAX_PATH];
    int res = fs_inode_to_path(fs, fs->current_dir_inode, path, sizeof(path));
    if (res != SUCCESS) {
        printf("pwd: error resolving current directory\n");
        return res;
    }

    printf("%s\n", path);
    return SUCCESS;
}

// cd
int cmd_cd(filesystem_t* fs, int argc, char** argv) {
    if (argc != 2) {
        printf("Usage: cd <path>\n");
        return 0;
    }
    int ret = fs_cd(fs, argv[1]);
    if (ret != SUCCESS)
        print_fs_error("cd", ret, argv[1]);
    return 0;
}

// mkdir
int cmd_mkdir(filesystem_t* fs, int argc, char** argv) {
    if (argc != 2) {
        printf("Usage: mkdir <dir>\n");
        return 0;
    }
    int ret = fs_mkdir(fs, argv[1], 0755);
    if (ret != SUCCESS)
        print_fs_error("mkdir", ret, argv[1]);
    return ret;
}

// rmdir
int cmd_rmdir(filesystem_t* fs, int argc, char** argv) {
    if (argc != 2) {
        printf("Usage: rmdir <dir>\n");
        return 0;
    }
    int ret = fs_rmdir(fs, argv[1]);
    if (ret != SUCCESS)
        print_fs_error("rmdir", ret, argv[1]);
    return 0;
}

// touch
int cmd_touch(filesystem_t* fs, int argc, char** argv) {
    if (argc != 2) {
        printf("Usage: touch <file>\n");
        return 0;
    }
    int ret = fs_create(fs, argv[1], 0644);
    if (ret != SUCCESS)
        print_fs_error("touch", ret, argv[1]);
    return 0;
}

// rm
int cmd_rm(filesystem_t* fs, int argc, char** argv) {
    if (argc != 2) {
        printf("Usage: rm <file>\n");
        return 0;
    }
    int ret = fs_unlink(fs, argv[1]);
    if (ret != SUCCESS)
        print_fs_error("rm", ret, argv[1]);
    return 0;
}

// cat
int cmd_cat(filesystem_t* fs, int argc, char** argv) {
    if (argc != 2) {
        printf("Usage: cat <file>\n");
        return 0;
    }

    open_file_t* f;
    if (fs_open(fs, argv[1], FS_O_RDONLY, &f) != SUCCESS) {
        printf("cat: cannot open %s\n", argv[1]);
        return 0;
    }

    char buf[1024];
    size_t read;
    int ret = fs_read(f, buf, sizeof(buf) - 1, &read);
    if (ret != SUCCESS) {
        print_fs_error("cat", ret, argv[1]);
        fs_close(f);
        return 0;
    }

    buf[read] = '\0';
    printf("%s", buf);
    printf("\n");

    fs_close(f);
    return 0;
}

// write
int cmd_write(filesystem_t* fs, int argc, char** argv) {
    if (argc != 3) {
        printf("Usage: write <file> \"text\"\n");
        return 0;
    }

    open_file_t* f;
    if (fs_open(fs, argv[1], FS_O_WRONLY | FS_O_TRUNC, &f) != SUCCESS) {
        printf("write: cannot open %s\n", argv[1]);
        return 0;
    }

    size_t w;
    int ret = fs_write(f, argv[2], strlen(argv[2]), &w);
    if (ret != SUCCESS) {
        print_fs_error("write", ret, argv[1]);
    }
    
    fs_close(f);
    return 0;
}

// append
int cmd_append(filesystem_t* fs, int argc, char** argv) {
    if (argc != 3) {
        printf("Usage: append <file> \"text\"\n");
        return 0;
    }

    open_file_t* f;
    if (fs_open(fs, argv[1], FS_O_WRONLY | FS_O_APPEND, &f) != SUCCESS) {
        printf("append: cannot open %s\n", argv[1]);
        return 0;
    }

    size_t w;
    int ret = fs_write(f, argv[2], strlen(argv[2]), &w);
    if (ret != SUCCESS) {
        print_fs_error("append", ret, argv[1]);
    }
    
    fs_close(f);
    return 0;
}

// ls
int cmd_ls(filesystem_t* fs, int argc, char** argv) {
    const char* path = (argc == 2) ? argv[1] : ".";

    struct dentry* list;
    uint32_t count;

    int ret = fs_list(fs, path, &list, &count);
    if (ret != SUCCESS) {
        print_fs_error("ls", ret, path);
        return 0;
    }

    for (uint32_t i = 0; i < count; i++)
        printf("%s  ", list[i].name);
    printf("\n");

    free(list);
    return 0;
}

// ln
int cmd_ln(filesystem_t* fs, int argc, char** argv) {
    if (argc != 3) {
        printf("Usage: ln <src> <dest>\n");
        return 0;
    }

    int ret = fs_link(fs, argv[1], argv[2]);
    if (ret != SUCCESS)
        printf("ln: cannot link %s -> %s: \n", argv[1], argv[2]);
    return 0;
}

// stat
int cmd_stat(filesystem_t* fs, int argc, char** argv) {
    if (argc != 2) {
        printf("Usage: stat <path>\n");
        return 0;
    }

    struct inode st;
    uint32_t inode_num;

    int ret = fs_stat(fs, argv[1], &st, &inode_num);
    if (ret != SUCCESS) {
        print_fs_error("stat", ret, argv[1]);
        return 0;
    }

    // build absolute path for pretty display
    char abs_path[MAX_PATH];

    if (path_is_absolute(argv[1])) {
        // just copy absolute path
        strncpy(abs_path, argv[1], MAX_PATH);
        abs_path[MAX_PATH - 1] = '\0';
    } else {
        // build absolute path from cwd + relative
        char cwd[MAX_PATH];
        fs_inode_to_path(fs, fs->current_dir_inode, cwd, sizeof(cwd));

        if (strcmp(cwd, "/") == 0) {
            /* Special case: root */
            snprintf(abs_path, sizeof(abs_path), "/%s", argv[1]);
        } else {
            snprintf(abs_path, sizeof(abs_path), "%s/%s", cwd, argv[1]);
        }
    }

    // clean up duplicate slashes if any
    path_normalize(abs_path);

    // print inode info
    printf("\n=== STAT ===\n");
    printf("Path          : %s\n", abs_path);
    printf("Type          : %s\n", inode_type_to_string(st.type));
    printf("Size          : %u bytes\n", st.size);
    printf("Blocks used   : %u\n", st.blocks_used);
    printf("Links count   : %u\n", st.links_count);
    printf("Permissions   : %o\n", st.permissions);

    printf("Created       : ");
    print_timestamp(st.created_time);

    printf("\nModified      : ");
    print_timestamp(st.modified_time);

    printf("\nAccessed      : ");
    print_timestamp(st.accessed_time);

    printf("\nDirect blocks : ");
    for (int i = 0; i < 12; i++) {
        if (st.direct[i] == 0) break;
        printf("%u ", st.direct[i]);
    }
    printf("\n");

    if (st.indirect != 0)
        printf("Indirect blk  : %u\n", st.indirect);
    else
        printf("Indirect blk  : (none)\n");

    printf("==============\n\n");
    return 0;
}

// fsinfo
int cmd_fsinfo(filesystem_t* fs) {
    fs_print_stats(fs);
    return SUCCESS;
}
