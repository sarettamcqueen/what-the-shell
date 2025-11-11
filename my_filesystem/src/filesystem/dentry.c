#include "dentry.h"
#include "inode.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// === PRIVATE FUNCTIONS ===

// helper: scans a single data block for a matching dentry
static int scan_dentry_block(disk_t disk, uint32_t block_num, const char* name, 
                             struct dentry* out_dentry, uint32_t* out_index, 
                             uint32_t* global_index) {
    if (block_num == 0)
        return ERROR_NOT_FOUND;

    char buffer[BLOCK_SIZE];
    if (disk_read_block(disk, block_num, buffer) != DISK_SUCCESS)
        return ERROR_IO;

    struct dentry* entries = (struct dentry*)buffer;
    for (uint32_t j = 0; j < DENTRIES_PER_BLOCK; j++) {
        if (entries[j].inode_num != 0 && strcmp(entries[j].name, name) == 0) {
            if (out_dentry) *out_dentry = entries[j];
            if (out_index) *out_index = *global_index;
            return SUCCESS;
        }
        (*global_index)++;
    }

    return ERROR_NOT_FOUND;
}

// helper: removes a dentry from a single block if found
static int remove_dentry_from_block(disk_t disk, uint32_t block_num, 
                                    const char* name, bool* found) {
    if (block_num == 0)
        return ERROR_INVALID;
    
    char buffer[BLOCK_SIZE];
    if (disk_read_block(disk, block_num, buffer) != DISK_SUCCESS)
        return ERROR_IO;
    
    struct dentry* entries = (struct dentry*)buffer;
    for (uint32_t j = 0; j < DENTRIES_PER_BLOCK; j++) {
        if (entries[j].inode_num != 0 && strcmp(entries[j].name, name) == 0) {
            // found - mark as free
            memset(&entries[j], 0, sizeof(struct dentry));
            
            if (disk_write_block(disk, block_num, buffer) != DISK_SUCCESS)
                return ERROR_IO;
            
            *found = true;
            return SUCCESS;
        }
    }
    
    *found = false;
    return SUCCESS;
}

// helper: counts valid dentries in a single block
static int count_dentries_in_block(disk_t disk, uint32_t block_num, uint32_t* count) {
    if (block_num == 0) {
        *count = 0;
        return SUCCESS;
    }
    
    char buffer[BLOCK_SIZE];
    if (disk_read_block(disk, block_num, buffer) != DISK_SUCCESS)
        return ERROR_IO;
    
    struct dentry* entries = (struct dentry*)buffer;
    uint32_t local_count = 0;
    
    for (uint32_t j = 0; j < DENTRIES_PER_BLOCK; j++) {
        if (entries[j].inode_num != 0)
            local_count++;
    }
    
    *count = local_count;
    return SUCCESS;
}

// helper: fills array with dentries from a single block
static int fill_dentries_from_block(disk_t disk, uint32_t block_num, 
                                    struct dentry* array, uint32_t* idx) {
    if (block_num == 0)
        return SUCCESS;
    
    char buffer[BLOCK_SIZE];
    if (disk_read_block(disk, block_num, buffer) != DISK_SUCCESS)
        return ERROR_IO;
    
    struct dentry* entries = (struct dentry*)buffer;
    
    for (uint32_t j = 0; j < DENTRIES_PER_BLOCK; j++) {
        if (entries[j].inode_num != 0) {
            array[(*idx)++] = entries[j];
        }
    }
    
    return SUCCESS;
}

// === PUBLIC FUNCTIONS ===

bool dentry_is_valid(const struct dentry* dentry) {
    if (!dentry) return false;
    
    // check 1: name not empty
    if (dentry->name_len == 0 || dentry->name[0] == '\0') 
        return false;
    
    // check 2: name_len coherent with string
    size_t actual_len = strlen(dentry->name);
    if (dentry->name_len != actual_len) 
        return false;
    
    // check 3: valid inode_num
    if (dentry->inode_num == 0) 
        return false;
    
    // check 4: valid file_type
    if (dentry->file_type != INODE_TYPE_FILE && 
        dentry->file_type != INODE_TYPE_DIRECTORY) 
        return false;
    
    return true;
}

bool dentry_is_valid_name(const char* name) {
    if (!name || name[0] == '\0') 
        return false;
    
    size_t len = strlen(name);
    
    // length check
    if (len >= MAX_FILENAME) 
        return false;
    
    // check forbidden characters
    if (strchr(name, '/') != NULL) 
        return false;
    
    // check reserved names
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) 
        return false;
    
    return true;
}

int dentry_create(const char* name, uint32_t inode_num, 
                  uint8_t file_type, struct dentry* out_dentry) {
    if (!name || !out_dentry) 
        return ERROR_INVALID;
    
    if (!dentry_is_valid_name(name)) {
        memset(out_dentry, 0, sizeof(*out_dentry));
        return ERROR_INVALID;
    }
    
    if (file_type != INODE_TYPE_FILE && 
        file_type != INODE_TYPE_DIRECTORY) {
        memset(out_dentry, 0, sizeof(*out_dentry));
        return ERROR_INVALID;
    }
    
    if (inode_num == 0) {
        memset(out_dentry, 0, sizeof(*out_dentry));
        return ERROR_INVALID;
    }
    
    memset(out_dentry, 0, sizeof(*out_dentry));
    
    // populates struct fields
    out_dentry->inode_num = inode_num;
    out_dentry->file_type = file_type;
    out_dentry->name_len = strlen(name);
    strncpy(out_dentry->name, name, MAX_FILENAME - 1);
    out_dentry->name[MAX_FILENAME - 1] = '\0';  // safety (since strncpy doesn't automatically null-terminate buffer)
    
    return SUCCESS;
}

int dentry_find(disk_t disk, uint32_t dir_inode_num, 
                const char* name, struct dentry* out_dentry, 
                uint32_t* out_index) {
    if (!disk || !name) 
        return ERROR_INVALID;
    
    // read directory inode
    struct inode dir_inode;
    if (inode_read(disk, dir_inode_num, &dir_inode) != SUCCESS) 
        return ERROR_IO;
    
    // verify it's a directory
    if (dir_inode.type != INODE_TYPE_DIRECTORY) 
        return ERROR_INVALID;
    
    uint32_t dentry_index = 0;
    int result;
    
    // scan direct blocks
    for (uint32_t i = 0; i < 12 && dir_inode.direct[i] != 0; i++) {
        result = scan_dentry_block(disk, dir_inode.direct[i], name, 
                                   out_dentry, out_index, &dentry_index);
        if (result == SUCCESS)
            return SUCCESS;
        if (result == ERROR_IO)
            return ERROR_IO;
    }
    
    // scan indirect block (if exists)
    if (dir_inode.indirect != 0) {
        char indirect_buffer[BLOCK_SIZE];
        if (disk_read_block(disk, dir_inode.indirect, indirect_buffer) != DISK_SUCCESS)
            return ERROR_IO;
        
        uint32_t* block_ptrs = (uint32_t*)indirect_buffer;
        uint32_t max_ptrs = BLOCK_SIZE / sizeof(uint32_t);  // 128 pointers
        
        for (uint32_t i = 0; i < max_ptrs && block_ptrs[i] != 0; i++) {
            result = scan_dentry_block(disk, block_ptrs[i], name, 
                                       out_dentry, out_index, &dentry_index);
            if (result == SUCCESS)
                return SUCCESS;
            if (result == ERROR_IO)
                return ERROR_IO;
        }
    }
    
    return ERROR_NOT_FOUND;
}

int dentry_add(disk_t disk, uint32_t dir_inode_num, 
               const struct dentry* new_dentry,
               struct bitmap* block_bitmap) {
    if (!disk || !new_dentry || !block_bitmap) 
        return ERROR_INVALID;
    
    if (!dentry_is_valid(new_dentry)) 
        return ERROR_INVALID;
    
    // read directory inode
    struct inode dir_inode;
    if (inode_read(disk, dir_inode_num, &dir_inode) != SUCCESS) 
        return ERROR_IO;
    
    if (dir_inode.type != INODE_TYPE_DIRECTORY) 
        return ERROR_INVALID;
    
    // check if entry already exists
    if (dentry_find(disk, dir_inode_num, new_dentry->name, NULL, NULL) == SUCCESS) 
        return ERROR_EXISTS;
    
    // find first free slot (in existing direct blocks or allocate new one)
    char buffer[BLOCK_SIZE];
    
    for (uint32_t i = 0; i < 12; i++) {
        // if this slot has no block allocated yet
        if (dir_inode.direct[i] == 0) {
            // allocate a new block
            int new_block = bitmap_find_first_free(block_bitmap);
            if (new_block < 0)
                return ERROR_NO_SPACE;  // disk full
            
            // mark block as used
            if (bitmap_set(block_bitmap, new_block) != SUCCESS)
                return ERROR_GENERIC;
            
            // update directory inode
            dir_inode.direct[i] = new_block;
            dir_inode.blocks_used++;
            dir_inode.modified_time = time(NULL);
            
            // write updated inode to disk
            if (inode_write(disk, dir_inode_num, &dir_inode) != SUCCESS) {
                // rollback: free the block
                bitmap_clear(block_bitmap, new_block);
                return ERROR_IO;
            }
            
            // initialize new block (all zero = all dentry slots free)
            memset(buffer, 0, BLOCK_SIZE);
            struct dentry* entries = (struct dentry*)buffer;
            
            // put new dentry in first slot
            entries[0] = *new_dentry;
            
            // write block to disk
            if (disk_write_block(disk, new_block, buffer) != DISK_SUCCESS) {
                // rollback: revert inode changes and free block
                dir_inode.direct[i] = 0;
                dir_inode.blocks_used--;
                inode_write(disk, dir_inode_num, &dir_inode);
                bitmap_clear(block_bitmap, new_block);
                return ERROR_IO;
            }
            
            return SUCCESS;
        }
        
        // block exists, check for free slot inside it
        if (disk_read_block(disk, dir_inode.direct[i], buffer) != DISK_SUCCESS)
            return ERROR_IO;
        
        struct dentry* entries = (struct dentry*)buffer;
        for (uint32_t j = 0; j < DENTRIES_PER_BLOCK; j++) {
            if (entries[j].inode_num == 0) {
                // found free slot in existing block
                entries[j] = *new_dentry;
                
                if (disk_write_block(disk, dir_inode.direct[i], buffer) != DISK_SUCCESS)
                    return ERROR_IO;
                
                // update directory modification time
                dir_inode.modified_time = time(NULL);
                inode_write(disk, dir_inode_num, &dir_inode);
                
                return SUCCESS;
            }
        }
    }
    
    // if all 12 direct blocks are full, try indirect block
    if (dir_inode.indirect == 0) {
        // allocate indirect block
        int indirect_block = bitmap_find_first_free(block_bitmap);
        if (indirect_block < 0)
            return ERROR_NO_SPACE;
        
        bitmap_set(block_bitmap, indirect_block);
        
        dir_inode.indirect = indirect_block;
        dir_inode.blocks_used++;
        
        if (inode_write(disk, dir_inode_num, &dir_inode) != SUCCESS) {
            bitmap_clear(block_bitmap, indirect_block);
            return ERROR_IO;
        }
        
        // initialize indirect block (all zeros = no data blocks allocated)
        char indirect_buffer[BLOCK_SIZE];
        memset(indirect_buffer, 0, BLOCK_SIZE);
        disk_write_block(disk, indirect_block, indirect_buffer);
    }
    
    // read indirect block
    char indirect_buffer[BLOCK_SIZE];
    if (disk_read_block(disk, dir_inode.indirect, indirect_buffer) != DISK_SUCCESS)
        return ERROR_IO;
    
    uint32_t* block_ptrs = (uint32_t*)indirect_buffer;
    uint32_t max_ptrs = BLOCK_SIZE / sizeof(uint32_t);  // 128
    
    // search for free slot in indirect blocks
    for (uint32_t i = 0; i < max_ptrs; i++) {
        if (block_ptrs[i] == 0) {
            // allocate new data block
            int new_block = bitmap_find_first_free(block_bitmap);
            if (new_block < 0)
                return ERROR_NO_SPACE;
            
            bitmap_set(block_bitmap, new_block);
            
            // update indirect block
            block_ptrs[i] = new_block;
            if (disk_write_block(disk, dir_inode.indirect, indirect_buffer) != DISK_SUCCESS) {
                bitmap_clear(block_bitmap, new_block);
                return ERROR_IO;
            }
            
            // update inode
            dir_inode.blocks_used++;
            dir_inode.modified_time = time(NULL);
            inode_write(disk, dir_inode_num, &dir_inode);
            
            // initialize new data block and add dentry
            char buffer[BLOCK_SIZE];
            memset(buffer, 0, BLOCK_SIZE);
            struct dentry* entries = (struct dentry*)buffer;
            entries[0] = *new_dentry;
            
            if (disk_write_block(disk, new_block, buffer) != DISK_SUCCESS) {
                // rollback
                block_ptrs[i] = 0;
                disk_write_block(disk, dir_inode.indirect, indirect_buffer);
                bitmap_clear(block_bitmap, new_block);
                return ERROR_IO;
            }
            
            return SUCCESS;
        }
        
        // check for free slot in existing indirect data block
        char buffer[BLOCK_SIZE];
        if (disk_read_block(disk, block_ptrs[i], buffer) != DISK_SUCCESS)
            return ERROR_IO;
        
        struct dentry* entries = (struct dentry*)buffer;
        for (uint32_t j = 0; j < DENTRIES_PER_BLOCK; j++) {
            if (entries[j].inode_num == 0) {
                entries[j] = *new_dentry;
                
                if (disk_write_block(disk, block_ptrs[i], buffer) != DISK_SUCCESS)
                    return ERROR_IO;
                
                dir_inode.modified_time = time(NULL);
                inode_write(disk, dir_inode_num, &dir_inode);
                
                return SUCCESS;
            }
        }
    }
    
    return ERROR_NO_SPACE;
}

int dentry_remove(disk_t disk, uint32_t dir_inode_num, const char* name) {
    if (!disk || !name) 
        return ERROR_INVALID;
    
    struct inode dir_inode;
    if (inode_read(disk, dir_inode_num, &dir_inode) != SUCCESS) 
        return ERROR_IO;
    
    if (dir_inode.type != INODE_TYPE_DIRECTORY) 
        return ERROR_INVALID;
    
    bool found = false;
    int result;
    
    // search in direct blocks
    for (uint32_t i = 0; i < 12 && dir_inode.direct[i] != 0; i++) {
        result = remove_dentry_from_block(disk, dir_inode.direct[i], name, &found);
        if (result != SUCCESS)
            return result;
        if (found) {
            dir_inode.modified_time = time(NULL);
            inode_write(disk, dir_inode_num, &dir_inode);
            return SUCCESS;
        }
    }
    
    // search in indirect blocks
    if (dir_inode.indirect != 0) {
        char indirect_buffer[BLOCK_SIZE];
        if (disk_read_block(disk, dir_inode.indirect, indirect_buffer) != DISK_SUCCESS)
            return ERROR_IO;
        
        uint32_t* block_ptrs = (uint32_t*)indirect_buffer;
        uint32_t max_ptrs = BLOCK_SIZE / sizeof(uint32_t);
        
        for (uint32_t i = 0; i < max_ptrs && block_ptrs[i] != 0; i++) {
            result = remove_dentry_from_block(disk, block_ptrs[i], name, &found);
            if (result != SUCCESS)
                return result;
            if (found) {
                dir_inode.modified_time = time(NULL);
                inode_write(disk, dir_inode_num, &dir_inode);
                return SUCCESS;
            }
        }
    }
    
    return ERROR_NOT_FOUND;
}

int dentry_list(disk_t disk, uint32_t dir_inode_num, 
                struct dentry** out_entries, uint32_t* out_count) {
    if (!disk || !out_entries || !out_count) 
        return ERROR_INVALID;
    
    struct inode dir_inode;
    if (inode_read(disk, dir_inode_num, &dir_inode) != SUCCESS) 
        return ERROR_IO;
    
    if (dir_inode.type != INODE_TYPE_DIRECTORY) 
        return ERROR_INVALID;
    
    // === COUNT PHASE ===
    uint32_t total_count = 0;
    uint32_t block_count;
    
    // count in direct blocks
    for (uint32_t i = 0; i < 12 && dir_inode.direct[i] != 0; i++) {
        if (count_dentries_in_block(disk, dir_inode.direct[i], &block_count) != SUCCESS)
            return ERROR_IO;
        total_count += block_count;
    }
    
    // count in indirect blocks
    if (dir_inode.indirect != 0) {
        char indirect_buffer[BLOCK_SIZE];
        if (disk_read_block(disk, dir_inode.indirect, indirect_buffer) != DISK_SUCCESS)
            return ERROR_IO;
        
        uint32_t* block_ptrs = (uint32_t*)indirect_buffer;
        uint32_t max_ptrs = BLOCK_SIZE / sizeof(uint32_t);
        
        for (uint32_t i = 0; i < max_ptrs && block_ptrs[i] != 0; i++) {
            if (count_dentries_in_block(disk, block_ptrs[i], &block_count) != SUCCESS)
                return ERROR_IO;
            total_count += block_count;
        }
    }
    
    // empty directory
    if (total_count == 0) {
        *out_entries = NULL;
        *out_count = 0;
        return SUCCESS;
    }
    
    // === ALLOCATION PHASE ===
    struct dentry* result = malloc(total_count * sizeof(struct dentry));
    if (!result) 
        return ERROR_GENERIC;
    
    uint32_t idx = 0;
    
    // fill from direct blocks
    for (uint32_t i = 0; i < 12 && dir_inode.direct[i] != 0; i++) {
        if (fill_dentries_from_block(disk, dir_inode.direct[i], result, &idx) != SUCCESS) {
            free(result);
            return ERROR_IO;
        }
    }
    
    // fill from indirect blocks
    if (dir_inode.indirect != 0) {
        char indirect_buffer[BLOCK_SIZE];
        if (disk_read_block(disk, dir_inode.indirect, indirect_buffer) != DISK_SUCCESS) {
            free(result);
            return ERROR_IO;
        }
        
        uint32_t* block_ptrs = (uint32_t*)indirect_buffer;
        uint32_t max_ptrs = BLOCK_SIZE / sizeof(uint32_t);
        
        for (uint32_t i = 0; i < max_ptrs && block_ptrs[i] != 0; i++) {
            if (fill_dentries_from_block(disk, block_ptrs[i], result, &idx) != SUCCESS) {
                free(result);
                return ERROR_IO;
            }
        }
    }
    
    *out_entries = result;
    *out_count = total_count;
    return SUCCESS;
}

void dentry_print(const struct dentry* dentry) {
    if (!dentry) {
        printf("NULL dentry\n");
        return;
    }
    
    printf("Dentry:\n");
    printf("  Name: %s (len=%u)\n", dentry->name, dentry->name_len);
    printf("  Inode: %u\n", dentry->inode_num);
    printf("  Type: %s\n", 
           dentry->file_type == INODE_TYPE_FILE ? "FILE" : 
           dentry->file_type == INODE_TYPE_DIRECTORY ? "DIR" : "UNKNOWN");
}
