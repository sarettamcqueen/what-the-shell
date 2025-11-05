CC = gcc
CFLAGS = -Wall -Wextra -std=gnu99 -g -Iinclude
SRCDIR = src
TESTDIR = tests
BUILDDIR = build

# disk module
DISK_SRC = $(SRCDIR)/disk/disk.c
DISK_OBJ = $(BUILDDIR)/disk.o

# common/utils module
COMMON_SRC = $(SRCDIR)/utils/common.c
COMMON_OBJ = $(BUILDDIR)/common.o

# bitmap module
BITMAP_SRC = $(SRCDIR)/utils/bitmap.c
BITMAP_OBJ = $(BUILDDIR)/bitmap.o

# superblock module
SUPERBLOCK_SRC = $(SRCDIR)/filesystem/superblock.c
SUPERBLOCK_OBJ = $(BUILDDIR)/superblock.o

# inode module
INODE_SRC = $(SRCDIR)/filesystem/inode.c
INODE_OBJ = $(BUILDDIR)/inode.o

# test binaries
TEST_DISK_SRC = $(TESTDIR)/test_disk.c
TEST_DISK_BIN = $(BUILDDIR)/test_disk

TEST_COMMON_SRC = $(TESTDIR)/test_common.c
TEST_COMMON_BIN = $(BUILDDIR)/test_common

TEST_BITMAP_SRC = $(TESTDIR)/test_bitmap.c
TEST_BITMAP_BIN = $(BUILDDIR)/test_bitmap

# default: compile and run all tests
all: dirs $(TEST_DISK_BIN) $(TEST_COMMON_BIN) $(TEST_BITMAP_BIN)
	@echo "=== Running test_disk ==="
	@./$(TEST_DISK_BIN)
	@echo ""
	@echo "=== Running test_common ==="
	@./$(TEST_COMMON_BIN)
	@echo ""
	@echo "=== Running test_bitmap ==="
	@./$(TEST_BITMAP_BIN)
	@echo ""
	@echo "All tests passed!"

# disk tests only
test_disk: dirs $(TEST_DISK_BIN)
	@./$(TEST_DISK_BIN)

# common tests only
test_common: dirs $(TEST_COMMON_BIN)
	@./$(TEST_COMMON_BIN)

# bitmap tests only
test_bitmap: dirs $(TEST_BITMAP_BIN)
	@./$(TEST_BITMAP_BIN)

# create directory
dirs:
	@mkdir -p $(BUILDDIR)

# compile disk.o
$(DISK_OBJ): $(DISK_SRC) $(SRCDIR)/disk/disk.h include/config.h
	@echo "Compiling disk module..."
	@$(CC) $(CFLAGS) -I$(SRCDIR)/disk -c $< -o $@

# compile common.o
$(COMMON_OBJ): $(COMMON_SRC) include/common.h include/config.h
	@echo "Compiling common module..."
	@$(CC) $(CFLAGS) -c $< -o $@

# compile bitmap.o
$(BITMAP_OBJ): $(BITMAP_SRC) $(SRCDIR)/utils/bitmap.h include/common.h
	@echo "Compiling bitmap module..."
	@$(CC) $(CFLAGS) -I$(SRCDIR)/utils -c $< -o $@

# compile superblock.o
$(SUPERBLOCK_OBJ): $(SUPERBLOCK_SRC) $(SRCDIR)/filesystem/superblock.h include/common.h
	@echo "Compiling superblock module..."
	@$(CC) $(CFLAGS) -I$(SRCDIR)/filesystem -c $< -o $@

# compile inode.o
$(INODE_OBJ): $(INODE_SRC) $(SRCDIR)/filesystem/inode.h $(SRCDIR)/utils/bitmap.h include/common.h
	@echo "Compiling inode module..."
	@$(CC) $(CFLAGS) -I$(SRCDIR)/filesystem -I$(SRCDIR)/utils -c $< -o $@

# link test_disk
$(TEST_DISK_BIN): $(DISK_OBJ) $(TEST_DISK_SRC)
	@echo "Building test_disk..."
	@$(CC) $(CFLAGS) -I$(SRCDIR)/disk $(TEST_DISK_SRC) $(DISK_OBJ) -o $@

# link test_common
$(TEST_COMMON_BIN): $(COMMON_OBJ) $(TEST_COMMON_SRC)
	@echo "Building test_common..."
	@$(CC) $(CFLAGS) $(TEST_COMMON_SRC) $(COMMON_OBJ) -o $@

# link test_bitmap
$(TEST_BITMAP_BIN): $(BITMAP_OBJ) $(COMMON_OBJ) $(TEST_BITMAP_SRC)
	@echo "Building test_bitmap..."
	@$(CC) $(CFLAGS) -I$(SRCDIR)/utils $(TEST_BITMAP_SRC) $(BITMAP_OBJ) $(COMMON_OBJ) -o $@

# cleanup
clean:
	@rm -rf $(BUILDDIR) *.img
	@echo "Cleaned!"

# help
help:
	@echo "Available targets:"
	@echo "  make           - Build and run all tests"
	@echo "  make test_disk - Run disk tests only"
	@echo "  make test_common - Run common tests only"
	@echo "  make test_bitmap - Run bitmap tests only"
	@echo "  make clean     - Clean build files"

.PHONY: all test_disk test_common test_bitmap clean dirs help
