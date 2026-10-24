#include <stddef.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

int putenv(char* string) {
    if (!environ_copied) {
        if (__copy_environment() < 0) return -1;
    }
    char* eq = strchr(string, '=');
    if (eq == NULL || eq == string) {
        errno = EINVAL;
        return -1;
    }

    size_t nlen = eq - string;
    size_t index = 0;

    while (environ[index] != NULL) {
        if (strncmp(environ[index], string, nlen + 1) == 0) {
            // Don't free the old string as the application may still use it
            environ[index] = string;
            return 0;
        }
        index++;
    }

    char** new_environ = realloc(environ, (index + 2) * sizeof(char*));
    if (!new_environ) {
        return -1;
    } else {
        environ = new_environ;
    }
    environ[index] = string;
    environ[index + 1] = NULL;

    return 0;
}
