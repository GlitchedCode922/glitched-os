#pragma once
#include <sys/types.h>
#include <stdint.h>
#include <uapi/dirent.h> // IWYU pragma: export

typedef struct {
    int fd;
    dirent_t* output;
} DIR;

DIR* opendir(const char* path);
DIR* fdopendir(int fd);
dirent_t* readdir(DIR* stream);
void rewinddir(DIR* stream);
off_t telldir(DIR* stream);
void seekdir(DIR* stream, off_t loc);
int closedir(DIR* stream);
