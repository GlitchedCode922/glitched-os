#pragma once
#include <stdint.h>
#include <stdarg.h>

typedef enum {
    OPERATIONAL,
    PANIC,
    STACK_TRACE,
    WAITING_INPUT,
} panic_state_t;

void stack_trace(uint64_t rbp);
__attribute__((noreturn)) void vpanic_int(uint64_t rbp, const char *fmt, va_list args);
__attribute__((noreturn)) void panic_int(uint64_t rbp, const char *fmt, ...);
__attribute__((noreturn)) void panic(const char *fmt, ...);
