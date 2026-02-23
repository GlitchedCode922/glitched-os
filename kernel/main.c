#include "limine.h"
#include "console.h"
#include "panic.h"
#include "memory/mman.h"
#include "memory/paging.h"
#include "drivers/block/ata.h"
#include "mount.h"
#include "gdt.h"
#include "idt.h"
#include "drivers/partitions/mbr.h"
#include "drivers/net/rtl8139.h"
#include "usermode/elf.h"
#include "io/pci.h"
#include "drivers/serial.h"
#include "usermode/scheduler.h"
#include "user_jump.h"
#include <stdint.h>

extern uint64_t __size;
extern void init_fpu();

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

__attribute__((used, section(".limine_requests")))
static volatile struct limine_executable_cmdline_request cmdline_request = {
    .id = LIMINE_EXECUTABLE_CMDLINE_REQUEST,
    .revision = 0
};

__attribute__((used, section(".limine_requests_end")))
static volatile LIMINE_REQUESTS_END_MARKER;

extern volatile struct limine_framebuffer* framebuffer;
volatile struct limine_framebuffer* framebuffer;

void parse_kernel_cmdline() {
    uint8_t root_disk = 0;
    uint8_t root_partition = 0;
    uint8_t root_readonly = 0;
    char init_binary_path[64] = "/bin/init";
    if (cmdline_request.response && cmdline_request.response->cmdline) {
        char* cmdline = cmdline_request.response->cmdline;
        while (*cmdline) {
            if (*cmdline == ' ') {
                cmdline++;
                continue;
            }
            if (cmdline[0] == 'r' && cmdline[1] == 'o' && cmdline[2] != 'o') {
                root_readonly = 1;
                cmdline += 2;
                continue;
            }

            if (cmdline[0] == 'r' && cmdline[1] == 'w' && cmdline[2] == ' ') {
                root_readonly = 0;
                cmdline += 2;
                continue;
            }

            if (cmdline[0] == 'r' && cmdline[1] == 'o' && cmdline[2] == 'o' && cmdline[3] == 't' && cmdline[4] == '=') {
                cmdline += 5;

                while (*cmdline == ' ') cmdline++;

                if (*cmdline != '(') {
                    panic("Invalid root= parameter format, expected root=(disk,partition)");
                }

                cmdline++;

                // Parse root=(disk,partition)
                while ((*cmdline) >= '0' && *cmdline <= '9') {
                    root_disk = root_disk * 10 + (*cmdline - '0');
                    cmdline++;
                }

                while (*cmdline == ',' || *cmdline == ' ') cmdline++;

                // Read the second number
                while (*cmdline >= '0' && *cmdline <= '9') {
                    root_partition = root_partition * 10 + (*cmdline - '0');
                    cmdline++;
                }

                while (*cmdline == ' ') cmdline++;
                cmdline++; // Skip the closing ')'

                continue;
            }


            if (cmdline[0] == 'i' && cmdline[1] == 'n' && cmdline[2] == 'i' && cmdline[3] == 't' && cmdline[4] == '=') {
                cmdline += 5;
                // Read the init binary path
                int i = 0;
                while (*cmdline != ' ' && *cmdline != '\0' && i < sizeof(init_binary_path) - 1) {
                    init_binary_path[i++] = *cmdline++;
                }
                init_binary_path[i] = '\0';
                continue;
            }

            panic("Unknown kernel command line argument here: %s", cmdline);
        }
    }
    mount_filesystem("/", "FAT", root_disk, root_partition, root_readonly);
    run_init(init_binary_path);
}

void kernel_main() {
    uintptr_t cr3;
    asm volatile(
        "mov %%cr3, %0"
        : "=r"(cr3)
    );
    init_paging(cr3, memmap_request.response, hhdm_request.response->offset);
    init_mman((size_t)&__size);
    gdt_init();
    idt_init();
    framebuffer = framebuffer_request.response->framebuffers[0];
    initialize_console();
    serial_init();
    ata_register();
    register_intree_filesystems();
    free_region(0x0, 0x100000000);
    register_rtl8139_driver();
    enumerate_pci();
    init_fpu();

    parse_kernel_cmdline();

    panic(0, "Init process exited unexpectedly!");
}
