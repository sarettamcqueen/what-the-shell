/*
    Test for inode module
*/

#include "inode.h"
#include "disk.h"
#include "bitmap.h"
#include "superblock.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

void test_inode_alloc() {
    printf("Test: inode allocation... ");
    
    disk_t disk;
    disk_attach("test_inode.img", 1024*1024, true, &disk);
    
    struct superblock sb;
    superblock_init(disk, &sb, 2048, 256);
    superblock_write(disk, &sb);
    
    struct bitmap* inode_bmp = bitmap_create(256);
    bitmap_set(inode_bmp, 0);
    
    struct inode in;
    uint32_t inode_num;
    int result = inode_alloc(disk, inode_bmp, INODE_TYPE_FILE, 0644, &in, &inode_num);
    
    assert(result == SUCCESS);
    assert(inode_num == 1);  // first allocatable
    assert(in.type == INODE_TYPE_FILE);
    assert(in.permissions == 0644);
    assert(in.size == 0);
    assert(in.blocks_used == 0);
    assert(in.links_count == 1);
    
    // verify bitmap was updated
    assert(bitmap_get(inode_bmp, inode_num) == true);
    assert(bitmap_get(inode_bmp, 0) == true);  // 0 doesn't change
    
    bitmap_destroy(&inode_bmp);
    disk_detach(disk);
    printf("OK\n");
}

void test_inode_zero_reserved() {
    printf("Test: inode 0 is reserved... ");
    
    disk_t disk;
    disk_attach("test_inode_zero.img", 1024*1024, true, &disk);

    struct superblock sb;
    superblock_init(disk, &sb, 2048, 256);
    superblock_write(disk, &sb);
    
    struct bitmap* inode_bmp = bitmap_create(256);
    
    bitmap_set(inode_bmp, 0);
    
    // allocates 3 inodes
    uint32_t inodes[3];
    for (int i = 0; i < 3; i++) {
        struct inode in;
        inode_alloc(disk, inode_bmp, INODE_TYPE_FILE, 0644, &in, &inodes[i]);
    }
    
    // checks that none of them is inode 0
    assert(inodes[0] == 1);
    assert(inodes[1] == 2);
    assert(inodes[2] == 3);
    
    assert(bitmap_get(inode_bmp, 0) == true);
    
    bitmap_destroy(&inode_bmp);
    disk_detach(disk);
    printf("OK\n");
}

void test_inode_read_write() {
    printf("Test: inode read/write... ");
    
    disk_t disk;
    disk_attach("test_inode2.img", 1024*1024, true, &disk);

    struct superblock sb;
    superblock_init(disk, &sb, 2048, 256);
    superblock_write(disk, &sb);
    
    struct inode in_write;
    memset(&in_write, 0, sizeof(in_write));
    in_write.type = INODE_TYPE_FILE;
    in_write.size = 1024;
    in_write.blocks_used = 2;
    in_write.direct[0] = 100;
    in_write.direct[1] = 101;
    
    assert(inode_write(disk, 5, &in_write) == SUCCESS);
    
    struct inode in_read;
    assert(inode_read(disk, 5, &in_read) == SUCCESS);
    
    assert(in_read.type == INODE_TYPE_FILE);
    assert(in_read.size == 1024);
    assert(in_read.blocks_used == 2);
    assert(in_read.direct[0] == 100);
    assert(in_read.direct[1] == 101);
    
    disk_detach(disk);
    printf("OK\n");
}

void test_inode_free() {
    printf("Test: inode free... ");
    
    disk_t disk;
    disk_attach("test_inode3.img", 1024*1024, true, &disk);

    struct superblock sb;
    superblock_init(disk, &sb, 2048, 256);
    superblock_write(disk, &sb);
    
    struct bitmap* inode_bmp = bitmap_create(256);
    struct bitmap* block_bmp = bitmap_create(2048);
    
    // allocate an inode
    struct inode in;
    uint32_t inode_num;
    inode_alloc(disk, inode_bmp, INODE_TYPE_FILE, 0644, &in, &inode_num);
    assert(bitmap_get(inode_bmp, inode_num) == true);
    
    // verify initial state
    struct inode in_alloc;
    inode_read(disk, inode_num, &in_alloc);
    assert(in_alloc.type == INODE_TYPE_FILE);
    
    // free the inode and track blocks freed
    uint32_t freed_blocks = 0;
    assert(inode_free(disk, inode_bmp, block_bmp, inode_num, &freed_blocks) == SUCCESS);
    
    // verify inode bitmap is updated
    assert(bitmap_get(inode_bmp, inode_num) == false);

    // update superblock tracking free inodes and blocks
    sb.free_inodes++;
    sb.free_blocks += freed_blocks;
    superblock_write(disk, &sb);
    
    // verify inode is zeroed on disk
    struct inode in_freed;
    inode_read(disk, inode_num, &in_freed);
    assert(in_freed.type == INODE_TYPE_FREE);
    
    // verify freed blocks count (should be 0 for newly allocated inode with no data)
    assert(freed_blocks == 0);
    
    // cleanup
    bitmap_destroy(&inode_bmp);
    bitmap_destroy(&block_bmp);
    disk_detach(disk);
    printf("OK\n");
}

void test_inode_free_with_blocks() {
    printf("Test: inode free with data blocks... ");
    
    disk_t disk;
    disk_attach("test_inode4.img", 1024*1024, true, &disk);

    struct superblock sb;
    superblock_init(disk, &sb, 2048, 256);
    superblock_write(disk, &sb);
    
    struct bitmap* inode_bmp = bitmap_create(256);
    struct bitmap* block_bmp = bitmap_create(2048);
    
    // allocate an inode
    struct inode in;
    uint32_t inode_num;
    inode_alloc(disk, inode_bmp, INODE_TYPE_FILE, 0644, &in, &inode_num);
    
    // manually allocate some direct blocks to the inode
    for (int i = 0; i < 3; i++) {
        in.direct[i] = 100 + i;  // Assume blocks 100, 101, 102
        bitmap_set(block_bmp, 100 + i);
    }
    inode_write(disk, inode_num, &in);
    
    // free the inode
    uint32_t freed_blocks = 0;
    assert(inode_free(disk, inode_bmp, block_bmp, inode_num, &freed_blocks) == SUCCESS);
    
    // verify 3 data blocks were freed
    assert(freed_blocks == 3);
    
    // verify blocks are now free in bitmap
    for (int i = 0; i < 3; i++) {
        assert(bitmap_get(block_bmp, 100 + i) == false);
    }

    sb.free_inodes++;
    sb.free_blocks += freed_blocks;
    superblock_write(disk, &sb);
    
    // cleanup
    bitmap_destroy(&inode_bmp);
    bitmap_destroy(&block_bmp);
    disk_detach(disk);
    printf("OK\n");
}

void test_inode_multiple_allocations() {
    printf("Test: multiple allocations... ");
    
    disk_t disk;
    disk_attach("test_inode4.img", 1024*1024, true, &disk);

    struct superblock sb;
    superblock_init(disk, &sb, 2048, 256);
    superblock_write(disk, &sb);
    
    struct bitmap* inode_bmp = bitmap_create(256);
    
    bitmap_set(inode_bmp, 0);
    
    uint32_t inodes[10];
    for (uint32_t i = 0; i < 10; i++) {
        struct inode in;
        inode_alloc(disk, inode_bmp, INODE_TYPE_FILE, 0644, &in, &inodes[i]);
        assert(inodes[i] == i + 1);  // should be 1, 2, ..., 10
    }
    
    assert(bitmap_count_used(inode_bmp) == 11);  // 0 reserved + 10 allocated
    
    bitmap_destroy(&inode_bmp);
    disk_detach(disk);
    printf("OK\n");
}


void test_inode_persistence() {
    printf("Test: inode persistence... ");
    
    disk_t disk;
    disk_attach("test_inode5.img", 1024*1024, true, &disk);

    struct superblock sb;
    superblock_init(disk, &sb, 2048, 256);
    superblock_write(disk, &sb);
    
    // write inode
    struct inode in1;
    memset(&in1, 0, sizeof(in1));
    in1.type = INODE_TYPE_DIRECTORY;
    in1.size = 2048;
    in1.direct[0] = 42;
    inode_write(disk, 10, &in1);
    disk_detach(disk);
    
    // read back
    disk_attach("test_inode5.img", 0, false, &disk);
    struct inode in2;
    inode_read(disk, 10, &in2);
    
    assert(in2.type == INODE_TYPE_DIRECTORY);
    assert(in2.size == 2048);
    assert(in2.direct[0] == 42);
    
    disk_detach(disk);
    printf("OK\n");
}

int main() {
    printf("=== Inode Tests ===\n\n");
    
    test_inode_alloc();
    test_inode_zero_reserved();
    test_inode_read_write();
    test_inode_free();
    test_inode_free_with_blocks();
    test_inode_multiple_allocations();
    test_inode_persistence();
    
    printf("\nAll inode tests pass!\n");
    return 0;
}
