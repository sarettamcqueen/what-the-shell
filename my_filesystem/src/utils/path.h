#pragma once

#include "common.h"
#include <stdbool.h>

// === CONSTANTS ===
#define PATH_SEPARATOR '/'
#define CURRENT_DIR "."
#define PARENT_DIR ".."

// === PATH PARSING STRUCTURE ===
struct path_components {
    char** components;      // array of path components
    int count;              // number of components
    bool is_absolute;       // true if it starts with /
};

// === PUBLIC FUNCTIONS ===

// path parsing
struct path_components* path_parse(const char* path);
int path_split(const char* path, char* parent, char* filename);
void path_components_free(struct path_components* pc);

// validation
bool path_is_absolute(const char* path);
bool path_is_root(const char* path);
bool path_is_valid(const char* path);
bool filename_is_valid(const char* filename);

// path resolution
char* path_get_basename(const char* path);
char* path_get_dirname(const char* path);

// path normalization
char* path_normalize(const char* path);

// utility functions
void path_print_components(const struct path_components* pc);
int path_depth(const char* path);
bool path_starts_with(const char* path, const char* prefix);

// conversions
char* path_components_to_string(const struct path_components* pc);