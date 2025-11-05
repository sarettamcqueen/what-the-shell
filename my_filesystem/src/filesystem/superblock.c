#include "superblock.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

int superblock_read(disk_t disk, struct superblock* sb) {
    if (!disk || !sb) return ERROR_INVALID;
    int res = disk_read_block(disk, SUPERBLOCK_BLOCK_NUM, sb);
    if (res != DISK_SUCCESS) return ERROR_IO;
    return superblock_valid(sb) ? SUCCESS : ERROR_INVALID;
}

int superblock_write(disk_t disk, const struct superblock* sb) {
    if (!disk || !sb) return ERROR_INVALID;
    int res = disk_write_block(disk, SUPERBLOCK_BLOCK_NUM, sb);
    if (res != DISK_SUCCESS) return ERROR_IO;
    return SUCCESS;
}

// basic initialization of most important fields
int superblock_init(disk_t disk, struct superblock* sb, size_t total_blocks, size_t total_inodes) {
    if (!disk || !sb) return ERROR_INVALID;
    if(total_blocks >= disk_get_blocks(disk)) return ERROR_NO_SPACE;
    
    memset(sb, 0, sizeof(struct superblock));
    
    // === basic metadata ===
    sb->magic_number = MAGIC_NUMBER;
    sb->total_blocks = total_blocks;
    sb->total_inodes = total_inodes;
    sb->block_size = BLOCK_SIZE;
    sb->inode_size = INODE_SIZE;
    
    // === calculate layout ===
    uint32_t current_block = 1;  // block 0: superblock (implicit, not counted in layout blocks)
    
    // data block bitmap
    size_t block_bitmap_bits = total_blocks;
    size_t block_bitmap_bytes = (block_bitmap_bits + 7) / 8;
    sb->block_bitmap_blocks = BLOCKS_NEEDED(block_bitmap_bytes);
    sb->block_bitmap_start = current_block;
    current_block += sb->block_bitmap_blocks;
    
    // inode bitmap
    size_t inode_bitmap_bits = total_inodes;
    size_t inode_bitmap_bytes = (inode_bitmap_bits + 7) / 8;
    sb->inode_bitmap_blocks = BLOCKS_NEEDED(inode_bitmap_bytes);
    sb->inode_bitmap_start = current_block;
    current_block += sb->inode_bitmap_blocks;
    
    // inode table
    size_t inode_table_bytes = total_inodes * INODE_SIZE;
    sb->inode_table_blocks = BLOCKS_NEEDED(inode_table_bytes);
    sb->inode_table_start = current_block;
    current_block += sb->inode_table_blocks;
    
    // data blocks start after all metadata
    sb->first_data_block = current_block;
    
    // calculate free blocks (data area only)
    if (current_block >= total_blocks) {
        return ERROR_NO_SPACE;  // not enough space for filesystem structures
    }
    sb->free_blocks = total_blocks - current_block;
    sb->free_inodes = total_inodes - 1;  // -1 for root directory (inode 2)
    
    // === timestamps ===
    sb->created_time = time(NULL);
    sb->last_mount_time = 0;
    sb->mount_count = 0;
    
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
