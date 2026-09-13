#pragma once
#include <stdint.h>
#include <uapi/ioctl.h> // IWYU pragma: export

int ioctl(int fd, uint64_t request, ...);
