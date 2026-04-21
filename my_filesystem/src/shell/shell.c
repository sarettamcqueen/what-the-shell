#include "shell.h"
#include "commands.h"
#include "disk.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

#define MAX_LINE 4096
#define MAX_ARGS 32

static void shell_print_prompt(filesystem_t* fs) {
    if (!fs) {
        printf("[no-mount]$ ");
        return;
    }

    char path_buf[MAX_PATH];
    if (fs_inode_to_path(fs, fs->current_dir_inode, path_buf, sizeof(path_buf)) != SUCCESS) {
        strcpy(path_buf, "?");
    }

    printf("[%s:%s]$ ", disk_get_filename(fs->disk), path_buf);
}

void shell_run(void) {
    char* line = NULL;
    size_t len = 0;
    char* argv[MAX_ARGS];
    filesystem_t* current_fs = NULL;

    printf("\nWhatTheShell v1.0\n");
    printf("Type 'help' to get available commands.\n");
    printf("Type 'exit' to quit.\n");
    printf("\n");

    while (1) {
        shell_print_prompt(current_fs);
        fflush(stdout);   // ensure prompt is visible before blocking on input

        ssize_t nread = getline(&line, &len, stdin);
        if (nread == -1)   /* EOF o errore */
            break;

        // reject lines that are unreasonably long
        if (nread > MAX_LINE) {
            printf("error: input line too long (max %d characters)\n", MAX_LINE);
            // discard whatever getline allocated and reset for next iteration
            free(line);
            line = NULL;
            len  = 0;
            continue;
        }

        trim_newline(line);

        int argc = parse_line(line, argv, MAX_ARGS);
        if (argc == 0)
            continue;

        if (argc < 0) {
            printf("error: malformed input (unclosed quote or too many tokens)\n");
            continue;
        }

        int status = shell_dispatch(&current_fs, argc, argv);
        if (status == SHELL_EXIT)
            break;
    }

    free(line);

    if (current_fs)
        fs_unmount(current_fs);
}

/* type for commands that do NOT require a mounted filesystem */
typedef int (*cmd_no_fs_fn)(filesystem_t**, int, char**);

/* type for commands that require a mounted filesystem */
typedef int (*cmd_with_fs_fn)(filesystem_t*, int, char**);

typedef struct {
    const char*    name;
    cmd_no_fs_fn   fn;
} cmd_no_fs_entry_t;

typedef struct {
    const char*    name;
    cmd_with_fs_fn fn;
} cmd_with_fs_entry_t;

/* --- handlers for commands that do not require a mounted filesystem --- */

static int handle_exit(filesystem_t** fs, int argc, char** argv) {
    (void)fs; (void)argc; (void)argv;
    return SHELL_EXIT;
}

static int handle_help(filesystem_t** fs, int argc, char** argv) {
    (void)fs; (void)argc; (void)argv;
    printf("Available commands:\n");
    printf("  format <diskname> <size_in_bytes>\n");
    printf("  mount <diskname>\n");
    printf("  unmount\n");
    printf("  pwd\n");
    printf("  cd <path>\n");
    printf("  ls [path]\n");
    printf("  touch <file>\n");
    printf("  write <file> \"text\"\n");
    printf("  append <file> \"text\"\n");
    printf("  rm <file>\n");
    printf("  mkdir <dir>\n");
    printf("  rmdir <dir>\n");
    printf("  ln <src> <dst>\n");
    printf("  stat <path>\n");
    printf("  fsinfo\n");
    printf("  cat <file>\n");
    printf("  help\n");
    printf("  exit\n");
    return 0;
}

static int handle_format(filesystem_t** fs, int argc, char** argv) {
    if (*fs != NULL) {
        printf("format: cannot format while a filesystem is mounted.\n");
        printf("Please run 'unmount' first.\n");
        return 0;
    }
    return cmd_format(argc, argv);
}

static int handle_mount(filesystem_t** fs, int argc, char** argv) {
    return cmd_mount(argc, argv, fs);
}

static int handle_unmount(filesystem_t** fs, int argc, char** argv) {
    (void)argc; (void)argv;
    return cmd_unmount(fs);
}

// wrapper needed
static int handle_fsinfo(filesystem_t* fs, int argc, char** argv) {
    (void)argc; (void)argv;
    return cmd_fsinfo(fs);
}

/* --- dispatch tables --- */

static const cmd_no_fs_entry_t cmds_no_fs[] = {
    { "exit",    handle_exit    },
    { "help",    handle_help    },
    { "format",  handle_format  },
    { "mount",   handle_mount   },
    { "unmount", handle_unmount },
    { NULL, NULL }
};

static const cmd_with_fs_entry_t cmds_with_fs[] = {
    { "pwd",    cmd_pwd    },
    { "cd",     cmd_cd     },
    { "ls",     cmd_ls     },
    { "touch",  cmd_touch  },
    { "write",  cmd_write  },
    { "append", cmd_append },
    { "rm",     cmd_rm     },
    { "cat",    cmd_cat    },
    { "mkdir",  cmd_mkdir  },
    { "rmdir",  cmd_rmdir  },
    { "ln",     cmd_ln     },
    { "stat",   cmd_stat   },
    { "fsinfo", handle_fsinfo }, // wrapper needed: cmd_fsinfo only takes fs
    { NULL, NULL }
};

int shell_dispatch(filesystem_t** current_fs, int argc, char** argv) {
    const char* cmd = argv[0];

    // look for the command among those that do not require a mounted fs
    for (int i = 0; cmds_no_fs[i].name != NULL; i++) {
        if (strcmp(cmd, cmds_no_fs[i].name) == 0)
            return cmds_no_fs[i].fn(current_fs, argc, argv);
    }

    // look for the command among those that require a mounted fs
    for (int i = 0; cmds_with_fs[i].name != NULL; i++) {
        if (strcmp(cmd, cmds_with_fs[i].name) == 0) {
            // valid command: now check whether a fs is actually mounted
            if (*current_fs == NULL) {
                printf("Error: '%s' requires a mounted filesystem.\n", cmd);
                printf("Use 'mount <diskname>' first.\n");
                return 0;
            }
            return cmds_with_fs[i].fn(*current_fs, argc, argv);
        }
    }

    // command not found in either table
    printf("Unknown command: '%s'. Type 'help' for available commands.\n", cmd);
    return 0;
}