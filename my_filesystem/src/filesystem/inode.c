#include "inode.h"
#include "superblock.h"
#include <stdio.h>
#include <string.h>

// === PRIVATE FUNCTIONS ===

// computes block position and offset for a given inode_num
static void inode_get_disk_position(const struct superblock* sb, uint32_t inode_num, uint32_t* block_num, uint32_t* block_offset) {
    uint32_t first_inode_block = sb->inode_table_start;
    uint32_t inodes_per_block = sb->block_size / sb->inode_size;
    *block_num   = first_inode_block + (inode_num / inodes_per_block);
    *block_offset = (inode_num % inodes_per_block) * sb->inode_size;
}

// === PUBLIC FUNCTIONS ===

int inode_read(disk_t disk, uint32_t inode_num, struct inode* out_inode) {
    // allocate superblock on the stack (uninitialized memory)
    struct superblock sb;
    
    //read superblock from disk (block 0) and populate sb
    if (superblock_read(disk, &sb) != SUCCESS) 
        return ERROR_IO;
    
    // calculate where the requested inode is located
    uint32_t block_num, block_offset;
    inode_get_disk_position(&sb, inode_num, &block_num, &block_offset);
    
    // read the block containing the inode
    char buffer[BLOCK_SIZE];
    if (disk_read_block(disk, block_num, buffer) != DISK_SUCCESS) 
        return ERROR_IO;
    // buffer now contains 512 bytes of the block (with 4 inodes inside)
    
    // copy the specific inode from buffer to output structure
    memcpy(out_inode, buffer + block_offset, sizeof(struct inode));
    // copies 128 bytes from the correct offset in buffer to out_inode
    
    return SUCCESS;
}

int inode_write(disk_t disk, uint32_t inode_num, const struct inode* in_inode) {
    // load superblock from disk to get layout info
    struct superblock sb;
    if (superblock_read(disk, &sb) != SUCCESS) 
        return ERROR_IO;
    
    // calculate position of the inode on disk
    uint32_t block_num, block_offset;
    inode_get_disk_position(&sb, inode_num, &block_num, &block_offset);
    
    // read the existing block (to preserve other inodes)
    char buffer[BLOCK_SIZE];
    if (disk_read_block(disk, block_num, buffer) != DISK_SUCCESS) 
        return ERROR_IO;
    
    // update only the specific inode in the buffer
    memcpy(buffer + block_offset, in_inode, sizeof(struct inode));
    
    // write the entire block back to disk
    if (disk_write_block(disk, block_num, buffer) != DISK_SUCCESS) 
        return ERROR_IO;
    
    return SUCCESS;
}


// allocates a free inode of the specified type and updates bitmap
int inode_alloc(disk_t disk, struct bitmap* inode_bitmap, uint8_t type, uint16_t permissions,
                struct inode* out_inode, uint32_t* out_inode_num) {          
    int free_idx = bitmap_find_first_free(inode_bitmap);
    if (free_idx < 0) return ERROR_NO_SPACE;
    
    bitmap_set(inode_bitmap, free_idx);

    struct inode new_inode = {0};
    new_inode.type = type;  // INODE_TYPE_FILE or INODE_TYPE_DIRECTORY
    new_inode.created_time = time(NULL);
    new_inode.modified_time = time(NULL);
    new_inode.accessed_time = time(NULL);
    new_inode.links_count = 1;
    new_inode.size = 0;
    new_inode.blocks_used = 0;
    new_inode.permissions = permissions;
    // direct and indirect pointers are already zeroed by = {0}

    // write inode to disk
    if (inode_write(disk, free_idx, &new_inode) != SUCCESS) {
        // rollback: free the bitmap bit on error
        bitmap_clear(inode_bitmap, free_idx);
        return ERROR_IO;
    }
    
    // return inode and its number if requested
    if (out_inode) *out_inode = new_inode;
    if (out_inode_num) *out_inode_num = free_idx;
    
    return SUCCESS;
}

// frees an inode and updates bitmap
int inode_free(disk_t disk, struct bitmap* inode_bitmap, uint32_t inode_num) {
    bitmap_clear(inode_bitmap, inode_num);

    struct inode zero_inode = {0};
    zero_inode.type = INODE_TYPE_FREE;
    if (inode_write(disk, inode_num, &zero_inode) != SUCCESS) return ERROR_IO;

    return SUCCESS;
}

int inode_is_valid(const struct inode* inode) {
    return inode && inode->type != INODE_TYPE_FREE;
}

void inode_print(const struct inode* inode, uint32_t inode_num) {
    if (!inode) {
        printf("NULL inode\n");
        return;
    }
    printf("Inode #%u:\n", inode_num);
    printf("  Type: %u\n", inode->type);
    printf("  Size: %u bytes\n", inode->size);
    printf("  Links: %u\n", inode->links_count);
    printf("  Permissions: %u\n", inode->permissions);
    printf("  Direct: ");
    for (int i=0; i<12; i++)
        printf("%u ", inode->direct[i]);
    printf("\n  Indirect: %u\n", inode->indirect);
    printf("  Created: "); print_timestamp(inode->created_time); printf("\n");
    printf("  Modified: "); print_timestamp(inode->modified_time); printf("\n");
    printf("  Accessed: "); print_timestamp(inode->accessed_time); printf("\n");
}
