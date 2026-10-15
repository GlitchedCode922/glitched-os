#pragma once
#include <stdint.h>

typedef struct {
    uint64_t boot_time;
    uint64_t uptime_ms;
    uint8_t signal_trampoline[16];
} __attribute__((packed)) vvar_t;
