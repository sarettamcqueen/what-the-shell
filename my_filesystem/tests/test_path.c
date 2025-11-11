/*
    test for path module
*/

#include "path.h"
#include "common.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

// === TEST FUNCTIONS ===

void test_path_parse() {
    printf("test: path_parse()... ");
    
    // test 1: absolute path
    struct path_components* pc = path_parse("/home/user/file.txt");
    assert(pc != NULL);
    assert(pc->is_absolute == true);
    assert(pc->count == 3);
    assert(strcmp(pc->components[0], "home") == 0);
    assert(strcmp(pc->components[1], "user") == 0);
    assert(strcmp(pc->components[2], "file.txt") == 0);
    path_components_free(pc);
    
    // test 2: relative path
    pc = path_parse("docs/readme.txt");
    assert(pc != NULL);
    assert(pc->is_absolute == false);
    assert(pc->count == 2);
    assert(strcmp(pc->components[0], "docs") == 0);
    assert(strcmp(pc->components[1], "readme.txt") == 0);
    path_components_free(pc);
    
    // test 3: root
    pc = path_parse("/");
    assert(pc != NULL);
    assert(pc->is_absolute == true);
    assert(pc->count == 0);
    path_components_free(pc);
    
    // test 4: multiple slashes
    pc = path_parse("/home//user///file.txt");
    assert(pc != NULL);
    assert(pc->count == 3);
    assert(strcmp(pc->components[0], "home") == 0);
    path_components_free(pc);
    
    // test 5: single component
    pc = path_parse("file.txt");
    assert(pc != NULL);
    assert(pc->is_absolute == false);
    assert(pc->count == 1);
    assert(strcmp(pc->components[0], "file.txt") == 0);
    path_components_free(pc);
    
    // test 6: NULL/empty
    pc = path_parse(NULL);
    assert(pc == NULL);
    
    pc = path_parse("");
    assert(pc == NULL);
    
    printf("OK\n");
}

void test_path_split() {
    printf("test: path_split()... ");
    
    char parent[MAX_PATH], filename[MAX_FILENAME];
    
    // test 1: normal absolute path
    assert(path_split("/home/user/file.txt", parent, filename) == SUCCESS);
    assert(strcmp(parent, "/home/user") == 0);
    assert(strcmp(filename, "file.txt") == 0);
    
    // test 2: root parent
    assert(path_split("/file.txt", parent, filename) == SUCCESS);
    assert(strcmp(parent, "/") == 0);
    assert(strcmp(filename, "file.txt") == 0);
    
    // test 3: relative path
    assert(path_split("file.txt", parent, filename) == SUCCESS);
    assert(strcmp(parent, ".") == 0);
    assert(strcmp(filename, "file.txt") == 0);
    
    // test 4: relative path with subdirectory
    assert(path_split("docs/file.txt", parent, filename) == SUCCESS);
    assert(strcmp(parent, "docs") == 0);
    assert(strcmp(filename, "file.txt") == 0);
    
    // test 5: trailing slash
    assert(path_split("/home/user/", parent, filename) == SUCCESS);
    assert(strcmp(parent, "/home") == 0);
    assert(strcmp(filename, "user") == 0);

    
    // test 6: empty path (should fail)
    assert(path_split("", parent, filename) == ERROR_INVALID);
    
    // test 7: root only
    assert(path_split("/", parent, filename) == ERROR_INVALID);
    
    printf("OK\n");
}

void test_path_components_free() {
    printf("test: path_components_free()... ");
    
    // test with valid structure
    struct path_components* pc = path_parse("/home/user/file.txt");
    assert(pc != NULL);
    path_components_free(pc);  // should not crash
    
    // test with NULL (should not crash)
    path_components_free(NULL);
    
    printf("OK\n");
}

void test_path_is_absolute() {
    printf("test: path_is_absolute()... ");
    
    assert(path_is_absolute("/home/user") == true);
    assert(path_is_absolute("/") == true);
    assert(path_is_absolute("home/user") == false);
    assert(path_is_absolute("file.txt") == false);
    assert(path_is_absolute("") == false);
    assert(path_is_absolute(NULL) == false);
    
    printf("OK\n");
}

void test_path_is_root() {
    printf("test: path_is_root()... ");
    
    assert(path_is_root("/") == true);
    assert(path_is_root("//") == true);
    assert(path_is_root("///") == true);
    assert(path_is_root("/home") == false);
    assert(path_is_root("home") == false);
    assert(path_is_root("") == false);
    assert(path_is_root(NULL) == false);
    
    printf("OK\n");
}

void test_path_is_valid() {
    printf("test: path_is_valid()... ");
    
    // valid paths
    assert(path_is_valid("/home/user/file.txt") == true);
    assert(path_is_valid("relative/path.txt") == true);
    assert(path_is_valid("/") == true);
    assert(path_is_valid("file.txt") == true);
    assert(path_is_valid("my-file_123.txt") == true);
    
    // invalid paths
    assert(path_is_valid("") == false);
    assert(path_is_valid(NULL) == false);
    
    printf("OK\n");
}

void test_filename_is_valid() {
    printf("test: filename_is_valid()... ");
    
    // valid filenames
    assert(filename_is_valid("file.txt") == true);
    assert(filename_is_valid("my-file_123.txt") == true);
    assert(filename_is_valid("a") == true);
    assert(filename_is_valid("README") == true);
    
    // invalid filenames
    assert(filename_is_valid("") == false);
    assert(filename_is_valid(NULL) == false);
    assert(filename_is_valid(".") == false);
    assert(filename_is_valid("..") == false);
    assert(filename_is_valid("file/name") == false);  // contains separator
    assert(filename_is_valid("file\nname") == false); // control character
    
    printf("OK\n");
}

void test_path_get_basename() {
    printf("test: path_get_basename()... ");
    
    char* base;
    
    // test 1: normal path
    base = path_get_basename("/home/user/file.txt");
    assert(strcmp(base, "file.txt") == 0);
    free(base);
    
    // test 2: trailing slash
    base = path_get_basename("/home/user/");
    assert(strcmp(base, "user") == 0);
    free(base);
    
    // test 3: relative path
    base = path_get_basename("file.txt");
    assert(strcmp(base, "file.txt") == 0);
    free(base);
    
    // test 4: root
    base = path_get_basename("/");
    assert(strcmp(base, "/") == 0);
    free(base);
    
    // test 5: single directory
    base = path_get_basename("/home");
    assert(strcmp(base, "home") == 0);
    free(base);
    
    // test 6: empty (becomes ".")
    base = path_get_basename("");
    assert(strcmp(base, ".") == 0);
    free(base);
    
    printf("OK\n");
}

void test_path_get_dirname() {
    printf("test: path_get_dirname()... ");
    
    char* dir;
    
    // test 1: normal path
    dir = path_get_dirname("/home/user/file.txt");
    assert(strcmp(dir, "/home/user") == 0);
    free(dir);
    
    // test 2: root parent
    dir = path_get_dirname("/home");
    assert(strcmp(dir, "/") == 0);
    free(dir);
    
    // test 3: relative path
    dir = path_get_dirname("file.txt");
    assert(strcmp(dir, ".") == 0);
    free(dir);
    
    // test 4: relative with subdirectory
    dir = path_get_dirname("docs/file.txt");
    assert(strcmp(dir, "docs") == 0);
    free(dir);
    
    // test 5: root
    dir = path_get_dirname("/");
    assert(strcmp(dir, "/") == 0);
    free(dir);
    
    // test 6: trailing slash
    dir = path_get_dirname("/home/user/");
    assert(strcmp(dir, "/home") == 0);
    free(dir);
    
    printf("OK\n");
}

void test_path_normalize() {
    printf("test: path_normalize()... ");
    
    char* norm;
    
    // test 1: with "." and ".."
    norm = path_normalize("/home/./user/../root");
    printf("\nDEBUG: got '%s', expected '/home/root'\n", norm);
    assert(strcmp(norm, "/home/root") == 0);
    free(norm);
    
    // test 2: relative path with ".."
    norm = path_normalize("docs/../src/./file.c");
    assert(strcmp(norm, "src/file.c") == 0);
    free(norm);
    
    // test 3: multiple slashes
    norm = path_normalize("//usr///bin");
    assert(strcmp(norm, "/usr/bin") == 0);
    free(norm);
    
    // test 4: root
    norm = path_normalize("/");
    assert(strcmp(norm, "/") == 0);
    free(norm);
    
    // test 5: ".." at root (should be ignored)
    norm = path_normalize("/../home");
    assert(strcmp(norm, "/home") == 0);
    free(norm);
    
    // test 6: only "."
    norm = path_normalize("./");
    assert(strcmp(norm, ".") == 0);
    free(norm);
    
    // test 7: relative ".." that can't go back
    norm = path_normalize("../file.txt");
    assert(strcmp(norm, "../file.txt") == 0);
    free(norm);
    
    // test 8: complex relative
    norm = path_normalize("a/b/../c/./d");
    assert(strcmp(norm, "a/c/d") == 0);
    free(norm);
    
    printf("OK\n");
}

void test_path_print_components() {
    printf("test: path_print_components()... ");
    
    // test with valid structure
    struct path_components* pc = path_parse("/home/user/file.txt");
    printf("\n");
    path_print_components(pc);
    path_components_free(pc);
    
    // test with NULL
    path_print_components(NULL);  // should not crash
    
    printf("OK\n");
}

void test_path_depth() {
    printf("test: path_depth()... ");
    
    assert(path_depth("/") == 0);
    assert(path_depth("/home") == 1);
    assert(path_depth("/home/user") == 2);
    assert(path_depth("/home/user/file.txt") == 3);
    assert(path_depth("docs/file.txt") == 2);
    assert(path_depth("file.txt") == 1);
    assert(path_depth(NULL) == -1);
    
    printf("OK\n");
}

void test_path_starts_with() {
    printf("test: path_starts_with()... ");
    
    // test 1: exact match
    assert(path_starts_with("/home/user/docs", "/home/user") == true);
    
    // test 2: not a match
    assert(path_starts_with("/home/user", "/home/other") == false);
    
    // test 3: exact same path
    assert(path_starts_with("/home/user", "/home/user") == true);
    
    // test 4: prefix is longer
    assert(path_starts_with("/home", "/home/user") == false);
    
    // test 5: with normalization
    assert(path_starts_with("/home/./user/docs", "/home/user") == true);
    
    // test 6: relative paths
    assert(path_starts_with("docs/readme.txt", "docs") == true);
    
    // test 7: NULL inputs
    assert(path_starts_with(NULL, "/home") == false);
    assert(path_starts_with("/home", NULL) == false);
    
    printf("OK\n");
}

void test_path_components_to_string() {
    printf("test: path_components_to_string()... ");
    
    // test 1: absolute path
    struct path_components* pc = path_parse("/home/user/file.txt");
    char* str = path_components_to_string(pc);
    assert(strcmp(str, "/home/user/file.txt") == 0);
    free(str);
    path_components_free(pc);
    
    // test 2: relative path
    pc = path_parse("docs/readme.txt");
    str = path_components_to_string(pc);
    assert(strcmp(str, "docs/readme.txt") == 0);
    free(str);
    path_components_free(pc);
    
    // test 3: root
    pc = path_parse("/");
    str = path_components_to_string(pc);
    assert(strcmp(str, "/") == 0);
    free(str);
    path_components_free(pc);
    
    // test 4: single component
    pc = path_parse("file.txt");
    str = path_components_to_string(pc);
    assert(strcmp(str, "file.txt") == 0);
    free(str);
    path_components_free(pc);
    
    // test 5: NULL
    str = path_components_to_string(NULL);
    assert(str == NULL);
    
    printf("OK\n");
}

void test_edge_cases() {
    printf("test: edge cases... ");
    
    // very long path (near MAX_PATH)
    char long_path[MAX_PATH];
    memset(long_path, 'a', MAX_PATH - 2);
    long_path[0] = '/';
    long_path[MAX_PATH - 2] = '\0';
    
    struct path_components* pc = path_parse(long_path);
    assert(pc != NULL);
    path_components_free(pc);
    
    // path with dots
    pc = path_parse("/home/.hidden/..config");
    assert(pc != NULL);
    assert(pc->count == 3);
    path_components_free(pc);
    
    // multiple consecutive slashes
    pc = path_parse("////home////user////");
    assert(pc != NULL);
    assert(pc->count == 2);
    path_components_free(pc);
    
    printf("OK\n");
}

// === MAIN ===

int main() {
    printf("=== Path Module Tests ===\n\n");
    
    test_path_parse();
    test_path_split();
    test_path_components_free();
    test_path_is_absolute();
    test_path_is_root();
    test_path_is_valid();
    test_filename_is_valid();
    test_path_get_basename();
    test_path_get_dirname();
    test_path_normalize();
    test_path_print_components();
    test_path_depth();
    test_path_starts_with();
    test_path_components_to_string();
    test_edge_cases();
    
    printf("\nAll path tests pass!\n");
    return 0;
}
