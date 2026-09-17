#pragma once
#include <stdint.h>
#include "limine.h"
#include "uapi/fb.h" // IWYU pragma: export

int64_t fbdev_read(int minor_number, uint64_t offset, uint8_t *buffer, uint64_t size);
int64_t fbdev_write(int minor_number, uint64_t offset, const uint8_t *buffer, uint64_t size);
int fbdev_ioctl(int minor_number, uint64_t request, uint64_t arg);
void fbdev_init(volatile struct limine_framebuffer_request* req);
