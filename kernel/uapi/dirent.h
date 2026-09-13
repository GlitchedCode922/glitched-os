#pragma once
#include <stdint.h>

enum {
    DT_UNKNOWN = 0,
    DT_FILE = 1,
    DT_DIR = 2,
    DT_BLOCK = 3,
    DT_CHAR = 4,
};

typedef struct {
    char name[256];
    uint32_t type;
} __attribute__((packed)) dirent_t;
