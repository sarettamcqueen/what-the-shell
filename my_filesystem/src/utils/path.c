#include "path.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

// === PRIVATE FUNCTIONS ===

// helper: counts the number of path separators in a string
static int count_separators(const char* path) {
    int count = 0;
    for (const char* p = path; *p; p++) {
        if (*p == PATH_SEPARATOR)
            count++;
    }
    return count;
}

// helper: removes trailing slashes from a path (except for root "/")
static void remove_trailing_slashes(char* path) {
    if (!path || !path[0])
        return;
    
    size_t len = strlen(path);
    while (len > 1 && path[len - 1] == PATH_SEPARATOR) {
        path[len - 1] = '\0';
        len--;
    }
}

// === PATH PARSING ===

struct path_components* path_parse(const char* path) {
    if (!path || path[0] == '\0')
        return NULL;
    
    struct path_components* pc = malloc(sizeof(struct path_components));
    if (!pc)
        return NULL;
    
    pc->is_absolute = (path[0] == PATH_SEPARATOR);
    pc->count = 0;
    pc->components = NULL;
    
    // make a working copy
    char temp[MAX_PATH];
    strncpy(temp, path, MAX_PATH - 1);
    temp[MAX_PATH - 1] = '\0';
    
    // count components (non-empty tokens between separators)
    char temp_count[MAX_PATH];
    strncpy(temp_count, temp, MAX_PATH - 1);
    
    char* token = strtok(temp_count, "/");
    int count = 0;
    while (token) {
        if (token[0] != '\0')  // skip empty tokens
            count++;
        token = strtok(NULL, "/");
    }
    
    if (count == 0) {
        // root path "/" or empty relative path
        return pc;
    }
    
    // allocate components array
    pc->components = malloc(count * sizeof(char*));
    if (!pc->components) {
        free(pc);
        return NULL;
    }
    
    // fill components
    token = strtok(temp, "/");
    int idx = 0;
    while (token) {
        if (token[0] != '\0') {
            pc->components[idx] = strdup(token);
            if (!pc->components[idx]) {
                // cleanup on error
                for (int i = 0; i < idx; i++)
                    free(pc->components[i]);
                free(pc->components);
                free(pc);
                return NULL;
            }
            idx++;
        }
        token = strtok(NULL, "/");
    }
    
    pc->count = count;
    return pc;
}

int path_split(const char* path, char* parent, char* filename) {
    if (!path || !parent || !filename)
        return ERROR_INVALID;
    
    // handle empty path
    if (path[0] == '\0') {
        strcpy(parent, CURRENT_DIR);
        filename[0] = '\0';
        return ERROR_INVALID;
    }
    
    // make working copy and remove trailing slashes
    char temp[MAX_PATH];
    strncpy(temp, path, MAX_PATH - 1);
    temp[MAX_PATH - 1] = '\0';
    remove_trailing_slashes(temp);
    
    // find last separator
    const char* last_slash = strrchr(temp, PATH_SEPARATOR);
    
    if (!last_slash) {
        // no separator: relative path, parent is current directory
        strcpy(parent, CURRENT_DIR);
        strncpy(filename, temp, MAX_FILENAME - 1);
        filename[MAX_FILENAME - 1] = '\0';
        return SUCCESS;
    }
    
    // extract parent (everything before last slash)
    size_t parent_len = last_slash - temp;
    if (parent_len == 0) {
        // root is the parent
        strcpy(parent, "/");
    } else {
        if (parent_len >= MAX_PATH)
            return ERROR_INVALID;
        strncpy(parent, temp, parent_len);
        parent[parent_len] = '\0';
    }
    
    // extract filename (everything after last slash)
    const char* name_start = last_slash + 1;
    if (name_start[0] == '\0') {
        // path ended with slash, no filename
        return ERROR_INVALID;
    }
    
    strncpy(filename, name_start, MAX_FILENAME - 1);
    filename[MAX_FILENAME - 1] = '\0';
    
    return SUCCESS;
}

void path_components_free(struct path_components* pc) {
    if (!pc)
        return;
    
    if (pc->components) {
        for (int i = 0; i < pc->count; i++) {
            if (pc->components[i])
                free(pc->components[i]);
        }
        free(pc->components);
    }
    
    free(pc);
}

// === PATH VALIDATION ===

bool path_is_absolute(const char* path) {
    return path && path[0] == PATH_SEPARATOR;
}

bool path_is_root(const char* path) {
    if (!path)
        return false;
    
    // root is "/" possibly with trailing slashes
    if (path[0] != PATH_SEPARATOR)
        return false;
    
    for (const char* p = path + 1; *p; p++) {
        if (*p != PATH_SEPARATOR)
            return false;
    }
    
    return true;
}

bool path_is_valid(const char* path) {
    if (!path || path[0] == '\0')
        return false;
    
    size_t len = strlen(path);
    if (len >= MAX_PATH)
        return false;
    
    // check for invalid characters (basic check)
    for (size_t i = 0; i < len; i++) {
        char c = path[i];
        // disallow null bytes and control characters
        if (c == '\0' || (c > 0 && c < 32 && c != PATH_SEPARATOR))
            return false;
    }
    
    // parse and validate each component
    struct path_components* pc = path_parse(path);
    if (!pc)
        return false;
    
    bool valid = true;
    for (int i = 0; i < pc->count; i++) {
        if (!filename_is_valid(pc->components[i]) &&
            strcmp(pc->components[i], CURRENT_DIR) != 0 &&
            strcmp(pc->components[i], PARENT_DIR) != 0) {
            valid = false;
            break;
        }
    }
    
    path_components_free(pc);
    return valid;
}

bool filename_is_valid(const char* filename) {
    if (!filename || filename[0] == '\0')
        return false;
    
    size_t len = strlen(filename);
    if (len >= MAX_FILENAME)
        return false;
    
    // disallow "." and ".." as filenames (they're special)
    if (strcmp(filename, CURRENT_DIR) == 0 || strcmp(filename, PARENT_DIR) == 0)
        return false;
    
    // disallow path separators in filename
    if (strchr(filename, PATH_SEPARATOR) != NULL)
        return false;
    
    // check for invalid characters
    for (size_t i = 0; i < len; i++) {
        char c = filename[i];
        // disallow control characters and null
        if (c == '\0' || (c > 0 && c < 32))
            return false;
    }
    
    return true;
}

// === PATH EXTRACTION ===

char* path_get_basename(const char* path) {
    if (!path)
        return NULL;
    
    // make working copy
    char temp[MAX_PATH];
    strncpy(temp, path, MAX_PATH - 1);
    temp[MAX_PATH - 1] = '\0';
    remove_trailing_slashes(temp);
    
    // handle empty or root
    if (temp[0] == '\0')
        return strdup(".");
    if (strcmp(temp, "/") == 0)
        return strdup("/");
    
    const char* last_slash = strrchr(temp, PATH_SEPARATOR);
    if (!last_slash) {
        return strdup(temp);
    }
    
    return strdup(last_slash + 1);
}

char* path_get_dirname(const char* path) {
    if (!path)
        return NULL;
    
    // make working copy
    char temp[MAX_PATH];
    strncpy(temp, path, MAX_PATH - 1);
    temp[MAX_PATH - 1] = '\0';
    remove_trailing_slashes(temp);
    
    // handle empty
    if (temp[0] == '\0')
        return strdup(".");
    
    // handle root
    if (strcmp(temp, "/") == 0)
        return strdup("/");
    
    const char* last_slash = strrchr(temp, PATH_SEPARATOR);
    if (!last_slash) {
        return strdup(CURRENT_DIR);
    }
    
    // root is parent
    if (last_slash == temp) {
        return strdup("/");
    }
    
    size_t len = last_slash - temp;
    char* dirname = malloc(len + 1);
    if (!dirname)
        return NULL;
    
    strncpy(dirname, temp, len);
    dirname[len] = '\0';
    
    return dirname;
}

// === PATH NORMALIZATION ===

char* path_normalize(const char* path) {
    if (!path)
        return NULL;
    
    struct path_components* pc = path_parse(path);
    if (!pc)
        return NULL;
    
    // allocate array for normalized components
    char** normalized = malloc(pc->count * sizeof(char*));
    if (!normalized) {
        path_components_free(pc);
        return NULL;
    }
    
    int norm_count = 0;
    
    // process each component
    for (int i = 0; i < pc->count; i++) {
        if (strcmp(pc->components[i], CURRENT_DIR) == 0) {
            // skip "."
            continue;
        } else if (strcmp(pc->components[i], PARENT_DIR) == 0) {
            // go back one level (if possible)
            if (norm_count > 0 && strcmp(normalized[norm_count - 1], PARENT_DIR) != 0) {
                free(normalized[norm_count - 1]);
                norm_count--;
            } else if (!pc->is_absolute) {
                // for relative paths, keep ".." if can't go back
                normalized[norm_count++] = strdup(PARENT_DIR);
            }
            // for absolute paths, ignore ".." at root
        } else {
            // normal component
            normalized[norm_count++] = strdup(pc->components[i]);
        }
    }
    
    // build result path
    char* result = malloc(MAX_PATH);
    if (!result) {
        for (int i = 0; i < norm_count; i++)
            free(normalized[i]);
        free(normalized);
        path_components_free(pc);
        return NULL;
    }
    
    result[0] = '\0';
    
    if (pc->is_absolute) {
        strcpy(result, "/");
    }
    
    for (int i = 0; i < norm_count; i++) {
        if (i > 0) {
            strcat(result, "/");
        }
        strcat(result, normalized[i]);
        free(normalized[i]);
    }
    
    // handle empty result
    if (result[0] == '\0') {
        strcpy(result, pc->is_absolute ? "/" : ".");
    }
    
    free(normalized);
    path_components_free(pc);
    
    return result;
}

// === UTILITY FUNCTIONS ===

void path_print_components(const struct path_components* pc) {
    if (!pc) {
        printf("Path components: NULL\n");
        return;
    }
    
    printf("Path components:\n");
    printf("  Is absolute: %s\n", pc->is_absolute ? "yes" : "no");
    printf("  Count: %d\n", pc->count);
    
    if (pc->count > 0) {
        printf("  Components:\n");
        for (int i = 0; i < pc->count; i++) {
            printf("    [%d] \"%s\"\n", i, pc->components[i]);
        }
    } else {
        printf("  (no components)\n");
    }
}

int path_depth(const char* path) {
    if (!path)
        return -1;
    
    struct path_components* pc = path_parse(path);
    if (!pc)
        return -1;
    
    int depth = pc->count;
    path_components_free(pc);
    
    return depth;
}

bool path_starts_with(const char* path, const char* prefix) {
    if (!path || !prefix)
        return false;
    
    // normalize both paths first
    char* norm_path = path_normalize(path);
    char* norm_prefix = path_normalize(prefix);
    
    if (!norm_path || !norm_prefix) {
        free(norm_path);
        free(norm_prefix);
        return false;
    }
    
    size_t prefix_len = strlen(norm_prefix);
    bool result = (strncmp(norm_path, norm_prefix, prefix_len) == 0);
    
    // if prefix doesn't end with '/', ensure path has '/' after prefix
    if (result && prefix_len < strlen(norm_path)) {
        if (norm_prefix[prefix_len - 1] != PATH_SEPARATOR &&
            norm_path[prefix_len] != PATH_SEPARATOR) {
            result = false;
        }
    }
    
    free(norm_path);
    free(norm_prefix);
    
    return result;
}

char* path_components_to_string(const struct path_components* pc) {
    if (!pc)
        return NULL;
    
    char* result = malloc(MAX_PATH);
    if (!result)
        return NULL;
    
    result[0] = '\0';
    
    if (pc->is_absolute) {
        strcpy(result, "/");
    }
    
    for (int i = 0; i < pc->count; i++) {
        if (i > 0) {
            strcat(result, "/");
        }
        strcat(result, pc->components[i]);
    }
    
    // handle empty path
    if (result[0] == '\0') {
        strcpy(result, ".");
    }
    
    return result;
}
