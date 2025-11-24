#include "bitmap.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// === MACROS FOR BIT-LEVEL OPS ===
#define BYTE_INDEX(bit)  ((bit) / 8)
#define BIT_OFFSET(bit)  ((bit) % 8)
#define BIT_MASK(bit)    (1U << BIT_OFFSET(bit))

// === PRIVATE FUNCTIONS ===

// calculates how many bytes are needed for num_bits
static inline size_t bits_to_bytes(size_t num_bits) {
    return (num_bits + 7) / 8;  // equivalent to ALIGN_TO_8(num_bits) / 8
}

// === INITIALIZATION AND CLEANUP ===

struct bitmap* bitmap_create(size_t num_bits) {
    if (num_bits == 0) {
        return NULL;
    }
    
    struct bitmap* bmp = malloc(sizeof(struct bitmap));
    if (!bmp) {
        return NULL;
    }
    
    bmp->size_bits = num_bits;
    bmp->size_bytes = bits_to_bytes(num_bits);
    
    bmp->data = calloc(bmp->size_bytes, 1);  // initializes to 0 (all bits free)
    if (!bmp->data) {
        free(bmp);
        return NULL;
    }
    
    return bmp;
}

void bitmap_destroy(struct bitmap** bmp) {
    if (!bmp || !(*bmp)) {
        return;
    }
    
    if ((*bmp)->data) {
        free((*bmp)->data);
    }
    free(*bmp);
    *bmp = NULL;
}

int bitmap_init_from_memory(struct bitmap* bmp, void* memory, size_t num_bits) {
    if (!bmp || !memory || num_bits == 0) {
        return ERROR_INVALID;
    }
    
    bmp->data = (uint8_t*)memory;
    bmp->size_bits = num_bits;
    bmp->size_bytes = bits_to_bytes(num_bits);
    
    return SUCCESS;
}

// === BIT OPERATIONS ===

bool bitmap_get(const struct bitmap* bmp, size_t bit_index) {
    if (!bitmap_is_valid_index(bmp, bit_index)) {
        return false;
    }
    
    size_t byte_idx = BYTE_INDEX(bit_index);
    uint8_t mask = BIT_MASK(bit_index);
    
    return (bmp->data[byte_idx] & mask) != 0;
}

int bitmap_set(struct bitmap* bmp, size_t bit_index) {
    if (!bitmap_is_valid_index(bmp, bit_index)) {
        return ERROR_INVALID;
    }
    
    size_t byte_idx = BYTE_INDEX(bit_index);
    uint8_t mask = BIT_MASK(bit_index);
    
    bmp->data[byte_idx] |= mask;
    
    return SUCCESS;
}

int bitmap_clear(struct bitmap* bmp, size_t bit_index) {
    if (!bitmap_is_valid_index(bmp, bit_index)) {
        return ERROR_INVALID;
    }
    
    size_t byte_idx = BYTE_INDEX(bit_index);
    uint8_t mask = BIT_MASK(bit_index);
    
    bmp->data[byte_idx] &= ~mask;
    
    return SUCCESS;
}

int bitmap_toggle(struct bitmap* bmp, size_t bit_index) {
    if (!bitmap_is_valid_index(bmp, bit_index)) {
        return ERROR_INVALID;
    }
    
    size_t byte_idx = BYTE_INDEX(bit_index);
    uint8_t mask = BIT_MASK(bit_index);
    
    bmp->data[byte_idx] ^= mask;
    
    return SUCCESS;
}

// === BULK OPERATIONS ===

void bitmap_set_all(struct bitmap* bmp) {
    if (!bmp || !bmp->data) {
        return;
    }
    
    memset(bmp->data, 0xFF, bmp->size_bytes);
}

void bitmap_clear_all(struct bitmap* bmp) {
    if (!bmp || !bmp->data) {
        return;
    }
    
    memset(bmp->data, 0x00, bmp->size_bytes);
}

int bitmap_set_range(struct bitmap* bmp, size_t start, size_t count) {
    if (!bmp || !bmp->data) {
        return ERROR_INVALID;
    }
    
    if (start >= bmp->size_bits || start + count > bmp->size_bits) {
        return ERROR_INVALID;
    }
    
    for (size_t i = 0; i < count; i++) {
        bitmap_set(bmp, start + i);
    }
    
    return SUCCESS;
}

int bitmap_clear_range(struct bitmap* bmp, size_t start, size_t count) {
    if (!bmp || !bmp->data) {
        return ERROR_INVALID;
    }
    
    if (start >= bmp->size_bits || start + count > bmp->size_bits) {
        return ERROR_INVALID;
    }
    
    for (size_t i = 0; i < count; i++) {
        bitmap_clear(bmp, start + i);
    }
    
    return SUCCESS;
}

// === SEARCH OPERATIONS ===

int bitmap_find_first_free(const struct bitmap* bmp) {
    return bitmap_find_next_free(bmp, 1);  // skips bit 0 (block 0 is superblock and inode 0 is reserved)
}

int bitmap_find_next_free(const struct bitmap* bmp, size_t start_from) {
    if (!bmp || !bmp->data || start_from >= bmp->size_bits) {
        return ERROR_NOT_FOUND;
    }
    
    // optimization: scans byte per byte
    for (size_t i = start_from; i < bmp->size_bits; i++) {
        if (!bitmap_get(bmp, i)) {
            return (int)i;
        }
    }
    
    return ERROR_NOT_FOUND;
}

int bitmap_find_first_used(const struct bitmap* bmp) {
    if (!bmp || !bmp->data) {
        return ERROR_NOT_FOUND;
    }
    
    for (size_t i = 0; i < bmp->size_bits; i++) {
        if (bitmap_get(bmp, i)) {
            return (int)i;
        }
    }
    
    return ERROR_NOT_FOUND;
}

int bitmap_count_free(const struct bitmap* bmp) {
    if (!bmp || !bmp->data) {
        return 0;
    }
    
    int count = 0;
    for (size_t i = 0; i < bmp->size_bits; i++) {
        if (!bitmap_get(bmp, i)) {
            count++;
        }
    }
    
    return count;
}

int bitmap_count_used(const struct bitmap* bmp) {
    if (!bmp || !bmp->data) {
        return 0;
    }
    
    return (int)bmp->size_bits - bitmap_count_free(bmp);
}

// === UTILITY FUNCTIONS ===

void bitmap_print(const struct bitmap* bmp, size_t max_bits_to_show) {
    if (!bmp || !bmp->data) {
        printf("Bitmap: NULL\n");
        return;
    }
    
    size_t limit = MIN(bmp->size_bits, max_bits_to_show);
    
    printf("Bitmap [%zu bits, %zu bytes]:\n", bmp->size_bits, bmp->size_bytes);
    printf("  ");
    
    for (size_t i = 0; i < limit; i++) {
        printf("%c", bitmap_get(bmp, i) ? '1' : '0');
        if ((i + 1) % 64 == 0) {
            printf("\n  ");
        } else if ((i + 1) % 8 == 0) {
            printf(" ");
        }
    }
    
    if (limit < bmp->size_bits) {
        printf("... (%zu more bits)", bmp->size_bits - limit);
    }
    printf("\n");
}

bool bitmap_is_valid_index(const struct bitmap* bmp, size_t bit_index) {
    if (!bmp || !bmp->data) {
        return false;
    }
    return bit_index < bmp->size_bits;
}
