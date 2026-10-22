#include <string.h>
#include <stdlib.h>
#include <stddef.h>

char* strdup(const char* s) {
    size_t len = strlen(s) + 1;
    char* copy = malloc(len);
    if (copy) memcpy(copy, s, len);
    return copy;
}
