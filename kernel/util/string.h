// IWYU pragma: always_keep
#pragma once
#include <stddef.h>
#include <stdint.h>

void* memcpy(void* dest, const void* src, size_t n);
void* memset(void* ptr, int value, size_t n);
void* memmove(void* dest, const void* src, size_t n);
int memcmp(const void* ptr1, const void* ptr2, size_t n);

size_t strlen(const char* s);
int strcmp(const char* s1, const char* s2);
char* strcpy(char* dest, const char* src);
char* strncpy(char* dest, const char* src, size_t len);
void strcat(char* dest, const char* src);
int strncmp(const char* s1, const char* s2, size_t n);
int strncasecmp(const char* s1, const char* s2, size_t n);

void itoa(uint64_t value, char* buffer);
