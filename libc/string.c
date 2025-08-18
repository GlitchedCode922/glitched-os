#include "string.h"
#include <stddef.h>

size_t strlen(const char *s) {
    size_t len = 0;
    while (s[len] != '\0') {
        len++;
    }
    return len;
}

char* strcpy(char *dest, const char *src) {
    char *ptr = dest;
    while ((*ptr++ = *src++) != '\0');
    return dest;
}

char* strncpy(char *dest, const char *src, size_t n) {
    char* orig_dest = dest;
    int s = 0;
    while (s < n && src[s] != 0) {
        dest[s] = src[s];
        s++;
    }
    if (s < n) dest[s] = '\0';
    return dest;
}

char* strcat(char* dest, const char* src) {
    char* ptr = dest;

    while (*ptr != '\0') {
        ptr++;
    }

    while (*src != '\0') {
        *ptr = *src;
        ptr++;
        src++;
    }

    *ptr = '\0';
    return dest;
}

char* strncat(char* dest, const char* src, size_t n) {
    char* ptr = dest;

    while (*ptr != '\0') {
        ptr++;
    }

    while (*src != '\0' && n > 0) {
        *ptr = *src;
        ptr++;
        src++;
        n--;
    }

    *ptr = '\0';
    return dest;
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 && *s2 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    while (*s1 && *s2 && (*s1 == *s2) && n > 0) {
        s1++;
        s2++;
        n--;
    }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

char* strchr(const char *s, int c) {
    while (*s) {
        if (*s == c) {
            return (char*)s;
        }
        s++;
    }

    if (c == '\0') {
        return (char*)s;
    }

    return NULL;
}
char* strrchr(const char *s, int c) {
    char* last = NULL;

    while (*s) {
        if (*s == c) {
            last = (char*)s;
        }
        s++;
    }

    if (c == '\0') {
        return (char*)s;
    }

    return last;
}

char* strstr(const char *haystack, const char *needle) {
    if (*needle == '\0') return (char*)haystack; // empty needle

    for (; *haystack != '\0'; haystack++) {
        const char *h = haystack;
        const char *n = needle;

        // Compare the substring
        while (*h && *n && *h == *n) {
            h++;
            n++;
        }

        if (*n == '\0') {
            return (char*)haystack; // found full needle
        }
    }

    return NULL; // not found
}

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

void* memcpy(void* dest, const void* src, size_t n) {
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;

    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
    return dest;
}

void* memset(void* ptr, unsigned char value, size_t n) {
    unsigned char *p = (unsigned char *)ptr;

    for (size_t i = 0; i < n; i++) {
        p[i] = value;
    }
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
