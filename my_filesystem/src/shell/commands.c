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

// helper: if 'dst' is a directory, builds 'dst/basename(src)'
static int shell_build_dest_path(vfs_t* vfs, const char* src, const char* dst, char* final_dst) {
    strncpy(final_dst, dst, MAX_PATH);

    filesystem_t* target_fs = NULL;
    char local_path[MAX_PATH];
    char abs_path[MAX_PATH];
    
    // resolve dest path to see if it exists
    int ret = vfs_resolve_path(vfs, dst, &target_fs, local_path, sizeof(local_path), abs_path, sizeof(abs_path));
    
    if (ret == SUCCESS && target_fs != NULL) {
        // if it's a directory, append filename
        struct inode st;
        uint32_t target_inode;
        if (fs_stat(target_fs, local_path, &st, &target_inode, NULL, 0) == SUCCESS) {
            if (st.type == INODE_TYPE_DIRECTORY) {
                char* base_name = path_get_basename(src);
                if (base_name != NULL) {
                    size_t dst_len = strlen(dst);
                    int written;

                    if (dst_len > 0 && dst[dst_len - 1] == '/') {
                        written = snprintf(final_dst, MAX_PATH, "%s%s", dst, base_name);
                    } else {
                        written = snprintf(final_dst, MAX_PATH, "%s/%s", dst, base_name);
                    }

                    free(base_name);

                    if (written < 0 || written >= MAX_PATH) {
                        printf("cp: resulting path is too long\n");
                        return ERROR_GENERIC;
                    }
                }
            }
        }
    }
    return SUCCESS;
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

// format <diskname> <size>
int cmd_format(vfs_t* vfs, int argc, char** argv) {
    (void)vfs;
    if (argc != 3) {
        printf("Usage: format <diskname> <size_in_bytes>\n");
        return ERROR_INVALID;
    }

    const char* filename = argv[1];
    int input_size = atoi(argv[2]);

    if (input_size <= 0) {
        printf("format: invalid size '%s'\n", argv[2]);
        return ERROR_INVALID;
    }

    int check = vfs_check_format(vfs, filename);
    if (check != SUCCESS) {
        print_fs_error("format", check, filename);
        return check;
    }

    int remainder = input_size % BLOCK_SIZE;
    long long aligned_size = input_size;

    if (remainder != 0) {
        aligned_size = input_size + (BLOCK_SIZE - remainder);
        printf("format: size %d is not aligned to %d bytes, rounding up to %lld\n",
               input_size, BLOCK_SIZE, aligned_size);
    }

    disk_t disk;
    int ret = disk_attach(filename, aligned_size, true, &disk);
    if (ret != DISK_SUCCESS) {
        printf("format: cannot attach %s\n", filename);
        return ERROR_IO;
    }

    uint64_t total_bytes = (uint64_t)aligned_size;
    uint32_t total_inodes = total_bytes / BYTES_PER_INODE;
    uint32_t total_blocks = total_bytes / BLOCK_SIZE;

    // round up to next multiple of INODES_PER_BLOCK
    if (total_inodes % INODES_PER_BLOCK != 0) {
        total_inodes += INODES_PER_BLOCK - (total_inodes % INODES_PER_BLOCK);
    }
    if (total_inodes < MIN_INODES) total_inodes = MIN_INODES;

    ret = fs_format(disk, total_blocks, total_inodes);
    if (ret != SUCCESS) {
        printf("format: failed to format '%s'\n", filename);
        disk_detach(disk);
        return ret;
    }

    printf("Filesystem '%s' formatted (%lld bytes, %d blocks, %u inodes)\n",
           filename, aligned_size, total_blocks, total_inodes);

    disk_detach(disk);
    return SUCCESS;
}

// mount
int cmd_mount(vfs_t* vfs, int argc, char** argv) {
    if (argc != 3) {
        printf("Usage: mount <disk.img> <mount_path>\n");
        return ERROR_INVALID;
    }

    char* filename = argv[1];
    char* mount_path = argv[2];

    int check = vfs_check_mount(vfs, mount_path, filename);
    if (check != SUCCESS) {
        if (check == ERROR_ROOT_REQUIRED) {
            print_fs_error("mount", check, mount_path);
        } else if (check == ERROR_EXISTS) {
            print_fs_error("mount", check, mount_path);
        } else if (check == ERROR_NOT_FOUND) {
            printf("mount: mount point '%s' does not exist.\n", mount_path);
        } else {
            printf("mount: invalid operation. Disk might be mounted already or path is not a directory.\n");
        }
        return check;
    }

    disk_t disk;
    int ret = disk_attach(filename, 0, false, &disk);
    if (ret != DISK_SUCCESS) {
        printf("mount: cannot open disk '%s'\n", filename);
        return ERROR_IO;
    }

    filesystem_t* fs = NULL;
    ret = fs_mount(disk, &fs);
    if (ret != SUCCESS) {
        printf("mount: failed to initialize filesystem on '%s'\n", filename);
        disk_detach(disk);
        return ret;
    }

    ret = vfs_mount(vfs, mount_path, fs);
    if (ret != SUCCESS) {
        print_fs_error("mount", ret, mount_path);
        fs_unmount(fs);
        return ret;
    }

    printf("Mounted %s on %s\n", filename, mount_path);
    return SUCCESS;
}

// unmount
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

// pwd
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

// cd
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

// mkdir
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

// rmdir
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

// touch
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

// rm
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

//cp
int cmd_cp(vfs_t* vfs, int argc, char** argv) {
    if (argc != 3) {
        printf("Usage: cp <src> <dst>\n");
        return ERROR_INVALID;
    }

    char* src = argv[1];
    char* dst = argv[2];
    char final_dst[MAX_PATH];

    int ret = shell_build_dest_path(vfs, src, dst, final_dst);
    if (ret != SUCCESS) return ret;

    ret = vfs_cp(vfs, src, final_dst);

    if (ret != SUCCESS) {
        print_fs_error("cp", ret, src);
    }
    
    return ret;
}

// mv
int cmd_mv(vfs_t* vfs, int argc, char** argv) {
    if (argc != 3) {
        printf("Usage: mv <src> <dest>\n");
        return ERROR_INVALID;
    }

    char* src = argv[1];
    char* dst = argv[2];
    char final_dst[MAX_PATH];

    int ret = shell_build_dest_path(vfs, src, dst, final_dst);
    if (ret != SUCCESS) return ret;

    ret = vfs_mv(vfs, src, final_dst);
    if (ret != SUCCESS)
        print_fs_error("mv", ret, src);
    return ret;
}

// cat
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

// write
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

// append
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

// ls
int cmd_ls(vfs_t* vfs, int argc, char** argv) {
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

// ln
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

// stat
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
