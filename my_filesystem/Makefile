CC = gcc
CFLAGS = -Wall -Wextra -std=gnu99 -g -Iinclude
CFLAGS += -fsanitize=address -g
LDFLAGS += -fsanitize=address
SRCDIR = src
TESTDIR = tests
BUILDDIR = build

# === COMMON HEADER GROUPS ===
CONFIG_HEADER = include/config.h
COMMON_HEADERS = include/common.h $(CONFIG_HEADER)

# === SOURCE MODULES ===
DISK_SRC = $(SRCDIR)/disk/disk.c
DISK_OBJ = $(BUILDDIR)/disk.o

COMMON_SRC = $(SRCDIR)/utils/common.c
COMMON_OBJ = $(BUILDDIR)/common.o

BITMAP_SRC = $(SRCDIR)/utils/bitmap.c
BITMAP_OBJ = $(BUILDDIR)/bitmap.o

PATH_SRC = $(SRCDIR)/utils/path.c
PATH_OBJ = $(BUILDDIR)/path.o

SUPERBLOCK_SRC = $(SRCDIR)/filesystem/superblock.c
SUPERBLOCK_OBJ = $(BUILDDIR)/superblock.o

INODE_SRC = $(SRCDIR)/filesystem/inode.c
INODE_OBJ = $(BUILDDIR)/inode.o

DENTRY_SRC = $(SRCDIR)/filesystem/dentry.c
DENTRY_OBJ = $(BUILDDIR)/dentry.o

# === FS MODULE ===
FS_SRCS = $(SRCDIR)/filesystem/fs_mount.c \
          $(SRCDIR)/filesystem/fs_io.c \
          $(SRCDIR)/filesystem/fs_dir.c \
          $(SRCDIR)/filesystem/fs_file.c \
          $(SRCDIR)/filesystem/fs_path.c \
          $(SRCDIR)/filesystem/fs_stat.c

FS_OBJS = $(BUILDDIR)/fs_mount.o \
          $(BUILDDIR)/fs_io.o \
          $(BUILDDIR)/fs_dir.o \
          $(BUILDDIR)/fs_file.o \
          $(BUILDDIR)/fs_path.o \
          $(BUILDDIR)/fs_stat.o

# === VFS MODULE ===
VFS_SRCS = $(SRCDIR)/vfs/vfs_mount.c \
           $(SRCDIR)/vfs/vfs_io.c \
           $(SRCDIR)/vfs/vfs_dir.c \
           $(SRCDIR)/vfs/vfs_file.c \
           $(SRCDIR)/vfs/vfs_path.c \
           $(SRCDIR)/vfs/vfs_stat.c

VFS_OBJS = $(BUILDDIR)/vfs_mount.o \
           $(BUILDDIR)/vfs_io.o \
           $(BUILDDIR)/vfs_dir.o \
           $(BUILDDIR)/vfs_file.o \
           $(BUILDDIR)/vfs_path.o \
           $(BUILDDIR)/vfs_stat.o

# === SHELL MODULE ===
SHELL_SRC = $(SRCDIR)/shell/shell.c
SHELL_OBJ = $(BUILDDIR)/shell.o

COMMANDS_SRC = $(SRCDIR)/shell/commands.c
COMMANDS_OBJ = $(BUILDDIR)/commands.o

PARSER_SRC = $(SRCDIR)/shell/parser.c
PARSER_OBJ = $(BUILDDIR)/parser.o

MAIN_SRC = $(SRCDIR)/main.c
MAIN_BIN = $(BUILDDIR)/my_filesystem

# === DEFAULT TARGET ===
all: dirs $(MAIN_BIN)

run: all
	./$(MAIN_BIN)

# === BASH TEST SUITE ===
test: all
	@echo "======================================"
	@echo "     Running Bash Test Suite          "
	@echo "======================================"
	@for script in $(TESTDIR)/*.sh; do \
		if [ -f "$$script" ]; then \
			echo ">>> Running $$script..."; \
			chmod +x "$$script"; \
			./$$script; \
			echo ""; \
		fi \
	done
	@echo "=== All tests finished ==="

# === DIRECTORY CREATION ===
dirs:
	@mkdir -p $(BUILDDIR)

# === COMPILE OBJECT FILES ===
$(DISK_OBJ): $(DISK_SRC) $(SRCDIR)/disk/disk.h $(CONFIG_HEADER)
	@echo "Compiling disk module..."
	@$(CC) $(CFLAGS) -I$(SRCDIR)/disk -c $< -o $@

$(COMMON_OBJ): $(COMMON_SRC) $(COMMON_HEADERS)
	@echo "Compiling common module..."
	@$(CC) $(CFLAGS) -c $< -o $@

$(BITMAP_OBJ): $(BITMAP_SRC) $(SRCDIR)/utils/bitmap.h $(COMMON_HEADERS)
	@echo "Compiling bitmap module..."
	@$(CC) $(CFLAGS) -I$(SRCDIR)/utils -c $< -o $@

$(PATH_OBJ): $(PATH_SRC) $(SRCDIR)/utils/path.h $(COMMON_HEADERS)
	@echo "Compiling path module..."
	@$(CC) $(CFLAGS) -I$(SRCDIR)/utils -c $< -o $@

$(SUPERBLOCK_OBJ): $(SUPERBLOCK_SRC) $(SRCDIR)/filesystem/superblock.h $(COMMON_HEADERS)
	@echo "Compiling superblock module..."
	@$(CC) $(CFLAGS) -I$(SRCDIR)/filesystem -I$(SRCDIR)/disk -c $< -o $@

$(INODE_OBJ): $(INODE_SRC) $(SRCDIR)/filesystem/inode.h $(SRCDIR)/utils/bitmap.h $(COMMON_HEADERS)
	@echo "Compiling inode module..."
	@$(CC) $(CFLAGS) -I$(SRCDIR)/filesystem -I$(SRCDIR)/utils -I$(SRCDIR)/disk -c $< -o $@

$(DENTRY_OBJ): $(DENTRY_SRC) $(SRCDIR)/filesystem/dentry.h $(SRCDIR)/filesystem/inode.h $(COMMON_HEADERS)
	@echo "Compiling dentry module..."
	@$(CC) $(CFLAGS) -I$(SRCDIR)/filesystem -I$(SRCDIR)/utils -I$(SRCDIR)/disk -c $< -o $@

# FS Objects
FS_INCLUDES = -I$(SRCDIR)/filesystem -I$(SRCDIR)/utils -I$(SRCDIR)/disk
FS_HEADERS = $(SRCDIR)/filesystem/fs.h $(SRCDIR)/filesystem/fs_internal.h $(COMMON_HEADERS)

$(BUILDDIR)/fs_mount.o: $(SRCDIR)/filesystem/fs_mount.c $(FS_HEADERS)
	@echo "Compiling fs_mount..."
	@$(CC) $(CFLAGS) $(FS_INCLUDES) -c $< -o $@

$(BUILDDIR)/fs_io.o: $(SRCDIR)/filesystem/fs_io.c $(FS_HEADERS)
	@echo "Compiling fs_io..."
	@$(CC) $(CFLAGS) $(FS_INCLUDES) -c $< -o $@

$(BUILDDIR)/fs_dir.o: $(SRCDIR)/filesystem/fs_dir.c $(FS_HEADERS)
	@echo "Compiling fs_dir..."
	@$(CC) $(CFLAGS) $(FS_INCLUDES) -c $< -o $@

$(BUILDDIR)/fs_file.o: $(SRCDIR)/filesystem/fs_file.c $(FS_HEADERS)
	@echo "Compiling fs_file..."
	@$(CC) $(CFLAGS) $(FS_INCLUDES) -c $< -o $@

$(BUILDDIR)/fs_path.o: $(SRCDIR)/filesystem/fs_path.c $(FS_HEADERS)
	@echo "Compiling fs_path..."
	@$(CC) $(CFLAGS) $(FS_INCLUDES) -c $< -o $@

$(BUILDDIR)/fs_stat.o: $(SRCDIR)/filesystem/fs_stat.c $(FS_HEADERS)
	@echo "Compiling fs_stat..."
	@$(CC) $(CFLAGS) $(FS_INCLUDES) -c $< -o $@

# VFS Objects
VFS_INCLUDES = -I$(SRCDIR)/vfs -I$(SRCDIR)/filesystem -I$(SRCDIR)/utils -I$(SRCDIR)/disk
VFS_HEADERS = $(SRCDIR)/vfs/vfs.h $(COMMON_HEADERS)

$(BUILDDIR)/vfs_mount.o: $(SRCDIR)/vfs/vfs_mount.c $(VFS_HEADERS)
	@echo "Compiling vfs_mount..."
	@$(CC) $(CFLAGS) $(VFS_INCLUDES) -c $< -o $@

$(BUILDDIR)/vfs_io.o: $(SRCDIR)/vfs/vfs_io.c $(VFS_HEADERS)
	@echo "Compiling vfs_io..."
	@$(CC) $(CFLAGS) $(VFS_INCLUDES) -c $< -o $@

$(BUILDDIR)/vfs_dir.o: $(SRCDIR)/vfs/vfs_dir.c $(VFS_HEADERS)
	@echo "Compiling vfs_dir..."
	@$(CC) $(CFLAGS) $(VFS_INCLUDES) -c $< -o $@

$(BUILDDIR)/vfs_file.o: $(SRCDIR)/vfs/vfs_file.c $(VFS_HEADERS)
	@echo "Compiling vfs_file..."
	@$(CC) $(CFLAGS) $(VFS_INCLUDES) -c $< -o $@

$(BUILDDIR)/vfs_path.o: $(SRCDIR)/vfs/vfs_path.c $(VFS_HEADERS)
	@echo "Compiling vfs_path..."
	@$(CC) $(CFLAGS) $(VFS_INCLUDES) -c $< -o $@

$(BUILDDIR)/vfs_stat.o: $(SRCDIR)/vfs/vfs_stat.c $(VFS_HEADERS)
	@echo "Compiling vfs_stat..."
	@$(CC) $(CFLAGS) $(VFS_INCLUDES) -c $< -o $@

# Shell and Commands
$(SHELL_OBJ): $(SHELL_SRC) $(SRCDIR)/shell/shell.h
	@echo "Compiling shell..."
	@$(CC) $(CFLAGS) -I$(SRCDIR)/shell -I$(SRCDIR)/vfs -I$(SRCDIR)/filesystem -I$(SRCDIR)/utils -I$(SRCDIR)/disk -c $< -o $@

$(COMMANDS_OBJ): $(COMMANDS_SRC) $(SRCDIR)/shell/commands.h
	@echo "Compiling commands..."
	@$(CC) $(CFLAGS) -I$(SRCDIR)/shell -I$(SRCDIR)/vfs -I$(SRCDIR)/filesystem -I$(SRCDIR)/utils -I$(SRCDIR)/disk -c $< -o $@

$(PARSER_OBJ): $(PARSER_SRC) $(SRCDIR)/shell/parser.h
	@echo "Compiling parser..."
	@$(CC) $(CFLAGS) -I$(SRCDIR)/shell -c $< -o $@

$(MAIN_BIN): $(MAIN_SRC) $(SHELL_OBJ) $(COMMANDS_OBJ) $(PARSER_OBJ) \
             $(FS_OBJS) $(VFS_OBJS) $(INODE_OBJ) $(DENTRY_OBJ) $(SUPERBLOCK_OBJ) \
             $(PATH_OBJ) $(BITMAP_OBJ) $(DISK_OBJ) $(COMMON_OBJ)
	@echo "Building shell..."
	@$(CC) $(CFLAGS) -I$(SRCDIR)/shell -I$(SRCDIR)/vfs -I$(SRCDIR)/filesystem -I$(SRCDIR)/utils -I$(SRCDIR)/disk \
		$(MAIN_SRC) $(SHELL_OBJ) $(COMMANDS_OBJ) $(PARSER_OBJ) \
		$(FS_OBJS) $(VFS_OBJS) $(INODE_OBJ) $(DENTRY_OBJ) $(SUPERBLOCK_OBJ) \
		$(PATH_OBJ) $(BITMAP_OBJ) $(DISK_OBJ) $(COMMON_OBJ) -o $@ $(LDFLAGS)

# === CLEANUP ===
clean:
	@rm -rf $(BUILDDIR) *.img *.log cmds_*.txt
	@echo "Cleaned!"

# === HELP ===
help:
	@echo "Available targets:"
	@echo "  make         - Build main executable"
	@echo "  make run     - Build and run main executable"
	@echo "  make test    - Run automated bash test suite"
	@echo "  make clean   - Clean build files, images and temp files"
	@echo "  make help    - Show this help"

.PHONY: all test run clean dirs help