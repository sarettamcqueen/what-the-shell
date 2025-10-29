CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g
SRCDIR = src
TESTDIR = tests
BUILDDIR = build

DISK_SRC = $(SRCDIR)/disk/disk.c
DISK_OBJ = $(BUILDDIR)/disk.o
TEST_SRC = $(TESTDIR)/test_disk.c
TEST_BIN = $(BUILDDIR)/test_disk

all: dirs $(TEST_BIN)

dirs:
	@mkdir -p $(BUILDDIR)

$(DISK_OBJ): $(DISK_SRC)
	$(CC) $(CFLAGS) -I$(SRCDIR)/disk -c $< -o $@

$(TEST_BIN): $(DISK_OBJ) $(TEST_SRC)
	$(CC) $(CFLAGS) -I$(SRCDIR)/disk -I$(TESTDIR) $(TEST_SRC) $(DISK_OBJ) -o $(TEST_BIN)

clean:
	rm -rf $(BUILDDIR) *.img

.PHONY: all clean dirs
