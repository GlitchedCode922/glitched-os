#include <errno.h>
#include <stddef.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

int setenv(const char* name, const char* value, int overwrite) {
    if (!environ_copied) {
        if (__copy_environment() < 0) return -1;
    }
    if (strchr(name, '=')) {
        errno = EINVAL;
        return -1;
    }
    size_t nlen = strlen(name);
    size_t index = 0;

    char* string = malloc(nlen + 1 + strlen(value) + 1);
    strcpy(string, name);
    string[nlen] = '=';
    strcpy(string + nlen + 1, value);

    while (environ[index] != NULL) {
        if (strncmp(environ[index], name, nlen) == 0 && environ[index][nlen] == '=') {
            if (!overwrite) {
                free(string);
                return 0;
            }
            // Don't free the old string as the application may still use it
            environ[index] = string;
            return 0;
        }
        index++;
    }

    char** new_environ = realloc(environ, (index + 2) * sizeof(char*));
    if (!new_environ) {
        free(string);
        return -1;
    } else {
        environ = new_environ;
    }
    environ[index] = string;
    environ[index + 1] = NULL;

    return 0;
}
