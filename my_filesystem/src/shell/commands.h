#pragma once
#include "fs.h"

// filesystem lifecycle
int cmd_format(int argc, char** argv);
int cmd_mount(int argc, char** argv, filesystem_t** fs_p);
int cmd_unmount(filesystem_t** fs_p);

// directories
int cmd_mkdir(filesystem_t* fs, int argc, char** argv);
int cmd_rmdir(filesystem_t* fs, int argc, char** argv);
int cmd_cd(filesystem_t* fs, int argc, char** argv);
int cmd_pwd(filesystem_t* fs, int argc, char** argv);

// files 
int cmd_touch(filesystem_t* fs, int argc, char** argv);
int cmd_rm(filesystem_t* fs, int argc, char** argv);
int cmd_cat(filesystem_t* fs, int argc, char** argv);
int cmd_write(filesystem_t* fs, int argc, char** argv);
int cmd_append(filesystem_t* fs, int argc, char** argv);

// listing 
int cmd_ls(filesystem_t* fs, int argc, char** argv);

// links 
int cmd_ln(filesystem_t* fs, int argc, char** argv);

// metadata 
int cmd_stat(filesystem_t* fs, int argc, char** argv);
int cmd_fsinfo(filesystem_t* fs);
