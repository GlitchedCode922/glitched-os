#pragma once
#include <stdint.h>

typedef uint64_t dev_t;

typedef struct {
    uint64_t size;
    uint64_t ctime;
    uint64_t mtime;
    uint64_t btime;
    uint32_t type;
    dev_t rdev;
} __attribute__((packed)) stat_t;
