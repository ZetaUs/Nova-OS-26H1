#ifndef ASC48_MAP_H
#define ASC48_MAP_H

#define ASC48_CHAR_SIZE 288
#define ASC48_CHAR_COUNT 63

// ASCII码(0~127) -> 点阵在bin中的下标，-1=不存在
static const int8_t asc48_lookup[128] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, -1, -1, -1, -1, -1,
    -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1,
    -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,
};

static inline uint8_t* asc48_get_bitmap(char c, uint8_t *base) {
    uint8_t code = (uint8_t)c;
    if(code >= 128) return NULL;
    int idx = asc48_lookup[code];
    if(idx == -1) return NULL;
    return base + idx * ASC48_CHAR_SIZE;
}

#endif