#pragma once
#include <stdint.h>

typedef struct {
    uint64_t boot_time;
    uint64_t uptime_ms;
} __attribute__((packed)) vvar_t;
