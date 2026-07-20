#pragma once
#include <stdint.h>
#include "../limine.h"

typedef struct {
    uint64_t width;
    uint64_t height;
    uint64_t pitch;
    uint16_t bpp;
    uint8_t memory_model;
    uint8_t red_mask_size;
    uint8_t red_mask_shift;
    uint8_t green_mask_size;
    uint8_t green_mask_shift;
    uint8_t blue_mask_size;
    uint8_t blue_mask_shift;
} __attribute__((packed)) framebuffer_info_t;

int64_t fbdev_read(int minor_number, uint64_t offset, uint8_t *buffer, uint64_t size);
int64_t fbdev_write(int minor_number, uint64_t offset, const uint8_t *buffer, uint64_t size);
int fbdev_ioctl(int minor_number, uint64_t request, uint64_t arg);
void fbdev_init(volatile struct limine_framebuffer_request* req);
