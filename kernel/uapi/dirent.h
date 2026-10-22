#pragma once
#include <stdint.h>

enum {
    DT_UNKNOWN = 0,
    DT_FILE = 1,
    DT_DIR = 2,
    DT_BLOCK = 3,
    DT_CHAR = 4,
};

#define POSIX_COMPAT_UNION(type, name1, name2) \
    union { \
        type name1; \
        type name2; \
    }

typedef struct dirent {
    POSIX_COMPAT_UNION(char, name[256], d_name[256]);
    POSIX_COMPAT_UNION(uint32_t, type, d_type);
} __attribute__((packed)) dirent_t;

#undef POSIX_COMPAT_UNION
