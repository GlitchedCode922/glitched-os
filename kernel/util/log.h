#pragma once
#include <stdarg.h>
#include <stdint.h>

typedef enum {
    LOG_DEBUG = 0,
    LOG_INFO = 1,
    LOG_WARN = 2,
    LOG_ERROR = 3,
    LOG_FATAL = 4,
} log_level_t;

extern log_level_t minimum_level;
extern log_level_t minimum_debugcon_level;
extern log_level_t minimum_serial_level;
extern log_level_t minimum_console_level;

void klog(log_level_t level, const char* subsystem, const char* string);
void kvlogf(log_level_t level, const char* subsystem, const char* format, va_list args);
void klogf(log_level_t level, const char* subsystem, const char* format, ...);
