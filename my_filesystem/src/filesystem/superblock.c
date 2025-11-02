#include "superblock.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

int superblock_read(disk_t disk, struct superblock* sb) {
    if (!sb) return ERROR_INVALID;
    int res = disk_read_block(disk, SUPERBLOCK_BLOCK_NUM, sb);
    if (res != DISK_SUCCESS) return ERROR_IO;
    return superblock_valid(sb) ? SUCCESS : ERROR_INVALID;
}

int superblock_write(disk_t disk, const struct superblock* sb) {
    if (!sb) return ERROR_INVALID;
    int res = disk_write_block(disk, SUPERBLOCK_BLOCK_NUM, sb);
    if (res != DISK_SUCCESS) return ERROR_IO;
    return SUCCESS;
}

// basic initialization of most important fields.
// NOTE: future layout info must be updated after accurate calculations of the disk layout
int superblock_init(disk_t disk, struct superblock* sb, size_t total_blocks, size_t total_inodes, size_t block_size, size_t inode_size) {
    memset(sb, 0, sizeof(struct superblock));
    sb->magic_number = MAGIC_NUMBER;
    sb->total_blocks = total_blocks;
    sb->total_inodes = total_inodes;
    sb->free_blocks = total_blocks;
    sb->free_inodes = total_inodes;
    sb->block_size = block_size;
    sb->inode_size = inode_size;
    sb->first_data_block = 0;       // to be set correctly after said calculations
    sb->created_time = time(NULL);
    sb->last_mount_time = 0;
    sb->mount_count = 0;
    // the field "reserved" has already been set to zero by memset
    return SUCCESS;
}

void superblock_print(const struct superblock* sb) {
    if (!sb) {
        printf("Superblock: NULL\n");
        return;
    }
    printf("Superblock info:\n");
    printf("  Magic number   : 0x%08X\n", sb->magic_number);
    printf("  Total blocks   : %u\n", sb->total_blocks);
    printf("  Free blocks    : %u\n", sb->free_blocks);
    printf("  Total inodes   : %u\n", sb->total_inodes);
    printf("  Free inodes    : %u\n", sb->free_inodes);
    printf("  Block size     : %u\n", sb->block_size);
    printf("  Inode size     : %u\n", sb->inode_size);
    printf("  First data block : %u\n", sb->first_data_block);
    printf("  Created        : ");
    print_timestamp(sb->created_time);
    printf("\n  Last mount     : ");
    print_timestamp(sb->last_mount_time);
    printf("\n  Mount count    : %u\n", sb->mount_count);
}

int is_superblock_valid(const struct superblock* sb) {
    return sb && sb->magic_number == MAGIC_NUMBER;
}
