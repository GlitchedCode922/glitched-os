#include "limine.h"
#include "console.h"
#include "panic.h"
#include "drivers/kbd.h"
#include "memory/mman.h"
#include "memory/paging.h"
#include "drivers/block/ata.h"
#include "fs/fat32.h"
#include "idt.h"
#include "drivers/partitions/mbr.h"
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
    ata_register();

    if (has_mbr(0) == 0) {
        select_partition(0, 0);
        init_fat32(0, 0);
        kprintf("FAT32 initialized on disk 0, partition 0.\n");
    } else {
        kprintf("No MBR found on disk 0.\n");
    }

    char* buffer = (char*)kmalloc(13);
    int i = 0;
    do {
        lsdir("TEST", buffer, i);
        if (*buffer) kprintf("Directory entry %d: %s\n", i, buffer);
        i++;
    } while (*buffer);

    if (file_exists("TEST/HELLO")) {
        kprintf("File 'HELLO' exists in 'TEST' directory.\n");
    } else {
        kprintf("File 'HELLO' does not exist in 'TEST' directory.\n");
    }

    if (file_exists("TEST/TEXT.TXT")) {
        kprintf("File 'TEXT.TXT' exists in 'TEST' directory.\n");
    } else {
        kprintf("File 'TEXT.TXT' does not exist in 'TEST' directory.\n");
    }

    while (1) {
        char* line = kbdinput();
        if (line != NULL) kprintf(line);
    }
}
