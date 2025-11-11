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
    
    bitmap_destroy(inode_bmp);
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
    
    bitmap_destroy(inode_bmp);
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
    
    // allocate
    struct inode in;
    uint32_t inode_num;
    inode_alloc(disk, inode_bmp, INODE_TYPE_FILE, 0644, &in, &inode_num);
    assert(bitmap_get(inode_bmp, inode_num) == true);
    
    // free
    assert(inode_free(disk, inode_bmp, inode_num) == SUCCESS);
    assert(bitmap_get(inode_bmp, inode_num) == false);
    
    // verify inode is zeroed
    struct inode in_check;
    inode_read(disk, inode_num, &in_check);
    assert(in_check.type == INODE_TYPE_FREE);
    
    bitmap_destroy(inode_bmp);
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
    
    bitmap_destroy(inode_bmp);
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
    test_inode_multiple_allocations();
    test_inode_persistence();
    
    printf("\nAll inode tests pass!\n");
    return 0;
}
