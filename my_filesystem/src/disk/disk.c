#include "disk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>

// disk emulator struct definition (it is private here in disk.c)
struct disk_emulator {
    int fd;                          // file descriptor of file on disk
    void* mapped_memory;             // pointer to mapped memory
    size_t size;                     // total size in bytes
    int block_count;                 // number of blocks
    int block_size;                  // size of a block (512)
    bool attached;                   // true if disk is attached
    char filename[MAX_FILENAME];     // filename on disk
};

// === PRIVATE FUNCTIONS ===

// validates a block number
static bool is_valid_block(disk_t disk, int block_num) {
    return block_num >= 0 && block_num < disk->block_count;
}

// converts block number into offset
static off_t block_to_offset(int block_num) {
    return (off_t)block_num * BLOCK_SIZE;
}

// === PUBLIC FUNCTIONS ===

int disk_attach(const char* filename, size_t size, bool create_new, disk_t* disk) {
    if (!filename || !disk) {
        return DISK_ERROR;
    }

    *disk = NULL;

    disk_t d = malloc(sizeof(struct disk_emulator));
    if (!d) {
        return DISK_ERROR;
    }
    
    // open/create file
    int flags = create_new ? (O_RDWR | O_CREAT | O_TRUNC) : O_RDWR;
    int mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
    
    d->fd = open(filename, flags, mode);
    if (d->fd == -1) {
        perror("disk_attach: open");
        return DISK_ERROR_IO;
    }

    // if we're creating a new disk, set new size
    if (create_new) {
        if (ftruncate(d->fd, size) == -1) {
            perror("disk_attach: ftruncate");
            close(d->fd);
            d->fd = -1;
            return DISK_ERROR_IO;
        }
        d->size = size;
    } else {
        // get file size
        struct stat st;
        if (fstat(d->fd, &st) == -1) {
            perror("disk_attach: fstat");
            close(d->fd);
            d->fd = -1;
            return DISK_ERROR_IO;
        }
        d->size = st.st_size;
    }

    // map file in memory
    d->mapped_memory = mmap(NULL, d->size, PROT_READ | PROT_WRITE, 
                              MAP_SHARED, d->fd, 0);
    if (d->mapped_memory == MAP_FAILED) {
        perror("disk_attach: mmap");
        close(d->fd);
        d->fd = -1;
        return DISK_ERROR_IO;
    }

    // initialize struct fields
    strncpy(d->filename, filename, MAX_FILENAME - 1);
    d->filename[MAX_FILENAME - 1] = '\0';
    d->block_count = d->size / BLOCK_SIZE;
    d->attached = true;

    printf("Disk attached: %s (Size: %zu bytes, Blocks: %d)\n",
           filename, d->size, d->block_count);

    *disk = d;
    return DISK_SUCCESS;
}

int disk_detach(disk_t disk) {
    if (!disk || !disk->attached) {
        return DISK_ERROR_NOT_ATTACHED;
    }

    // sync before detach
    disk_sync(disk);

    // unmap memory
    if (disk->mapped_memory && disk->mapped_memory != MAP_FAILED) {
        if (munmap(disk->mapped_memory, disk->size) == -1) {
            perror("disk_detach: munmap");
        }
    }

    // close file
    if (disk->fd != -1) {
        close(disk->fd);
    }

    printf("Disk detached: %s\n", disk->filename);

    // reset structure
    memset(disk, 0, sizeof(struct disk_emulator));
    disk->fd = -1;
    disk->block_size = BLOCK_SIZE;

    return DISK_SUCCESS;
}

int disk_read_block(disk_t disk, int block_num, void* buffer) {
    if (!disk_is_attached(disk)) {
        return DISK_ERROR_NOT_ATTACHED;
    }
    
    if (!is_valid_block(disk, block_num)) {
        return DISK_ERROR_INVALID_BLOCK;
    }
    
    if (!buffer) {
        return DISK_ERROR;
    }

    off_t offset = block_to_offset(block_num);
    
    // copy from mapped memory to buffer
    memcpy(buffer, (char*)disk->mapped_memory + offset, BLOCK_SIZE);
    
    return DISK_SUCCESS;
}

int disk_write_block(disk_t disk, int block_num, const void* buffer) {
    if (!disk_is_attached(disk)) {
        return DISK_ERROR_NOT_ATTACHED;
    }
    
    if (!is_valid_block(disk, block_num)) {
        return DISK_ERROR_INVALID_BLOCK;
    }
    
    if (!buffer) {
        return DISK_ERROR;
    }

    off_t offset = block_to_offset(block_num);
    
    // copy from buffer to mapped memory
    memcpy((char*)disk->mapped_memory + offset, buffer, BLOCK_SIZE);
    
    return DISK_SUCCESS;
}

int disk_read(disk_t disk, off_t offset, void* buffer, size_t size) {
    if (!disk_is_attached(disk)) {
        return DISK_ERROR_NOT_ATTACHED;
    }
    
    if (!buffer || size == 0) {
        return DISK_ERROR;
    }
    
    if (offset + size > disk->size) {
        return DISK_ERROR_INVALID_BLOCK;
    }
    
    memcpy(buffer, (char*)disk->mapped_memory + offset, size);
    return DISK_SUCCESS;
}

int disk_write(disk_t disk, off_t offset, const void* buffer, size_t size) {
    if (!disk_is_attached(disk)) {
        return DISK_ERROR_NOT_ATTACHED;
    }
    
    if (!buffer || size == 0) {
        return DISK_ERROR;
    }
    
    if (offset + size > disk->size) {
        return DISK_ERROR_INVALID_BLOCK;
    }
    
    memcpy((char*)disk->mapped_memory + offset, buffer, size);
    return DISK_SUCCESS;
}

int disk_get_size(disk_t disk) {
    if (!disk_is_attached(disk)) {
        return DISK_ERROR_NOT_ATTACHED;
    }
    return (int)disk->size;
}

int disk_get_blocks(disk_t disk) {
    if (!disk_is_attached(disk)) {
        return DISK_ERROR_NOT_ATTACHED;
    }
    return disk->block_count;
}

int disk_get_block_size(disk_t disk) {
    if (!disk_is_attached(disk)) {
        return DISK_ERROR_NOT_ATTACHED;
    }
    return disk->block_size;
}

bool disk_is_attached(disk_t disk) {
    if (!disk) {
        return false;
    }
    return disk->attached;
}

const char* disk_get_filename(disk_t disk) {
    if (!disk_is_attached(disk)) {
        return NULL;
    }
    return disk->filename;
}

int disk_sync(disk_t disk) {
    if (!disk_is_attached(disk)) {
        return DISK_ERROR_NOT_ATTACHED;
    }
    
    // force write on disk
    if (msync(disk->mapped_memory, disk->size, MS_SYNC) == -1) {
        perror("disk_sync: msync");
        return DISK_ERROR_IO;
    }
    
    return DISK_SUCCESS;
}

void disk_print_info(disk_t disk) {
    if (!disk_is_attached(disk)) {
        printf("Disk not attached\n");
        return;
    }

    printf("Disk Info:\n");
    printf("  Filename: %s\n", disk->filename);
    printf("  Size: %zu bytes\n", disk->size);
    printf("  Blocks: %d\n", disk->block_count);
    printf("  Block size: %d bytes\n", disk->block_size);
    printf("  Attached: %s\n", disk->attached ? "yes" : "no");
}

const char* disk_error_string(int error_code) {
    switch (error_code) {
        case DISK_SUCCESS: return "Success";
        case DISK_ERROR: return "Generic error";
        case DISK_ERROR_NOT_FOUND: return "Disk not found";
        case DISK_ERROR_ALREADY_ATTACHED: return "Disk already attached";
        case DISK_ERROR_NOT_ATTACHED: return "Disk not attached";
        case DISK_ERROR_INVALID_BLOCK: return "Invalid block number";
        case DISK_ERROR_IO: return "I/O error";
        case DISK_ERROR_NO_SPACE: return "No space available";
        default: return "Unknown error";
    }
}