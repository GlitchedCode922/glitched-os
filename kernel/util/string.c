#include "string.h"
#include <stddef.h>
#include <stdint.h>

int memcmp(const void *ptr1, const void *ptr2, size_t n){
    const unsigned char *p1 = (const unsigned char *)ptr1;
    const unsigned char *p2 = (const unsigned char *)ptr2;

    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return p1[i] - p2[i];
        }
    }
    return 0; // Memory regions are equal
}

void* memcpy(void* restrict dest, const void* restrict src, size_t n) {
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;

    while (n && ((uint64_t)d & 7)) {
        *d++ = *s++;
        n--;
    }

    while (n >= 8) {
        *(uint64_t*)d = *(const uint64_t*)s;
        d += 8;
        s += 8;
        n -= 8;
    }

    while (n--) *d++ = *s++;
    return dest;
}

void* memset(void* ptr, int value, size_t n) {
    unsigned char *p = (unsigned char *)ptr;
    unsigned char c = (unsigned char)value;

    while (n && ((uintptr_t)p & sizeof((uintptr_t) - 1))) {
        *p++ = c;
        n--;
    }

    uint64_t word = 0;
    for (size_t i = 0; i < 8; i++) {
        word = (word << 8) | c;
    }

    uint64_t* w = (uint64_t*)p;
    while (n >= 8) {
        *w++ = word;
        n -= 8;
    }

    p = (unsigned char*)w;
    while (n--) *p++ = c;

    return ptr;
}

void* memmove(void* dest, const void* src, size_t n) {
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;

    if (d < s || d >= s + n) {
        // Non-overlapping regions
        return memcpy(dest, src, n);
    } else {
        // Overlapping regions
        for (size_t i = n; i > 0; i--) {
            d[i - 1] = s[i - 1];
        }
    }
    return dest;
}

size_t strlen(const char* s) {
    size_t len = 0;
    while (s[len] != '\0') len++;
    return len;
}

int strcmp(const char* s1, const char* s2) {
    while (*s1 && *s2 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

char* strcpy(char* dest, const char* src) {
    char* ret = dest;
    while ((*dest++ = *src++)); // copy until '\0'
    return ret;
}

char* strncpy(char* dest, const char* src, size_t len) {
    char* ret = dest;
    int i = 0;
    while ((i++ < len) && (*dest++ = *src++)); // copy until '\0'
    return ret;
}

void strcat(char* dest, const char* src) {
    while (*dest) dest++;
    while (*src) *dest++ = *src++;
    *dest = '\0';
}

int strncmp(const char* s1, const char* s2, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (s1[i] != s2[i]) {
            return (unsigned char)s1[i] - (unsigned char)s2[i];
        }
        if (s1[i] == '\0') {
            return 0;
        }
    }
    return 0;
}

int strncasecmp(const char* s1, const char* s2, size_t n) {
    for (size_t i = 0; i < n; i++) {
        char c1 = (s1[i] >= 'A' && s1[i] <= 'Z') ? (s1[i] + 32) : s1[i];
        char c2 = (s2[i] >= 'A' && s2[i] <= 'Z') ? (s2[i] + 32) : s2[i];
        if (c1 != c2) {
            return (unsigned char)c1 - (unsigned char)c2;
        }
        if (c1 == '\0') {
            return 0;
        }
    }
    return 0;
}

static int digits_u64(uint64_t v) {
    int d = 1;
    while (v >= 10) {
        v /= 10;
        d++;
    }
    return d;
}

void itoa(uint64_t value, char* buffer) {
    int len = digits_u64(value);
    buffer[len] = '\0';

    if (value == 0) {
        buffer[0] = '0';
        return;
    }

    while (len--) {
        buffer[len] = '0' + (value % 10);
        value /= 10;
    }
}
