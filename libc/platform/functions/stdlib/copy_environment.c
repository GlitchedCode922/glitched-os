#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int __copy_environment() {
    size_t envc = 0;
    while (environ[envc++]);
    char** new_environ = malloc((envc + 1) * sizeof(char*));
    if (new_environ == NULL) return -1;
    if (new_environ == NULL) return -1;
    for (size_t i = 0; i < envc; i++) new_environ[i] = environ[i];
    new_environ[envc] = NULL;
    environ = new_environ;
    environ_copied = 1;
    return 0;
}