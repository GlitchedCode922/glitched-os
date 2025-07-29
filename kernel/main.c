#include "limine.h"
#include "console.h"
#include "panic.h"
#include "drivers/kbd.h"
#include "memory/mman.h"
#include "memory/paging.h"
#include "drivers/block/ata.h"
#include "idt.h"
#include <stdint.h>

extern uint64_t __size;

__attribute__((used, section(".limine_requests_start")))
static volatile LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests")))
static volatile LIMINE_BASE_REVISION(3);

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_stack_size_request stack_size_request = {
    .id = LIMINE_STACK_SIZE_REQUEST,
    .revision = 0,
    .stack_size = 0x1000000 // 10 MiB
};

__attribute__((used, section(".limine_requests_end")))
static volatile LIMINE_REQUESTS_END_MARKER;

void kernel_main() {
    uintptr_t cr3;
    asm volatile(
        "mov %%cr3, %0"
        : "=r"(cr3)
    );
    init_paging(cr3, memmap_request.response, hhdm_request.response->offset);
    init_mman((size_t)&__size);
    idt_init();
    initialize_console(framebuffer_request.response->framebuffers[0]);

    unsigned char* disk_sector = kmalloc(512);
    scan_for_devices();
    read_sectors(0, 0, disk_sector, 1);
    kprintf("Disk sector read successfully.\n");
    kprintf("Disk sector data:\n");
    kprintf("%s\n", disk_sector);
    get_smart_data(0, disk_sector);
    kprintf("SMART data read successfully.\n");
    kprintf("SMART data:\n");
    for (int i = 0; i < 512; i++) {
        kprintf_dec(disk_sector[i]);
        kprintf(" ");
    }
    kfree(disk_sector);

    while (1) {
        char* line = kbdinput();
        if (line != NULL) kprintf(line);
    }
}
