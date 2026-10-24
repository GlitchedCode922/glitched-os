#include <string.h>
#include <stdlib.h>
#include <stddef.h>

char* strdup(const char* s) {
    size_t len = strlen(s) + 1;
    char* copy = malloc(len);
    if (copy) memcpy(copy, s, len);
    return copy;
}

char *strndup(const char *s, size_t n) {
    size_t len = 0;
    while (len < n && s[len] != '\0') len++;
    char *copy = malloc(len + 1);
    if (copy == NULL) return NULL;
    memcpy(copy, s, len);
    copy[len] = '\0';
    return copy;
}
