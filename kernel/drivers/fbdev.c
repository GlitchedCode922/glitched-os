#include "fbdev.h"
#include "chrdev.h"
#include "../memory/mman.h"
#include "../error.h"
#include "../ioctl_list.h"

static volatile struct limine_framebuffer_response* fb_response;

static int digits_u64(uint64_t v) {
    int d = 1;
    while (v >= 10) {
        v /= 10;
        d++;
    }
    return d;
}

static void itoa(uint64_t value, char* buffer) {
    int len = digits_u64(value);
    buffer[len] = '\0';

    if (value == 0) {
        buffer[0] = '0';
        return;
    }

    while (len--) {
        buffer[len] = '0' + (value % 10);
        value /= 10;
    }
}

void fbdev_init(volatile struct limine_framebuffer_request *req) {
    fb_response = req->response;
    char_driver_t fbdev_driver = {
        .read = fbdev_read,
        .write = fbdev_write,
        .ioctl = fbdev_ioctl,
    };
    int driver_index = register_char_driver(&fbdev_driver);
    if (driver_index <= 0) return;
    for (int i = 0; i < fb_response->framebuffer_count; i++) {
        char* name = kmalloc(64);
        name[0] = 'f';
        name[1] = 'b';
        itoa(i, name + 2);
        char_device_t dev = {
            .major_number = driver_index,
            .minor_number = i,
            .name = name,
        };
        register_char_device(&dev);
    }
}

int fbdev_read(int minor_number, uint64_t offset, uint8_t *buffer, uint64_t size) {
    if (minor_number < 0 || minor_number >= fb_response->framebuffer_count) return -ENODEV;
    volatile struct limine_framebuffer* fb = fb_response->framebuffers[minor_number];
    if (offset > fb->height * fb->pitch) return 0;
    if (offset + size > fb->height * fb->pitch) size = fb->height * fb->pitch - offset;
    memcpy(buffer, fb->address + offset, size);
    return size;
}

int fbdev_write(int minor_number, uint64_t offset, const uint8_t *buffer, uint64_t size) {
    if (minor_number < 0 || minor_number >= fb_response->framebuffer_count) return -ENODEV;
    volatile struct limine_framebuffer* fb = fb_response->framebuffers[minor_number];
    if (offset > fb->height * fb->pitch) return 0;
    if (offset + size > fb->height * fb->pitch) size = fb->height * fb->pitch - offset;
    memcpy(fb->address + offset, buffer, size);
    return size;
}

int fbdev_ioctl(int minor_number, uint64_t request, uint64_t arg) {
    if (minor_number < 0 || minor_number >= fb_response->framebuffer_count) return -ENODEV;
    volatile struct limine_framebuffer* fb = fb_response->framebuffers[minor_number];
    if (request != FB_GET_INFO) return -ENOTTY;
    framebuffer_info_t* buffer = (framebuffer_info_t*)arg;
    *buffer = (framebuffer_info_t){
        .width = fb->width,
        .height = fb->height,
        .pitch = fb->pitch,
        .bpp = fb->bpp,
        .memory_model = fb->memory_model,
        .red_mask_size = fb->red_mask_size,
        .red_mask_shift = fb->red_mask_shift,
        .green_mask_size = fb->green_mask_size,
        .green_mask_shift = fb->green_mask_shift,
        .blue_mask_size = fb->blue_mask_size,
        .blue_mask_shift = fb->blue_mask_shift,
    };
    return 0;
}
