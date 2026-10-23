#include <string.h>
#include <stdlib.h>
#include <stddef.h>

char* strdup(const char* s) {
    size_t len = strlen(s) + 1;
    char* copy = malloc(len);
    if (copy) memcpy(copy, s, len);
    return copy;
}

char* strndup(const char* s, size_t n) {
    size_t len = strlen(s) + 1;
    if (len > n + 1) len = n + 1;
    char* copy = malloc(len);
    if (copy) memcpy(copy, s, len);
    return copy;
}
