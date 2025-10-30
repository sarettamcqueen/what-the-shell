/* 
    Temporary (and silly) test for common definitions and functions
*/

#include "common.h"
#include <stdio.h>
#include <assert.h>

int main() {
    printf("=== Common Definitions Test ===\n");
    
    // test 1: struct sizes
    printf("sizeof(struct superblock) = %zu\n", sizeof(struct superblock));
    printf("sizeof(struct inode) = %zu\n", sizeof(struct inode));
    printf("sizeof(struct dentry) = %zu\n", sizeof(struct dentry));
    
    assert(sizeof(struct inode) == 128);
    assert(sizeof(struct dentry) == 256);
    printf("Structs are properly aligned\n");
    
    // test 2: macros
    assert(BLOCKS_NEEDED(100) == 1);
    assert(BLOCKS_NEEDED(512) == 1);
    assert(BLOCKS_NEEDED(513) == 2);
    assert(BLOCKS_NEEDED(1000) == 2);
    printf("BLOCKS_NEEDED works\n");
    
    assert(ALIGN_TO_BLOCK(100) == 512);
    assert(ALIGN_TO_BLOCK(512) == 512);
    assert(ALIGN_TO_BLOCK(513) == 1024);
    printf("ALIGN_TO_BLOCK works\n");
    
    assert(MIN(5, 10) == 5);
    assert(MAX(5, 10) == 10);
    printf("MIN/MAX work\n");
    
    // test 3: constants
    assert(BLOCK_SIZE == 512);
    assert(MAGIC_NUMBER == 0x12345678);
    printf("Constants are correct\n");
    
    printf("\nAll tests pass!\n");
    return 0;
}