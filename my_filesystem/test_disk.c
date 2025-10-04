/* 
    Temporary test for disk module
*/

#include "disk.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

int main() {
    printf("=== Disk Emulator Test ===\n");
    
    // test 1: disk creation
    disk_t disk;
    int result = disk_attach("test.img", 1024*1024, true, &disk);
    assert(disk != NULL);
    printf("Disk was successfully created.\n");
    
    // test 2: block write
    char write_buf[512] = "Mtzpp!";
    assert(disk_write_block(disk, 0, write_buf) == DISK_SUCCESS);
    printf("Wrote block 0\n");
    
    // test 3: block read
    char read_buf[512] = {0};
    assert(disk_read_block(disk, 0, read_buf) == DISK_SUCCESS);
    assert(strcmp(write_buf, read_buf) == 0);
    printf("Read block 0\n");
    
    // test 4: detach
    assert(disk_detach(disk) == DISK_SUCCESS);
    printf("Disk successfully detached\n");
    
    // test 5: persistence
    result = disk_attach("test.img", 0, false, &disk);
    assert(disk != NULL);

    memset(read_buf, 0, 512);
    disk_read_block(disk, 0, read_buf);
    assert(strcmp(write_buf, read_buf) == 0);
    printf("Persistent data\n");
    
    disk_detach(disk);
    
    printf("\nAll tests pass!\n");
    return 0;
}