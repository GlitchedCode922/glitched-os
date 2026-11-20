#pragma once
#include "usermode/scheduler.h" // IWYU pragma: keep

#define spin(condition, timeout, body) ({ \
    int result = 0; \
    uint64_t start = vvar_vaddr->uptime_ms; \
    while (condition) { \
        body; \
        if ((timeout) && vvar_vaddr->uptime_ms > start + (timeout)) { \
            result = -1;\
            break; \
        } \
        asm volatile("pause"); \
    } \
    result; \
})
