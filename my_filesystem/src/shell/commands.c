#include "commands.h"
#include "common.h"
#include "vfs.h"
#include "fs.h"
#include "path.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* fs_error_to_string(int code) {
    switch (code) {
        case SUCCESS:               return "Success";
        case ERROR_INVALID:         return "Invalid argument or malformed path";
        case ERROR_NOT_FOUND:       return "Path not found";
        case ERROR_EXISTS:          return "File or directory already exists";
        case ERROR_PERMISSION:      return "Permission denied";
        case ERROR_NO_SPACE:        return "No space left on device";
        case ERROR_IO:              return "Disk I/O error";
        case ERROR_ROOT_REQUIRED:   return "First mount must be on root directory '/'";
        case ERROR_BUSY:            return "Device or resource busy";
        default:                    return "Unknown error";
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

int cmd_format(vfs_t* vfs, int argc, char** argv) {
    if (argc != 3) {
        printf("Usage: format <diskname> <size_in_bytes>\n");
        return ERROR_INVALID;
    }

    int input_size = atoi(argv[2]);
    if (input_size <= 0) {
        printf("format: invalid size '%s'\n", argv[2]);
        return ERROR_INVALID;
    }

    int remainder = input_size % BLOCK_SIZE;
    long long aligned = input_size;
    if (remainder != 0) {
        aligned = input_size + (BLOCK_SIZE - remainder);
        printf("format: size %d is not aligned to %d bytes, rounding up to %lld\n",
               input_size, BLOCK_SIZE, aligned);
    }

    int ret = vfs_format(vfs, argv[1], (uint64_t)aligned);
    if (ret != SUCCESS)
        print_fs_error("format", ret, argv[1]);

    return ret;
}

int cmd_mount(vfs_t* vfs, int argc, char** argv) {
    if (argc != 3) {
        printf("Usage: mount <disk.img> <mount_path>\n");
        return ERROR_INVALID;
    }

    int ret = vfs_mount(vfs, argv[1], argv[2]);
    if (ret != SUCCESS)
        print_fs_error("mount", ret, argv[2]);
    return ret;
}

int cmd_unmount(vfs_t* vfs, int argc, char** argv) {
    if (argc != 2) {
        printf("Usage: unmount <mount_path>\n");
        return ERROR_INVALID;
    }

    int ret = vfs_unmount(vfs, argv[1]);
    if (ret != SUCCESS) {
        print_fs_error("unmount", ret, argv[1]);
        return ret;
    }

    printf("Unmounted %s\n", argv[1]);
    return SUCCESS;
}

int cmd_pwd(vfs_t* vfs, int argc, char** argv) {
    (void)argv;
    if (argc != 1) {
        printf("Usage: pwd\n");
        return ERROR_INVALID;
    }
    char path[MAX_PATH];
    int ret = vfs_getcwd(vfs, path, sizeof(path));
    if (ret != SUCCESS) {
        printf("pwd: error resolving current directory\n");
        return ret;
    }

    printf("%s\n", path);
    return SUCCESS;
}

int cmd_cd(vfs_t* vfs, int argc, char** argv) {
    if (argc != 2) {
        printf("Usage: cd <path>\n");
        return ERROR_INVALID;
    }
    int ret = vfs_cd(vfs, argv[1]);
    if (ret != SUCCESS)
        print_fs_error("cd", ret, argv[1]);
    return ret;
}

int cmd_mkdir(vfs_t* vfs, int argc, char** argv) {
    if (argc != 2) {
        printf("Usage: mkdir <dir>\n");
        return ERROR_INVALID;
    }
    int ret = vfs_mkdir(vfs, argv[1], 0755);
    if (ret != SUCCESS)
        print_fs_error("mkdir", ret, argv[1]);
    return ret;
}

int cmd_rmdir(vfs_t* vfs, int argc, char** argv) {
    if (argc != 2) {
        printf("Usage: rmdir <dir>\n");
        return ERROR_INVALID;
    }
    int ret = vfs_rmdir(vfs, argv[1]);
    if (ret != SUCCESS)
        print_fs_error("rmdir", ret, argv[1]);
    return ret;
}

int cmd_touch(vfs_t* vfs, int argc, char** argv) {
    if (argc != 2) {
        printf("Usage: touch <file>\n");
        return ERROR_INVALID;
    }
    int ret = vfs_create(vfs, argv[1], 0644);
    if (ret != SUCCESS)
        print_fs_error("touch", ret, argv[1]);
    return ret;
}

int cmd_rm(vfs_t* vfs, int argc, char** argv) {
    if (argc != 2) {
        printf("Usage: rm <file>\n");
        return ERROR_INVALID;
    }
    int ret = vfs_rm(vfs, argv[1]);
    if (ret != SUCCESS)
        print_fs_error("rm", ret, argv[1]);
    return ret;
}

int cmd_cp(vfs_t* vfs, int argc, char** argv) {
    if (argc != 3) { printf("Usage: cp <src> <dst>\n"); return ERROR_INVALID; }
    int ret = vfs_cp(vfs, argv[1], argv[2]);
    if (ret != SUCCESS) print_fs_error("cp", ret, argv[1]);
    return ret;
}

int cmd_mv(vfs_t* vfs, int argc, char** argv) {
    if (argc != 3) { printf("Usage: mv <src> <dst>\n"); return ERROR_INVALID; }
    int ret = vfs_mv(vfs, argv[1], argv[2]);
    if (ret != SUCCESS) print_fs_error("mv", ret, argv[1]);
    return ret;
}

int cmd_cat(vfs_t* vfs, int argc, char** argv) {
    if (argc != 2) {
        printf("Usage: cat <file>\n");
        return ERROR_INVALID;
    }

    open_file_t* f;
    int ret = vfs_open(vfs, argv[1], FS_O_RDONLY, &f);
    if (ret != SUCCESS) {
        printf("cat: cannot open %s\n", argv[1]);
        return ret;
    }

    char buf[1024];
    size_t bytes_read = 0;

    while (1) {
        int ret = vfs_read(f, buf, sizeof(buf) - 1, &bytes_read);
        if (ret != SUCCESS) {
            print_fs_error("cat", ret, argv[1]);
            vfs_close(f);
            return ret;
        }

        if (bytes_read == 0) break;

        fwrite(buf, 1, bytes_read, stdout);
    }

    printf("\n");

    vfs_close(f);
    return SUCCESS;
}

int cmd_write(vfs_t* vfs, int argc, char** argv) {
    if (argc != 3) {
        printf("Usage: write <file> \"text\"\n");
        return ERROR_INVALID;
    }

    open_file_t* f;
    int ret = vfs_open(vfs, argv[1], FS_O_WRONLY | FS_O_TRUNC | FS_O_CREAT, &f);
    if (ret != SUCCESS) {
        printf("write: cannot open %s\n", argv[1]);
        return ret;
    }

    size_t w;
    ret = vfs_write(f, argv[2], strlen(argv[2]), &w);
    if (ret != SUCCESS) {
        print_fs_error("write", ret, argv[1]);
    }
    
    vfs_close(f);
    return ret;
}

int cmd_append(vfs_t* vfs, int argc, char** argv) {
    if (argc != 3) {
        printf("Usage: append <file> \"text\"\n");
        return ERROR_INVALID;
    }

    open_file_t* f;
    int ret = vfs_open(vfs, argv[1], FS_O_WRONLY | FS_O_APPEND | FS_O_CREAT, &f);
    if (ret != SUCCESS) {
        printf("append: cannot open %s\n", argv[1]);
        return ret;
    }

    size_t w;
    ret = vfs_write(f, argv[2], strlen(argv[2]), &w);
    if (ret != SUCCESS) {
        print_fs_error("append", ret, argv[1]);
    }
    
    vfs_close(f);
    return ret;
}

int cmd_ls(vfs_t* vfs, int argc, char** argv) {
    if (argc > 2) {
        printf("Usage: ls [path]\n");
        return ERROR_INVALID;
    }
    const char* path = (argc == 2) ? argv[1] : ".";

    struct dentry* list;
    uint32_t count;

    int ret = vfs_ls(vfs, path, &list, &count);
    if (ret != SUCCESS) {
        print_fs_error("ls", ret, path);
        return ret;
    }

    for (uint32_t i = 0; i < count; i++)
        printf("%s  ", list[i].name);
    printf("\n");

    free(list);
    return SUCCESS;
}

int cmd_ln(vfs_t* vfs, int argc, char** argv) {
    if (argc != 3) {
        printf("Usage: ln <src> <dest>\n");
        return ERROR_INVALID;
    }

    int ret = vfs_link(vfs, argv[1], argv[2]);
    if (ret != SUCCESS)
        printf("ln: cannot link %s -> %s: %s\n", argv[1], argv[2], fs_error_to_string(ret));
    return ret;
}

int cmd_stat(vfs_t* vfs, int argc, char** argv) {
    if (argc != 2) {
        printf("Usage: stat <path>\n");
        return ERROR_INVALID;
    }

    struct inode st;
    uint32_t inode_num;
    char abs_path[MAX_PATH];

    int ret = vfs_stat(vfs, argv[1], &st, &inode_num, abs_path, sizeof(abs_path));
    if (ret != SUCCESS) {
        print_fs_error("stat", ret, argv[1]);
        return ret;
    }
    printf("[DEBUG cmd_stat] argv[1]='%s' inode_num=%u\n", argv[1], inode_num);

    // print inode info
    printf("\n=== STAT ===\n");
    printf("Path          : %s\n", abs_path);
    printf("Type          : %s\n", inode_type_to_string(st.type));
    printf("Inode Number  : %u\n", inode_num);
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
        if (st.direct[i] == 0) continue;
        printf("%u ", st.direct[i]);
    }
    printf("\n");

    if (st.indirect != 0)
        printf("Indirect block  : %u\n", st.indirect);
    else
        printf("Indirect block  : (none)\n");

    printf("==============\n\n");
    return SUCCESS;
}

// df (was fsinfo)
int cmd_df(vfs_t* vfs, int argc, char** argv) {
    (void)argc;
    (void)argv;
    vfs_df(vfs);
    return SUCCESS;
}
