/*
    Test for fs module
*/

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "fs.h"
#include "disk.h"

#define TEST_DISK "test_disk.img"

void test_fs_format() {
    printf("Running test_fs_format...\n");

    remove(TEST_DISK);   // ensure clean slate
    disk_t disk = NULL;
    // attach new disk
    int ret = disk_attach(TEST_DISK, 512 * 1000, true, &disk);
    assert(ret == DISK_SUCCESS);
    assert(disk != NULL);

    // test format with valid parameters
    ret = fs_format(disk, 1000, 128);
    assert(ret == SUCCESS);

    // test format with NULL disk
    ret = fs_format(NULL, 1000, 128);
    assert(ret == ERROR_INVALID);

    // test format with invalid parameters (0 blocks)
    ret = fs_format(disk, 0, 128);
    assert(ret != SUCCESS);

    // detach disk after tests
    ret = disk_detach(disk);
    assert(ret == DISK_SUCCESS);
}

void test_fs_mount() {
    printf("Running test_fs_mount...\n");

    remove(TEST_DISK);
    disk_t disk = NULL;
    int ret = disk_attach(TEST_DISK, 512 * 1000, true, &disk);
    assert(ret == DISK_SUCCESS);
    assert(disk != NULL);
    fs_format(disk, 1000, 128);

    filesystem_t* fs = NULL;
    ret = fs_mount(disk, &fs);
    assert(ret == SUCCESS);
    assert(fs != NULL);
    assert(fs->is_mounted == true);

    fs_unmount(fs);
    ret = disk_detach(disk);
    assert(ret == DISK_SUCCESS);

    printf("test_fs_mount PASSED\n\n");
} 

void test_fs_mkdir() {
    printf("Running test_fs_mkdir...\n");

    remove(TEST_DISK);
    disk_t disk = NULL;
    int ret = disk_attach(TEST_DISK, 512 * 1000, true, &disk);
    assert(ret == DISK_SUCCESS);
    assert(disk != NULL);
    fs_format(disk, 1000, 128);

    filesystem_t* fs = NULL;
    fs_mount(disk, &fs);

    ret = fs_mkdir(fs, "/dir1", 0755);
    assert(ret == SUCCESS);

    struct inode st;
    ret = fs_stat(fs, "/dir1", &st);
    assert(ret == SUCCESS);
    assert(st.type == INODE_TYPE_DIRECTORY);

    fs_unmount(fs);
    ret = disk_detach(disk);
    assert(ret == DISK_SUCCESS);

    printf("test_fs_mkdir PASSED\n\n");
}

void test_fs_create() {
    printf("Running test_fs_create...\n");

    remove(TEST_DISK);
    disk_t disk = NULL;
    int ret = disk_attach(TEST_DISK, 512 * 1000, true, &disk);
    assert(ret == DISK_SUCCESS);
    assert(disk != NULL);
    fs_format(disk, 1000, 128);

    filesystem_t* fs = NULL;
    fs_mount(disk, &fs);

    ret = fs_create(fs, "/a.txt", 0644);
    assert(ret == SUCCESS);

    struct inode st;
    ret = fs_stat(fs, "/a.txt", &st);
    assert(ret == SUCCESS);
    assert(st.type == INODE_TYPE_FILE);

    fs_unmount(fs);
    ret = disk_detach(disk);

    printf("test_fs_create PASSED\n\n");
}

void test_fs_write_read() {
    printf("Running test_fs_write_read...\n");

    remove(TEST_DISK);
    disk_t disk = NULL;
    int ret = disk_attach(TEST_DISK, 512 * 1000, true, &disk);
    assert(ret == DISK_SUCCESS);
    assert(disk != NULL);
    fs_format(disk, 1000, 128);

    filesystem_t* fs = NULL;
    fs_mount(disk, &fs);

    fs_create(fs, "/data.bin", 0644);

    open_file_t* f = NULL;
    fs_open(fs, "/data.bin", FS_O_RDWR, &f);

    const char* msg = "Hello filesystem!";
    size_t written = 0;
    fs_write(f, msg, strlen(msg), &written);
    assert(written == strlen(msg));

    fs_seek(f, 0);

    char buffer[64] = {0};
    size_t read = 0;
    fs_read(f, buffer, sizeof(buffer), &read);

    assert(read == strlen(msg));
    assert(strcmp(buffer, msg) == 0);

    fs_close(f);
    fs_unmount(fs);
    disk_detach(disk);

    printf("test_fs_write_read PASSED\n\n");
}


void test_fs_link() {
    printf("Running test_fs_link...\n");

    remove(TEST_DISK);
    disk_t disk = NULL;
    int ret = disk_attach(TEST_DISK, 512 * 1000, true, &disk);
    assert(ret == DISK_SUCCESS);
    assert(disk != NULL);

    // formats filesystem
    ret = fs_format(disk, 1000, 128);
    assert(ret == SUCCESS);

    // mounts filesystem
    filesystem_t* fs = NULL;
    ret = fs_mount(disk, &fs);
    assert(ret == SUCCESS);
    assert(fs != NULL);

    // creates original file
    ret = fs_create(fs, "/orig.txt", 0644);
    assert(ret == SUCCESS);

    // writes something in original file
    open_file_t* f = NULL;
    ret = fs_open(fs, "/orig.txt", FS_O_RDWR, &f);
    assert(ret == SUCCESS);
    assert(f != NULL);

    const char* msg = "hello through links";
    size_t written = 0;
    ret = fs_write(f, msg, strlen(msg), &written);
    assert(ret == SUCCESS);
    assert(written == strlen(msg));

    fs_close(f);

    // creates a hard link: /alias.txt must have same inode as /orig.txt
    ret = fs_link(fs, "/orig.txt", "/alias.txt");
    assert(ret == SUCCESS);

    // checks inodes: links_count must be 2 on both
    struct inode st1, st2;

    ret = fs_stat(fs, "/orig.txt", &st1);
    assert(ret == SUCCESS);

    ret = fs_stat(fs, "/alias.txt", &st2);
    assert(ret == SUCCESS);

    assert(st1.links_count == 2);
    assert(st2.links_count == 2);

    // reading through alias must return same data
    open_file_t* f2 = NULL;
    ret = fs_open(fs, "/alias.txt", FS_O_RDONLY, &f2);
    assert(ret == SUCCESS);
    assert(f2 != NULL);

    char buf[64] = {0};
    size_t read_bytes = 0;
    ret = fs_read(f2, buf, sizeof(buf), &read_bytes);
    assert(ret == SUCCESS);
    assert(read_bytes == strlen(msg));
    assert(memcmp(buf, msg, read_bytes) == 0);

    fs_close(f2);

    // unmounts and detaches disk
    ret = fs_unmount(fs);
    assert(ret == SUCCESS);

    ret = disk_detach(disk);
    assert(ret == DISK_SUCCESS);

    printf("test_fs_link passed.\n");
}

void test_fs_unlink() {
    printf("Running test_fs_unlink...\n");

    remove(TEST_DISK);
    disk_t disk = NULL;
    int ret = disk_attach(TEST_DISK, 512 * 1000, true, &disk);
    fs_format(disk, 1000, 128);

    filesystem_t* fs = NULL;
    fs_mount(disk, &fs);

    fs_create(fs, "/tmp.txt", 0644);

    ret = fs_unlink(fs, "/tmp.txt");
    assert(ret == SUCCESS);

    struct inode st;
    ret = fs_stat(fs, "/tmp.txt", &st);
    assert(ret == ERROR_NOT_FOUND);

    fs_unmount(fs);
    ret = disk_detach(disk);
    assert(ret == DISK_SUCCESS);

    printf("test_fs_unlink PASSED\n\n");
}

void test_fs_cd() {
    printf("Running test_fs_cd...\n");

    remove(TEST_DISK);
    disk_t disk = NULL;
    int ret = disk_attach("test_disk.img", 512 * 1000, true, &disk);
    assert(ret == DISK_SUCCESS);

    // format + mount
    ret = fs_format(disk, 1000, 128);
    assert(ret == SUCCESS);

    filesystem_t* fs = NULL;
    ret = fs_mount(disk, &fs);
    assert(ret == SUCCESS);

    // we start at root
    assert(fs->current_dir_inode == ROOT_INODE_NUM);  // == 1

    // creates a directory "dir1"
    ret = fs_mkdir(fs, "/dir1", 0755);
    assert(ret == SUCCESS);

    // cd /dir1
    ret = fs_cd(fs, "/dir1");
    assert(ret == SUCCESS);
    assert(fs->current_dir_inode != ROOT_INODE_NUM);

    // cd .. (back to root)
    ret = fs_cd(fs, "..");
    assert(ret == SUCCESS);
    assert(fs->current_dir_inode == ROOT_INODE_NUM);

    // creates dir2 into dir1
    ret = fs_mkdir(fs, "/dir1/dir2", 0755);
    assert(ret == SUCCESS);

    // cd /dir1/dir2
    ret = fs_cd(fs, "/dir1/dir2");
    assert(ret == SUCCESS);

    // cd ./ (should remain in same directory)
    ret = fs_cd(fs, "./");
    assert(ret == SUCCESS);

    // cd ../.. (should go back to root)
    ret = fs_cd(fs, "../..");
    assert(ret == SUCCESS);
    assert(fs->current_dir_inode == ROOT_INODE_NUM);

    // cd to a non-existing directory
    ret = fs_cd(fs, "/does_not_exist");
    assert(ret == ERROR_NOT_FOUND);

    // cd to a FILE (should fail)
    ret = fs_create(fs, "/file.txt", 0644);
    assert(ret == SUCCESS);

    ret = fs_cd(fs, "/file.txt");
    assert(ret == ERROR_INVALID);

    // cleanup
    ret = fs_unmount(fs);
    assert(ret == SUCCESS);

    ret = disk_detach(disk);
    assert(ret == DISK_SUCCESS);

    printf("test_fs_cd passed.\n");
}

void test_fs_rmdir() {
    printf("Running test_fs_rmdir...\n");

    remove(TEST_DISK);
    disk_t disk = NULL;
    int ret = disk_attach(TEST_DISK, 512 * 100, 20, &disk);
    fs_format(disk, 100, 20);

    filesystem_t* fs = NULL;
    fs_mount(disk, &fs);

    fs_mkdir(fs, "/d", 0755);

    ret = fs_rmdir(fs, "/d");
    assert(ret == SUCCESS);

    struct inode st;
    ret = fs_stat(fs, "/d", &st);
    assert(ret == ERROR_NOT_FOUND);

    fs_unmount(fs);
    ret = disk_detach(disk);

    printf("test_fs_rmdir PASSED\n\n");
}

int main() {
    test_fs_format();
    test_fs_mount();
    test_fs_mkdir();
    test_fs_create();
    test_fs_write_read();
    test_fs_link();
    test_fs_unlink();
    test_fs_cd();
    test_fs_rmdir();

    printf("\n===== ALL FS TESTS PASSED SUCCESSFULLY =====\n");
    return 0;
}