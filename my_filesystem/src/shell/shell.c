#include "shell.h"
#include "commands.h"
#include "vfs.h"
#include "fs.h"
#include "disk.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>

#define MAX_LINE 4096
#define MAX_ARGS 32

static void shell_print_prompt(vfs_t* vfs) {
    if (vfs->count == 0) {
        printf("\033[1;33m[no-mount]$\033[0m ");
        return;
    }

    char path_buf[MAX_PATH];
    if (vfs_getcwd(vfs, path_buf, sizeof(path_buf)) != SUCCESS) {
        strcpy(path_buf, "?");
    }

    filesystem_t* fs = NULL;
    char local_path[MAX_PATH];
    char abs_path[MAX_PATH];

    int ret = vfs_resolve_path(vfs, path_buf, &fs, local_path, sizeof(local_path), abs_path, sizeof(abs_path));

    if (ret == SUCCESS && fs != NULL) {
        const char* disk_name = disk_get_filename(fs->disk); 
        printf("\033[1;32m[%s@%s]$\033[0m ", disk_name, path_buf);
    } else {
        printf("\033[1;32m[vfs:%s]$\033[0m ", path_buf);
    }
}

void shell_run(void) {
    char* line = NULL;
    size_t len = 0;
    char* argv[MAX_ARGS];
    
    vfs_t my_vfs;
    vfs_init(&my_vfs);

    if (isatty(STDIN_FILENO)) {
        printf("\nWhatTheShell v2.0\n");
        printf("Type 'help' to get available commands.\n");
        printf("Type 'exit' to quit.\n");
        printf("\n");
    }

    while (1) {
        if (isatty(STDIN_FILENO)) {
            shell_print_prompt(&my_vfs);
            fflush(stdout);   // ensure prompt is visible before blocking on input
            }
        ssize_t nread = getline(&line, &len, stdin);
        if (nread == -1) // EOF or error
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

        int status = shell_dispatch(&my_vfs, argc, argv);
        if (status == SHELL_EXIT)
            break;
    }

    free(line);
    vfs_destroy(&my_vfs);
}

typedef int (*cmd_fn_t)(vfs_t*, int, char**);

typedef struct {
    const char* name;
    cmd_fn_t    fn;
} cmd_entry_t;

/* --- handlers for commands that do not require a mounted filesystem --- */

static int handle_exit(vfs_t* vfs, int argc, char** argv) {
    (void)vfs; (void)argc; (void)argv;
    return SHELL_EXIT;
}

static int handle_help(vfs_t* vfs, int argc, char** argv) {
    (void)vfs; (void)argc; (void)argv;
    printf("Available commands:\n");
    printf("  format <diskname> <size_in_bytes>\n");
    printf("  mount <diskname> <mount_path>\n");
    printf("  unmount <mount_path>\n");
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
    printf("  cp <src> <dst>\n");
    printf("  mv <src> <dst>\n");
    printf("  stat <path>\n");
    printf("  fsinfo\n");
    printf("  cat <file>\n");
    printf("  help\n");
    printf("  exit\n");
    return 0;
}

/* --- dispatch table --- */

static const cmd_entry_t shell_cmds[] = {
    { "exit",    handle_exit },
    { "help",    handle_help },
    { "format",  cmd_format  },
    { "mount",   cmd_mount   },
    { "unmount", cmd_unmount },
    { "pwd",     cmd_pwd     },
    { "cd",      cmd_cd      },
    { "ls",      cmd_ls      },
    { "touch",   cmd_touch   },
    { "write",   cmd_write   },
    { "append",  cmd_append  },
    { "cat",     cmd_cat     },
    { "rm",      cmd_rm      },
    { "mkdir",   cmd_mkdir   },
    { "rmdir",   cmd_rmdir   },
    { "ln",      cmd_ln      },
    { "cp",      cmd_cp      },
    { "mv",      cmd_mv      },
    { "chmod",   cmd_chmod   },
    { "stat",    cmd_stat    },
    { "df",      cmd_df      },
    { NULL, NULL }
};

int shell_dispatch(vfs_t* vfs, int argc, char** argv) {
    const char* cmd = argv[0];

    for (int i = 0; shell_cmds[i].name != NULL; i++) {
        if (strcmp(cmd, shell_cmds[i].name) == 0) {
            return shell_cmds[i].fn(vfs, argc, argv);
        }
    }

    printf("Unknown command: '%s'. Type 'help' for available commands.\n", cmd);
    return 0;
}