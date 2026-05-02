# what-the-shell — Custom filesystem with VFS layer and memory mapping

A from-scratch implementation in C of a Unix-like filesystem built on top of a
memory-mapped disk image. The on-disk format is inspired by ext2. A
Virtual File System (VFS) layer sits above the filesystem to provide
transparent multi-mount support, unified path resolution, and cross-disk
operations, all accessible through an interactive shell.

Built as a university thesis project to explore how operating systems
implement block-level storage, memory-mapped I/O, and the VFS abstraction
from the ground up.

---

## Architecture

The system is organized in four strictly separated layers, each depending only
on the layer below it:

```
┌─────────────────────────────┐
│           shell             │  interactive command interpreter
├─────────────────────────────┤
│            vfs              │  multi-mount path resolution, cross-disk operations
├─────────────────────────────┤
│         filesystem          │  inode/block allocation, dentries handling, permissions
├─────────────────────────────┤
│            disk             │  raw block I/O on .img files
└─────────────────────────────┘
```

### Disk layer (`src/disk/`)

Provides raw block-level read and write operations on a regular file used as a
disk image. The block size is 512 bytes (it can be safely modified in `my_filesystem/include/config.h`). The layer exposes a `disk_t` handle
and is entirely agnostic of filesystem semantics.

### Filesystem layer (`src/filesystem/`)

Implements the on-disk format and all filesystem logic:

- **Superblock**: stores the metadata needed to mount and validate the filesystem: total and free block/inode counts, block size,
  timestamps, and the starting position and size of each metadata region (block bitmap, inode bitmap, inode table, and data area).
  Layout is verified at compile time with `_Static_assert`.
- **Bitmaps**: used to track free blocks/inodes. Loaded into memory on
  mount and flushed to disk on every write operation and on unmount.
- **Inode table**: fixed-size 128-byte inodes containing file type, size,
  link count, permission bits, three timestamps, 12 direct block pointers, and
  one singly-indirect block pointer. Inode 0 is reserved as invalid; inode 1
  is the root directory.
- **Dentries**: fixed-size 64-byte directory entries containing the entry
  name, inode number, and file type. Each directory always contains `.` and
  `..` entries.
- **Permission enforcement**: the 9-bit Unix `rwxrwxrwx` field is stored in
  every inode. Since the filesystem has no concept of users or groups, the
  owner bits (`rwx------`) are used for all access checks. Permissions are
  enforced at `open`, directory traversal, `ls`, `cd`, `mkdir`, `touch`, `rm`,
  `rmdir`, `mv`, and `ln`.

The minimum disk size is 10 KiB (enforced at format time).

### VFS layer (`src/vfs/`)

Maintains a mount table of up to 8 simultaneously mounted filesystem instances
and presents them as a single unified namespace:

- **Path resolution**: `vfs_resolve_path` performs longest-prefix matching on
  mount paths to dispatch each operation to the correct underlying filesystem.
- **Unified CWD**: a single absolute path string tracks the current working
  directory across all mounted filesystems transparently.
- **Cross-disk operations**: `vfs_cp` always performs a full read/write copy, transfering data block by block, 
  regardless of whether source and destination reside on the same filesystem
  or not. `vfs_mv` instead distinguishes between the two cases: if source and destination are on the
  same filesystem it only updates the dentry (rename);
  if they reside on different filesystems it falls back to `vfs_cp` followed
  by deletion of the source.

### Shell layer (`src/shell/`)

A minimal interactive shell that tokenizes input, dispatches commands, and
prints human-readable error messages. It operates entirely through the VFS API
and has no knowledge of the underlying filesystem implementation.

---

## On-Disk format

```
Block 0          : superblock
Blocks 1..n      : block allocation bitmap
Blocks n+1..m    : inode allocation bitmap
Blocks m+1..p    : inode table
Blocks p+1..end  : data area
```

Sizes are computed at format time from the total disk size, using a ratio of
one inode per 8 KiB of data space (`BYTES_PER_INODE = 8192`), with a minimum
of 64 inodes (`MIN_INODES = 64`).

---

## Building

Requires GCC and GNU Make.

```sh
cd my_filesystem
make        # builds the binary
make run    # builds and runs the shell directly
make test   # builds and runs the automated test suite
make clean  # removes build artifacts
```

The compiled binary is placed in `build/my_filesystem`.

---

## Usage

```sh
make run
# or
./build/my_filesystem
```

The shell starts with no filesystem mounted. The typical workflow is:

```
format <image> <size_bytes>   # initialize a new disk image
mount  <image> <mountpoint>   # mount it (first mount must be on /)
```

### Available commands

| Command | Description |
|---|---|
| `format <img> <size>` | Create and initialize a new disk image |
| `mount <img> <path>` | Mount a disk image at the given path |
| `unmount <path>` | Flush and unmount a filesystem |
| `ls [path]` | List directory contents |
| `cd <path>` | Change current directory |
| `pwd` | Print current working directory |
| `mkdir <path>` | Create a directory |
| `rmdir <path>` | Remove an empty directory |
| `touch <path>` | Create an empty file |
| `rm <path>` | Remove a file |
| `cat <path>` | Print file contents |
| `write <path> "text"` | Overwrite file with text |
| `append <path> "text"` | Append text to file |
| `cp <src> <dst>` | Copy a file (cross-disk supported) |
| `mv <src> <dst>` | Move or rename a file/directory (cross-disk supported) |
| `ln <existing> <new>` | Create a hard link (same filesystem only) |
| `chmod <mode> <path>` | Change permission bits (octal, e.g. `755`) |
| `stat <path>` | Display inode metadata |
| `df` | Show free space for all mounted filesystems |

### Single-disk session

```
format disk.img 1024000
mount disk.img /
mkdir /home
touch /home/notes.txt
write /home/notes.txt "hello"
cat /home/notes.txt
stat /home/notes.txt
exit
```

Data persists across unmount/remount cycles. The magic number and filesystem
geometry can be inspected directly:

```sh
xxd disk.img | head -1
```

### Multi-mount session

```
format root.img 2048000
format usb.img 1024000
mount root.img /
mkdir /mnt
mkdir /mnt/usb
mount usb.img /mnt/usb
touch /home/file.txt
write /home/file.txt "data"
cp /home/file.txt /mnt/usb/backup.txt   # cross-disk copy
df                                      # statistics for both filesystems
unmount /mnt/usb
exit
```

### Permission examples

```
touch /secret.txt
write /secret.txt "classified"
chmod 400 /secret.txt       # read-only
cat /secret.txt             # ok
write /secret.txt "no"      # Permission denied

chmod 200 /secret.txt       # write-only
cat /secret.txt             # Permission denied
write /secret.txt "ok"      # ok

chmod 000 /dir              # no access
ls /dir                     # Permission denied
cd /dir                     # Permission denied
```

---

## Known limitations

- **Fixed-size dentries**: directory entries are 64 bytes regardless of name
  length, which wastes space compared to the variable-length dentries used by
  ext2 and later filesystems.
- **No journaling**: an unclean shutdown may leave the filesystem in an
  inconsistent state. There is no recovery mechanism.
- **No concurrency**: the filesystem is designed for single-threaded access
  only. No locking of any kind is implemented.
- **Single indirect block**: files are limited to 12 + 128 = 140 blocks
  (71 680 bytes with 512-byte blocks). Double and triple indirection are not
  implemented.
- **No users or groups**: the owner permission bits (`rwx------`) are applied
  uniformly to all callers. The group and other bits are stored but not enforced.
- **Single filesystem type**: the VFS supports up to 8 simultaneous mounts,
  but all must be instances of the same custom filesystem. Support for
  heterogeneous filesystem types is a natural next step but is not implemented.
