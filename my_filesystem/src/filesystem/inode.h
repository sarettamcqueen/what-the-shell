#pragma once

#include "common.h"
#include "disk.h"
#include "bitmap.h"

int inode_read(disk_t disk, uint32_t inode_num, struct inode* out_inode);
int inode_write(disk_t disk, uint32_t inode_num, const struct inode* in_inode);

int inode_alloc(disk_t disk, struct bitmap* inode_bitmap, uint8_t type, uint16_t permissions, struct inode* out_inode, uint32_t* out_inode_num);

/* Frees an inode and its blocks, updates bitmaps.
 * NOTE: caller must update superblock (free_inodes++,
 * free_blocks += out_num_freed_blocks) after calling it.
 */
int inode_free(disk_t disk, struct bitmap* inode_bitmap, struct bitmap* block_bitmap,
                uint32_t inode_num, uint32_t* out_num_freed_blocks);

int inode_is_valid(const struct inode* inode);
void inode_print(const struct inode* inode, uint32_t inode_num);
