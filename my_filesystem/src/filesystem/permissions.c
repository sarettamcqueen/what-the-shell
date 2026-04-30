#include "permissions.h"

bool perm_can_read(const struct inode* inode) {
    if (!inode) return false;
    return (inode->permissions & PERM_READ) != 0;
}

bool perm_can_write(const struct inode* inode) {
    if (!inode) return false;
    return (inode->permissions & PERM_WRITE) != 0;
}

bool perm_can_execute(const struct inode* inode) {
    if (!inode) return false;
    return (inode->permissions & PERM_EXEC) != 0;
}