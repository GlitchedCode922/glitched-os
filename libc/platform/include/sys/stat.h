#pragma once
#include <sys/types.h>
#include <uapi/stat.h> // IWYU pragma: export

int stat(const char* path, stat_t* out);
int fstat(int fd, stat_t* out);
