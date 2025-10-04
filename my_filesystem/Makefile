CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g
SRCDIR = src
BUILDDIR = build

DISK_SRC = $(SRCDIR)/disk/disk.c
DISK_OBJ = $(BUILDDIR)/disk.o

all: dirs test_disk

dirs:
	@mkdir -p $(BUILDDIR)

$(BUILDDIR)/disk.o: $(SRCDIR)/disk/disk.c
	$(CC) $(CFLAGS) -I$(SRCDIR)/disk -c $< -o $@

test_disk: $(DISK_OBJ) test_disk.c
	$(CC) $(CFLAGS) -I$(SRCDIR)/disk test_disk.c $(DISK_OBJ) -o test_disk

clean:
	rm -rf $(BUILDDIR) test_disk *.img

.PHONY: all clean dirs