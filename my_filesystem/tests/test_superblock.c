/*
    Test for superblock module
*/

#include "superblock.h"
#include "disk.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

void test_superblock_init() {
    printf("Test: superblock init... ");
    
    disk_t disk;
    disk_attach("test_sb_init.img", 1024*1024, true, &disk);
    
    struct superblock sb;
    int result = superblock_init(disk, &sb, 2048, 256);
    
    assert(result == SUCCESS);
    assert(sb.magic_number == MAGIC_NUMBER);
    assert(sb.total_blocks == 2048);
    assert(sb.total_inodes == 256);
    assert(sb.block_size == BLOCK_SIZE);
    assert(sb.inode_size == INODE_SIZE);
    assert(sb.free_blocks > 0);
    assert(sb.free_inodes == 255);
    
    assert(sb.block_bitmap_start > 0);
    assert(sb.inode_bitmap_start > sb.block_bitmap_start);
    assert(sb.inode_table_start > sb.inode_bitmap_start);
    assert(sb.first_data_block > sb.inode_table_start);
    
    disk_detach(disk);
    printf("OK\n");
}

void test_superblock_read_write() {
    printf("Test: superblock read/write... ");
    
    disk_t disk;
    assert(disk_attach("test_sb.img", 1024*1024, true, &disk) == DISK_SUCCESS);
    
    struct superblock sb_write;
    superblock_init(disk, &sb_write, 2048, 256);
    assert(superblock_write(disk, &sb_write) == SUCCESS);
    
    struct superblock sb_read;
    assert(superblock_read(disk, &sb_read) == SUCCESS);
    
    assert(sb_read.magic_number == sb_write.magic_number);
    assert(sb_read.total_blocks == sb_write.total_blocks);
    assert(sb_read.total_inodes == sb_write.total_inodes);
    assert(sb_read.free_blocks == sb_write.free_blocks);
    
    disk_detach(disk);
    printf("OK\n");
}

void test_superblock_validation() {
    printf("Test: superblock validation... ");
    
    disk_t disk;
    disk_attach("test_sb_valid.img", 1024*1024, true, &disk);
    
    struct superblock sb_valid;
    superblock_init(disk, &sb_valid, 2048, 256);
    assert(superblock_is_valid(&sb_valid) == true);
    
    struct superblock sb_invalid;
    memset(&sb_invalid, 0, sizeof(sb_invalid));
    sb_invalid.magic_number = 0xDEADBEEF;
    assert(superblock_is_valid(&sb_invalid) == false);
    
    assert(superblock_is_valid(NULL) == false);
    
    disk_detach(disk);
    printf("OK\n");
}

void test_superblock_persistence() {
    printf("Test: superblock persistence... ");
    
    disk_t disk;
    
    disk_attach("test_sb2.img", 1024*1024, true, &disk);
    struct superblock sb1;
    superblock_init(disk, &sb1, 2048, 256);
    sb1.mount_count = 42;
    superblock_write(disk, &sb1);
    disk_detach(disk);
    
    disk_attach("test_sb2.img", 0, false, &disk);
    struct superblock sb2;
    superblock_read(disk, &sb2);
    
    assert(sb2.magic_number == MAGIC_NUMBER);
    assert(sb2.mount_count == 42);
    
    disk_detach(disk);
    printf("OK\n");
}

void test_superblock_update_counters() {
    printf("Test: update counters... ");
    
    disk_t disk;
    disk_attach("test_sb3.img", 1024*1024, true, &disk);
    
    struct superblock sb;
    superblock_init(disk, &sb, 2048, 256);
    superblock_write(disk, &sb);
    
    superblock_read(disk, &sb);
    sb.free_blocks -= 10;
    sb.free_inodes -= 5;
    superblock_write(disk, &sb);
    
    struct superblock sb_check;
    superblock_read(disk, &sb_check);
    assert(sb_check.free_blocks == sb.free_blocks);
    assert(sb_check.free_inodes == sb.free_inodes);
    
    disk_detach(disk);
    printf("OK\n");
}

int main() {
    printf("=== Superblock Tests ===\n\n");
    
    test_superblock_init();
    test_superblock_read_write();
    test_superblock_validation();
    test_superblock_persistence();
    test_superblock_update_counters();
    
    printf("\nAll superblock tests pass!\n");
    return 0;
}
