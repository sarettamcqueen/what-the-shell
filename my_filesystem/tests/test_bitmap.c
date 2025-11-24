/* 
    Temporary test for bitmap module
*/

#include "bitmap.h"
#include <stdio.h>
#include <assert.h>

void test_create_destroy() {
    printf("Test: create and destroy... ");
    
    struct bitmap* bmp = bitmap_create(100);
    assert(bmp != NULL);
    assert(bitmap_count_free(bmp) == 100);
    
    bitmap_destroy(&bmp);
    printf("OK\n");
}

void test_set_clear_get() {
    printf("Test: set, clear, get... ");
    
    struct bitmap* bmp = bitmap_create(64);
    
    // every bit has to be free initially
    for (int i = 0; i < 64; i++) {
        assert(!bitmap_get(bmp, i));
    }
    
    // sets a few bits
    bitmap_set(bmp, 0);
    bitmap_set(bmp, 10);
    bitmap_set(bmp, 63);
    
    assert(bitmap_get(bmp, 0));
    assert(bitmap_get(bmp, 10));
    assert(bitmap_get(bmp, 63));
    assert(!bitmap_get(bmp, 5));
    
    // clear
    bitmap_clear(bmp, 10);
    assert(!bitmap_get(bmp, 10));
    
    bitmap_destroy(&bmp);
    printf("OK\n");
}

void test_find_operations() {
    printf("Test: find operations... ");
    
    struct bitmap* bmp = bitmap_create(100);
    
    bitmap_set(bmp, 0);
    bitmap_set(bmp, 5);
    bitmap_set(bmp, 10);
    
    int first_free = bitmap_find_first_free(bmp);
    assert(first_free == 1);
    
    int first_used = bitmap_find_first_used(bmp);
    assert(first_used == 0);
    
    int next_free = bitmap_find_next_free(bmp, 6);
    assert(next_free == 6);
    
    bitmap_destroy(&bmp);
    printf("OK\n");
}

void test_count() {
    printf("Test: count operations... ");
    
    struct bitmap* bmp = bitmap_create(100);
    
    assert(bitmap_count_free(bmp) == 100);
    assert(bitmap_count_used(bmp) == 0);
    
    bitmap_set(bmp, 10);
    bitmap_set(bmp, 20);
    bitmap_set(bmp, 30);
    
    assert(bitmap_count_free(bmp) == 97);
    assert(bitmap_count_used(bmp) == 3);
    
    bitmap_destroy(&bmp);
    printf("OK\n");
}

void test_range_operations() {
    printf("Test: range operations... ");
    
    struct bitmap* bmp = bitmap_create(100);
    
    bitmap_set_range(bmp, 10, 20);
    
    for (int i = 10; i < 30; i++) {
        assert(bitmap_get(bmp, i));
    }
    assert(!bitmap_get(bmp, 9));
    assert(!bitmap_get(bmp, 30));
    
    bitmap_clear_range(bmp, 15, 10);
    for (int i = 15; i < 25; i++) {
        assert(!bitmap_get(bmp, i));
    }
    
    bitmap_destroy(&bmp);
    printf("OK\n");
}

int main() {
    printf("=== Bitmap Tests ===\n\n");
    
    test_create_destroy();
    test_set_clear_get();
    test_find_operations();
    test_count();
    test_range_operations();
    
    printf("\nAll bitmap tests pass!\n");
    return 0;
}
