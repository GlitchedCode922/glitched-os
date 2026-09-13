#pragma once
#include <stdint.h>

typedef struct {
    char* ascii[128];
    uint8_t width;
    uint8_t height;
} __attribute__((packed)) font_t;
