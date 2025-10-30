#pragma once

#include "common.h"
#include <stdint.h>
#include <stdbool.h>

// === BITMAP STRUCTURE ===
struct bitmap {
    uint8_t* data;          // array of bits
    size_t size_bits;       // total number of bits
    size_t size_bytes;      // number of bytes needed
};

// === PUBLIC FUNCTIONS ===

// initialization and cleanup
struct bitmap* bitmap_create(size_t num_bits);
void bitmap_destroy(struct bitmap* bmp);
int bitmap_init_from_memory(struct bitmap* bmp, void* memory, size_t num_bits);

// bit operations
bool bitmap_get(const struct bitmap* bmp, size_t bit_index);
int bitmap_set(struct bitmap* bmp, size_t bit_index);
int bitmap_clear(struct bitmap* bmp, size_t bit_index);
int bitmap_toggle(struct bitmap* bmp, size_t bit_index);

// bulk operations
void bitmap_set_all(struct bitmap* bmp);
void bitmap_clear_all(struct bitmap* bmp);
int bitmap_set_range(struct bitmap* bmp, size_t start, size_t count);
int bitmap_clear_range(struct bitmap* bmp, size_t start, size_t count);

// search operations
int bitmap_find_first_free(const struct bitmap* bmp);
int bitmap_find_next_free(const struct bitmap* bmp, size_t start_from);
int bitmap_find_first_used(const struct bitmap* bmp);
int bitmap_count_free(const struct bitmap* bmp);
int bitmap_count_used(const struct bitmap* bmp);

// utility functions
void bitmap_print(const struct bitmap* bmp, size_t max_bits_to_show);
bool bitmap_is_valid_index(const struct bitmap* bmp, size_t bit_index);