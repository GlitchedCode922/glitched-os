#pragma once
#include <stdint.h>
#include <sys/types.h>

typedef struct stat {
    uint64_t size;
    uint64_t ctime;
    uint64_t mtime;
    uint64_t btime;
    uint32_t type;
    dev_t rdev;
} __attribute__((packed)) stat_t;


int stat(const char* path, stat_t* out);
int fstat(int fd, stat_t* out);
