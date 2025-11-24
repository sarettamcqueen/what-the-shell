/*
    Test for dentry module
*/

#include "dentry.h"
#include "disk.h"
#include "bitmap.h"
#include "inode.h"
#include "superblock.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

void test_dentry_create() {
    printf("Test: dentry create... ");
    
    struct dentry d;
    int result = dentry_create("file.txt", 42, INODE_TYPE_FILE, &d);
    
    assert(result == SUCCESS);
    assert(d.inode_num == 42);
    assert(d.file_type == INODE_TYPE_FILE);
    assert(strcmp(d.name, "file.txt") == 0);
    assert(d.name_len == strlen("file.txt"));
    
    printf("OK\n");
}

void test_dentry_validation() {
    printf("Test: dentry validation... ");
    
    struct dentry d1;
    dentry_create("valid.txt", 10, INODE_TYPE_FILE, &d1);
    assert(dentry_is_valid(&d1) == true);
    
    // invalid: inode 0
    struct dentry d2;
    dentry_create("file.txt", INVALID_INODE_NUM, INODE_TYPE_FILE, &d2);
    assert(dentry_is_valid(&d2) == false);
    
    // invalid: empty name
    struct dentry d3;
    dentry_create("", 10, INODE_TYPE_FILE, &d3);
    assert(dentry_is_valid(&d3) == false);
    
    printf("OK\n");
}

void test_dentry_find() {
    printf("Test: dentry find... ");
    
    disk_t disk;
    disk_attach("test_dentry.img", 1024*1024, true, &disk);

    struct superblock sb;
    superblock_init(disk, &sb, 2048, 256);
    superblock_write(disk, &sb);
    
    struct bitmap* inode_bmp = bitmap_create(256);
    struct bitmap* block_bmp = bitmap_create(2048);
    
    // create directory inode
    struct inode dir;
    uint32_t dir_inode_num;
    inode_alloc(disk, inode_bmp, INODE_TYPE_DIRECTORY, 0755, &dir, &dir_inode_num);
    
    // allocate one block for directory
    int block = bitmap_find_first_free(block_bmp);
    bitmap_set(block_bmp, block);
    dir.direct[0] = block;
    dir.blocks_used = 1;
    inode_write(disk, dir_inode_num, &dir);
    
    // write some dentries
    char buffer[BLOCK_SIZE];
    memset(buffer, 0, BLOCK_SIZE);
    struct dentry* entries = (struct dentry*)buffer;
    
    dentry_create("file1.txt", 10, INODE_TYPE_FILE, &entries[0]);
    dentry_create("file2.txt", 11, INODE_TYPE_FILE, &entries[1]);
    disk_write_block(disk, block, buffer);
    
    // find existing
    struct dentry found;
    assert(dentry_find(disk, dir_inode_num, "file1.txt", &found, NULL) == SUCCESS);
    assert(found.inode_num == 10);
    
    // find non-existing
    assert(dentry_find(disk, dir_inode_num, "nonexistent", &found, NULL) == ERROR_NOT_FOUND);
    
    bitmap_destroy(&inode_bmp);
    bitmap_destroy(&block_bmp);
    disk_detach(disk);
    printf("OK\n");
}

void test_dentry_add() {
    printf("Test: dentry add... ");
    
    disk_t disk;
    disk_attach("test_dentry2.img", 1024*1024, true, &disk);

    struct superblock sb;
    superblock_init(disk, &sb, 2048, 256);
    superblock_write(disk, &sb);
    
    struct bitmap* inode_bmp = bitmap_create(256);
    struct bitmap* block_bmp = bitmap_create(2048);
    
    // create directory
    struct inode dir;
    uint32_t dir_inode_num;
    inode_alloc(disk, inode_bmp, INODE_TYPE_DIRECTORY, 0755, &dir, &dir_inode_num);
    
    // allocate block for directory
    int block = bitmap_find_first_free(block_bmp);
    bitmap_set(block_bmp, block);
    dir.direct[0] = block;
    dir.blocks_used = 1;
    inode_write(disk, dir_inode_num, &dir);
    
    // initialize block
    char buffer[BLOCK_SIZE];
    memset(buffer, 0, BLOCK_SIZE);
    disk_write_block(disk, block, buffer);
    
    // add dentry
    struct dentry new_entry;
    dentry_create("newfile.txt", 20, INODE_TYPE_FILE, &new_entry);
    assert(dentry_add(disk, dir_inode_num, &new_entry, block_bmp) == SUCCESS);
    
    // verify it exists
    struct dentry found;
    assert(dentry_find(disk, dir_inode_num, "newfile.txt", &found, NULL) == SUCCESS);
    assert(found.inode_num == 20);
    
    bitmap_destroy(&inode_bmp);
    bitmap_destroy(&block_bmp);
    disk_detach(disk);
    printf("OK\n");
}

void test_dentry_remove() {
    printf("Test: dentry remove... ");
    
    disk_t disk;
    disk_attach("test_dentry3.img", 1024*1024, true, &disk);

    struct superblock sb;
    superblock_init(disk, &sb, 2048, 256);
    superblock_write(disk, &sb);
    
    struct bitmap* inode_bmp = bitmap_create(256);
    struct bitmap* block_bmp = bitmap_create(2048);
    
    // create directory with entries
    struct inode dir;
    uint32_t dir_inode_num;
    inode_alloc(disk, inode_bmp, INODE_TYPE_DIRECTORY, 0755, &dir, &dir_inode_num);
    
    int block = bitmap_find_first_free(block_bmp);
    bitmap_set(block_bmp, block);
    dir.direct[0] = block;
    dir.blocks_used = 1;
    inode_write(disk, dir_inode_num, &dir);
    
    char buffer[BLOCK_SIZE];
    memset(buffer, 0, BLOCK_SIZE);
    struct dentry* entries = (struct dentry*)buffer;
    dentry_create("file1.txt", 10, INODE_TYPE_FILE, &entries[0]);
    dentry_create("file2.txt", 11, INODE_TYPE_FILE, &entries[1]);
    disk_write_block(disk, block, buffer);
    
    // remove file1
    assert(dentry_remove(disk, dir_inode_num, "file1.txt") == SUCCESS);
    
    // verify removed
    assert(dentry_find(disk, dir_inode_num, "file1.txt", NULL, NULL) == ERROR_NOT_FOUND);
    
    // verify file2 still exists
    assert(dentry_find(disk, dir_inode_num, "file2.txt", NULL, NULL) == SUCCESS);
    
    bitmap_destroy(&inode_bmp);
    bitmap_destroy(&block_bmp);
    disk_detach(disk);
    printf("OK\n");
}

void test_dentry_list() {
    printf("Test: dentry list... ");
    
    disk_t disk;
    disk_attach("test_dentry4.img", 1024*1024, true, &disk);

    struct superblock sb;
    superblock_init(disk, &sb, 2048, 256);
    superblock_write(disk, &sb);
    
    struct bitmap* inode_bmp = bitmap_create(256);
    struct bitmap* block_bmp = bitmap_create(2048);
    
    // create directory with entries
    struct inode dir;
    uint32_t dir_inode_num;
    inode_alloc(disk, inode_bmp, INODE_TYPE_DIRECTORY, 0755, &dir, &dir_inode_num);
    
    int block = bitmap_find_first_free(block_bmp);
    bitmap_set(block_bmp, block);
    dir.direct[0] = block;
    dir.blocks_used = 1;
    inode_write(disk, dir_inode_num, &dir);
    
    char buffer[BLOCK_SIZE];
    memset(buffer, 0, BLOCK_SIZE);
    struct dentry* entries = (struct dentry*)buffer;
    dentry_create("file1.txt", 10, INODE_TYPE_FILE, &entries[0]);
    dentry_create("file2.txt", 11, INODE_TYPE_FILE, &entries[1]);
    disk_write_block(disk, block, buffer);
    
    // list all
    struct dentry* list = NULL;
    uint32_t count = 0;
    assert(dentry_list(disk, dir_inode_num, &list, &count) == SUCCESS);
    
    assert(count == 2);
    assert(list != NULL);
    
    // verify entries
    bool found1 = false, found2 = false;
    for (uint32_t i = 0; i < count; i++) {
        if (strcmp(list[i].name, "file1.txt") == 0) found1 = true;
        if (strcmp(list[i].name, "file2.txt") == 0) found2 = true;
    }
    assert(found1 && found2);
    
    free(list);
    bitmap_destroy(&inode_bmp);
    bitmap_destroy(&block_bmp);
    disk_detach(disk);
    printf("OK\n");
}

int main() {
    printf("=== Dentry Tests ===\n\n");
    
    test_dentry_create();
    test_dentry_validation();
    test_dentry_find();
    test_dentry_add();
    test_dentry_remove();
    test_dentry_list();
    
    printf("\nAll dentry tests pass!\n");
    return 0;
}
