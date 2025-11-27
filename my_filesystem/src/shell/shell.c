#include "shell.h"
#include "commands.h"
#include "disk.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#define MAX_LINE 1024
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
    char line[MAX_LINE];
    char* argv[MAX_ARGS];
    filesystem_t* current_fs = NULL;
    
    printf("\nWhatTheShell v1.0\n");
    printf("Type 'help' to get available commands.\n");
    printf("Type 'exit' to quit.\n");
    printf("\n");

    while (1) {

        shell_print_prompt(current_fs);

        if (!fgets(line, sizeof(line), stdin))
            break;

        trim_newline(line);

        int argc = parse_line(line, argv, MAX_ARGS);
        if (argc == 0)
            continue;

        int status = shell_dispatch(&current_fs, argc, argv);

        if (status == SHELL_EXIT)
            break;
    }

    if (current_fs) {
        fs_unmount(current_fs);
    }
}

int shell_dispatch(filesystem_t** current_fs, int argc, char** argv) {
    const char* cmd = argv[0];

    //printf("argc: %d\n", argc);
    //for (int i = 0; i < argc; i++)
    //    printf("argv[%d] = '%s'\n", i, argv[i]);


    if (strcmp(cmd, "exit") == 0)
        return SHELL_EXIT;

    if (strcmp(cmd, "help") == 0) {
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
        printf("  help\n");
        printf("  exit\n");
        return 0;
    }

    if (strcmp(cmd, "format") == 0) {
        if (*current_fs != NULL) {
            printf("format: cannot format while a filesystem is mounted.\n");
            printf("Please run 'unmount' first.\n");
            return 0;
        }
        return cmd_format(argc, argv);
    }

    if (strcmp(cmd, "mount") == 0)
        return cmd_mount(argc, argv, current_fs);

    if (strcmp(cmd, "unmount") == 0)
        return cmd_unmount(current_fs);

    // all other commands need a mounted filesystem
    if (*current_fs == NULL) {
        printf("Error: no filesystem mounted.\n");
        return 0;
    }

    if (strcmp(cmd, "pwd") == 0)     return cmd_pwd(*current_fs, argc, argv);
    if (strcmp(cmd, "cd") == 0)      return cmd_cd(*current_fs, argc, argv);
    if (strcmp(cmd, "ls") == 0)      return cmd_ls(*current_fs, argc, argv);

    if (strcmp(cmd, "touch") == 0)   return cmd_touch(*current_fs, argc, argv);
    if (strcmp(cmd, "write") == 0)   return cmd_write(*current_fs, argc, argv);
    if (strcmp(cmd, "append") == 0)  return cmd_append(*current_fs, argc, argv);
    if (strcmp(cmd, "rm") == 0)      return cmd_rm(*current_fs, argc, argv);
    if (strcmp(cmd, "cat") == 0)     return cmd_cat(*current_fs, argc, argv);

    if (strcmp(cmd, "mkdir") == 0)   return cmd_mkdir(*current_fs, argc, argv);
    if (strcmp(cmd, "rmdir") == 0)   return cmd_rmdir(*current_fs, argc, argv);

    if (strcmp(cmd, "ln") == 0)      return cmd_ln(*current_fs, argc, argv);
    if (strcmp(cmd, "stat") == 0)    return cmd_stat(*current_fs, argc, argv);

    if (strcmp(cmd, "fsinfo") == 0)  return cmd_fsinfo(*current_fs);

    printf("Unknown command: %s\n", cmd);
    return 0;
}