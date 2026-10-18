#pragma once
#include <uapi/fd.h> // IWYU pragma: export
#include <unistd.h>

int open(const char* path, uint16_t flags, ...);
int fcntl(int fd, int cmd, ...);
