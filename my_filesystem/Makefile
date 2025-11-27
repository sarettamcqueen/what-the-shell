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

# disk module
DISK_SRC = $(SRCDIR)/disk/disk.c
DISK_OBJ = $(BUILDDIR)/disk.o

# common/utils module
COMMON_SRC = $(SRCDIR)/utils/common.c
COMMON_OBJ = $(BUILDDIR)/common.o

# bitmap module
BITMAP_SRC = $(SRCDIR)/utils/bitmap.c
BITMAP_OBJ = $(BUILDDIR)/bitmap.o

# path module
PATH_SRC = $(SRCDIR)/utils/path.c
PATH_OBJ = $(BUILDDIR)/path.o

# superblock module
SUPERBLOCK_SRC = $(SRCDIR)/filesystem/superblock.c
SUPERBLOCK_OBJ = $(BUILDDIR)/superblock.o

# inode module
INODE_SRC = $(SRCDIR)/filesystem/inode.c
INODE_OBJ = $(BUILDDIR)/inode.o

# dentry module
DENTRY_SRC = $(SRCDIR)/filesystem/dentry.c
DENTRY_OBJ = $(BUILDDIR)/dentry.o

# fs module
FS_SRC = $(SRCDIR)/filesystem/fs.c
FS_OBJ = $(BUILDDIR)/fs.o

# shell module
SHELL_SRC = $(SRCDIR)/shell/shell.c
SHELL_OBJ = $(BUILDDIR)/shell.o

# commands module
COMMANDS_SRC = $(SRCDIR)/shell/commands.c
COMMANDS_OBJ = $(BUILDDIR)/commands.o

# parser module
PARSER_SRC = $(SRCDIR)/shell/parser.c
PARSER_OBJ = $(BUILDDIR)/parser.o

# main program
MAIN_SRC = $(SRCDIR)/main.c
MAIN_BIN = $(BUILDDIR)/my_filesystem

# === TEST BINARIES ===

TEST_DISK_SRC = $(TESTDIR)/test_disk.c
TEST_DISK_BIN = $(BUILDDIR)/test_disk

TEST_COMMON_SRC = $(TESTDIR)/test_common.c
TEST_COMMON_BIN = $(BUILDDIR)/test_common

TEST_BITMAP_SRC = $(TESTDIR)/test_bitmap.c
TEST_BITMAP_BIN = $(BUILDDIR)/test_bitmap

TEST_SUPERBLOCK_SRC = $(TESTDIR)/test_superblock.c
TEST_SUPERBLOCK_BIN = $(BUILDDIR)/test_superblock

TEST_INODE_SRC = $(TESTDIR)/test_inode.c
TEST_INODE_BIN = $(BUILDDIR)/test_inode

TEST_DENTRY_SRC = $(TESTDIR)/test_dentry.c
TEST_DENTRY_BIN = $(BUILDDIR)/test_dentry

TEST_PATH_SRC = $(TESTDIR)/test_path.c
TEST_PATH_BIN = $(BUILDDIR)/test_path

TEST_FS_SRC = $(TESTDIR)/test_fs.c
TEST_FS_BIN = $(BUILDDIR)/test_fs

# === ALL TESTS ===

ALL_TESTS = $(TEST_DISK_BIN) $(TEST_COMMON_BIN) $(TEST_BITMAP_BIN) \
            $(TEST_SUPERBLOCK_BIN) $(TEST_INODE_BIN) $(TEST_DENTRY_BIN) \
            $(TEST_PATH_BIN) $(TEST_FS_BIN)

# === DEFAULT TARGET ===

all: dirs $(ALL_TESTS)
	@echo "=== Running test_disk ==="
	@./$(TEST_DISK_BIN)
	@echo ""
	@echo "=== Running test_common ==="
	@./$(TEST_COMMON_BIN)
	@echo ""
	@echo "=== Running test_bitmap ==="
	@./$(TEST_BITMAP_BIN)
	@echo ""
	@echo "=== Running test_superblock ==="
	@./$(TEST_SUPERBLOCK_BIN)
	@echo ""
	@echo "=== Running test_inode ==="
	@./$(TEST_INODE_BIN)
	@echo ""
	@echo "=== Running test_dentry ==="
	@./$(TEST_DENTRY_BIN)
	@echo ""
	@echo "=== Running test_path ==="
	@./$(TEST_PATH_BIN)
	@echo ""
	@echo "All tests passed!"

# === INDIVIDUAL TEST TARGETS ===

test_disk: dirs $(TEST_DISK_BIN)
	@./$(TEST_DISK_BIN)

test_common: dirs $(TEST_COMMON_BIN)
	@./$(TEST_COMMON_BIN)

test_bitmap: dirs $(TEST_BITMAP_BIN)
	@./$(TEST_BITMAP_BIN)

test_superblock: dirs $(TEST_SUPERBLOCK_BIN)
	@./$(TEST_SUPERBLOCK_BIN)

test_inode: dirs $(TEST_INODE_BIN)
	@./$(TEST_INODE_BIN)

test_dentry: dirs $(TEST_DENTRY_BIN)
	@./$(TEST_DENTRY_BIN)

test_path: dirs $(TEST_PATH_BIN)
	@./$(TEST_PATH_BIN)

test_fs: dirs $(TEST_FS_BIN)
	@./$(TEST_FS_BIN)

run: dirs $(MAIN_BIN)
	./$(MAIN_BIN)

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

$(FS_OBJ): $(FS_SRC) $(SRCDIR)/filesystem/fs.h $(COMMON_HEADERS)
	@echo "Compiling fs module..."
	@$(CC) $(CFLAGS) -I$(SRCDIR)/filesystem -I$(SRCDIR)/utils -I$(SRCDIR)/disk -c $< -o $@

$(SHELL_OBJ): $(SHELL_SRC) $(SRCDIR)/shell/shell.h
	@echo "Compiling shell..."
	@$(CC) $(CFLAGS) -I$(SRCDIR)/shell -I$(SRCDIR)/filesystem -I$(SRCDIR)/utils -I$(SRCDIR)/disk -c $< -o $@

$(COMMANDS_OBJ): $(COMMANDS_SRC) $(SRCDIR)/shell/commands.h
	@echo "Compiling commands..."
	@$(CC) $(CFLAGS) -I$(SRCDIR)/shell -I$(SRCDIR)/filesystem -I$(SRCDIR)/utils -I$(SRCDIR)/disk -c $< -o $@

$(PARSER_OBJ): $(PARSER_SRC) $(SRCDIR)/shell/parser.h
	@echo "Compiling parser..."
	@$(CC) $(CFLAGS) -I$(SRCDIR)/shell -c $< -o $@

# === LINK TEST BINARIES ===

$(TEST_DISK_BIN): $(DISK_OBJ) $(TEST_DISK_SRC)
	@echo "Building test_disk..."
	@$(CC) $(CFLAGS) -I$(SRCDIR)/disk $(TEST_DISK_SRC) $(DISK_OBJ) -o $@

$(TEST_COMMON_BIN): $(COMMON_OBJ) $(TEST_COMMON_SRC)
	@echo "Building test_common..."
	@$(CC) $(CFLAGS) $(TEST_COMMON_SRC) $(COMMON_OBJ) -o $@

$(TEST_BITMAP_BIN): $(BITMAP_OBJ) $(COMMON_OBJ) $(TEST_BITMAP_SRC)
	@echo "Building test_bitmap..."
	@$(CC) $(CFLAGS) -I$(SRCDIR)/utils $(TEST_BITMAP_SRC) $(BITMAP_OBJ) $(COMMON_OBJ) -o $@

$(TEST_SUPERBLOCK_BIN): $(SUPERBLOCK_OBJ) $(DISK_OBJ) $(COMMON_OBJ) $(TEST_SUPERBLOCK_SRC)
	@echo "Building test_superblock..."
	@$(CC) $(CFLAGS) -I$(SRCDIR)/filesystem -I$(SRCDIR)/disk $(TEST_SUPERBLOCK_SRC) \
		$(SUPERBLOCK_OBJ) $(DISK_OBJ) $(COMMON_OBJ) -o $@

$(TEST_INODE_BIN): $(INODE_OBJ) $(SUPERBLOCK_OBJ) $(BITMAP_OBJ) $(DISK_OBJ) $(COMMON_OBJ) $(TEST_INODE_SRC)
	@echo "Building test_inode..."
	@$(CC) $(CFLAGS) -I$(SRCDIR)/filesystem -I$(SRCDIR)/utils -I$(SRCDIR)/disk $(TEST_INODE_SRC) \
		$(INODE_OBJ) $(SUPERBLOCK_OBJ) $(BITMAP_OBJ) $(DISK_OBJ) $(COMMON_OBJ) -o $@

$(TEST_DENTRY_BIN): $(DENTRY_OBJ) $(INODE_OBJ) $(SUPERBLOCK_OBJ) $(BITMAP_OBJ) $(DISK_OBJ) $(COMMON_OBJ) $(TEST_DENTRY_SRC)
	@echo "Building test_dentry..."
	@$(CC) $(CFLAGS) -I$(SRCDIR)/filesystem -I$(SRCDIR)/utils -I$(SRCDIR)/disk $(TEST_DENTRY_SRC) \
		$(DENTRY_OBJ) $(INODE_OBJ) $(SUPERBLOCK_OBJ) $(BITMAP_OBJ) $(DISK_OBJ) $(COMMON_OBJ) -o $@

$(TEST_PATH_BIN): $(PATH_OBJ) $(COMMON_OBJ) $(TEST_PATH_SRC)
	@echo "Building test_path..."
	@$(CC) $(CFLAGS) -I$(SRCDIR)/utils $(TEST_PATH_SRC) $(PATH_OBJ) $(COMMON_OBJ) -o $@

$(TEST_FS_BIN): $(FS_OBJ) $(DISK_OBJ) $(COMMON_OBJ) $(BITMAP_OBJ) \
                $(INODE_OBJ) $(DENTRY_OBJ) $(PATH_OBJ) $(SUPERBLOCK_OBJ) \
                $(TEST_FS_SRC)
	@echo "Building test_fs..."
	@$(CC) $(CFLAGS) -I$(SRCDIR)/filesystem -I$(SRCDIR)/utils -I$(SRCDIR)/disk \
		$(TEST_FS_SRC) $(FS_OBJ) $(DISK_OBJ) $(COMMON_OBJ) $(BITMAP_OBJ) \
		$(INODE_OBJ) $(DENTRY_OBJ) $(PATH_OBJ) $(SUPERBLOCK_OBJ) -o $@

$(MAIN_BIN): $(MAIN_SRC) $(SHELL_OBJ) $(COMMANDS_OBJ) $(PARSER_OBJ) \
             $(FS_OBJ) $(INODE_OBJ) $(DENTRY_OBJ) $(SUPERBLOCK_OBJ) \
             $(PATH_OBJ) $(BITMAP_OBJ) $(DISK_OBJ) $(COMMON_OBJ)
	@echo "Building shell..."
	@$(CC) $(CFLAGS) -I$(SRCDIR)/shell -I$(SRCDIR)/filesystem -I$(SRCDIR)/utils -I$(SRCDIR)/disk \
		$(MAIN_SRC) $(SHELL_OBJ) $(COMMANDS_OBJ) $(PARSER_OBJ) \
		$(FS_OBJ) $(INODE_OBJ) $(DENTRY_OBJ) $(SUPERBLOCK_OBJ) \
		$(PATH_OBJ) $(BITMAP_OBJ) $(DISK_OBJ) $(COMMON_OBJ) -o $@

# === CLEANUP ===

clean:
	@rm -rf $(BUILDDIR) *.img
	@echo "Cleaned!"

# === HELP ===

help:
	@echo "Available targets:"
	@echo "  make                	- Build and run all tests"
	@echo "  make test_disk      	- Run disk tests only"
	@echo "  make test_common    	- Run common tests only"
	@echo "  make test_bitmap    	- Run bitmap tests only"
	@echo "  make test_superblock 	- Run superblock tests only"
	@echo "  make test_inode     	- Run inode tests only"
	@echo "  make test_dentry    	- Run dentry tests only"
	@echo "  make test_path      	- Run path tests only"
	@echo "	 make test_fs			- Run fs tests only"
	@echo "  make clean          	- Clean build files"
	@echo "  make help           	- Show this help"

.PHONY: all test_disk test_common test_bitmap test_superblock test_inode test_dentry test_path clean dirs help