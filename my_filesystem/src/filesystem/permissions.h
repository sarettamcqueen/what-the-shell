#pragma once

#include "common.h"
#include <stdbool.h>

// === PERMISSION BITS (rwxrwxrwx format) ===
#define PERM_OWNER_R    0400
#define PERM_OWNER_W    0200
#define PERM_OWNER_X    0100
#define PERM_GROUP_R    0040
#define PERM_GROUP_W    0020
#define PERM_GROUP_X    0010
#define PERM_OTHER_R    0004
#define PERM_OTHER_W    0002
#define PERM_OTHER_X    0001

// we only use "owner" permissions (first 3 bits) since we have no users/groups
#define PERM_READ       PERM_OWNER_R
#define PERM_WRITE      PERM_OWNER_W
#define PERM_EXEC       PERM_OWNER_X

/**
 * Checks if a file/directory inode allows read access.
 * 
 * @param inode The inode to check
 * @return true if read is allowed, false otherwise
 */
bool perm_can_read(const struct inode* inode);

/**
 * Checks if a file/directory inode allows write access.
 * 
 * @param inode The inode to check
 * @return true if write is allowed, false otherwise
 */
bool perm_can_write(const struct inode* inode);

/**
 * Checks if a directory inode allows execute (traverse) access.
 * Execute permission on directories controls the ability to cd into them
 * and to look up files within them.
 * 
 * @param inode The inode to check
 * @return true if execute is allowed, false otherwise
 */
bool perm_can_execute(const struct inode* inode);