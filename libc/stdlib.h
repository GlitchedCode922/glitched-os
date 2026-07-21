#pragma once
#include <stddef.h>

void __attribute__((noreturn)) exit(int status);

int atoi(const char* str);
long atol(const char* str);
long long atoll(const char* str);
int abs(int x);
long labs(long x);
long long llabs(long long x);

void* malloc(size_t size);
void* calloc(size_t n, size_t size);
void* realloc(void* ptr, size_t size);
void free(void* ptr);
