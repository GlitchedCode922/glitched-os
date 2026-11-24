#include "panic.h"
#include "console.h"
#include "log.h"
#include "util/string.h"
#include "uacpi/sleep.h"
#include "uacpi/acpi.h"
#include "uacpi/tables.h"
#include "limine_requests.h"
#include "drivers/ps2_keyboard.h"
#include "drivers/tty.h"
#include "error.h"
#include <stdarg.h>
#include <stdint.h>

panic_state_t panic_state = OPERATIONAL;
static const uint16_t invalid_idtptr[5] = {0};

void stack_trace_log(uint64_t rip, uint64_t rbp) {
    if (rbp == 0) asm volatile("mov %%rbp, %0" : "=r"(rbp));
    klogf(LOG_FATAL, "panic", "  #0: 0x%x\n", rip);
    for (int i = 1; ; i++) {
        uint64_t return_address = *((uint64_t*)(rbp + 8));
        if (return_address == 0) break;
        klogf(LOG_FATAL, "panic", "  #%d: 0x%x\n", i, return_address);
        rbp = *((uint64_t*)rbp);
        if (rbp == 0) break;
    }
}

void stack_trace(uint64_t rip, uint64_t rbp) {
    if (rbp == 0) asm volatile("mov %%rbp, %0" : "=r"(rbp));
    kprintf("  #0: 0x%x\n", rip);
    for (int i = 1; ; i++) {
        uint64_t return_address = *((uint64_t*)(rbp + 8));
        if (return_address == 0) break;
        kprintf("  #%d: 0x%x\n", i, return_address);
        rbp = *((uint64_t*)rbp);
        if (rbp == 0) break;
    }
}

__attribute__((noreturn)) void vpanic_int(uint64_t rip, uint64_t rbp, const char *fmt, va_list args) {
    switch (panic_state) {
        case OPERATIONAL:
            panic_state = PANIC;
            goto operational;
        case STACK_TRACE:
            panic_state = WAITING_INPUT;
            klogf(LOG_FATAL, "panic", "Cannot unwind stack further\n");
            goto stack_trace_failed;
        case PANIC:
        case WAITING_INPUT:
            // Double panic, halt CPU
            asm volatile ("cli");
            while (1) {
                asm volatile ("hlt");
            }
    }
operational:;

    // Print the panic message
    {
        int len = 0;
        while (fmt[len] != '\0') len++;
        char buffer[14 + len];
        memcpy(buffer, "Kernel Panic: ", 14);
        memcpy(buffer + 14, fmt, len + 1);
        kvlogf(LOG_FATAL, "panic", buffer, args);
        klogf(LOG_FATAL, "panic", "");
    }

    // Stack trace
    panic_state = STACK_TRACE;
    klogf(LOG_FATAL, "panic", "Stack trace:\n");
    stack_trace_log(rip, rbp);

stack_trace_failed:
    panic_state = WAITING_INPUT;
    if (console_tty_id >= 0) kprintf("\nPress any button to reboot...\n");

    char c = 0;
    asm volatile ("sti");
    input_disabled = 0;
    if (console_tty_id >= 0) ttys[console_tty_id]->termios.c_lflag = 0;
    if (console_tty_id >= 0) while (tty_read(console_tty_id, 0, (uint8_t*)&c, 1) == -EAGAIN);
    if (rsdp_request.response->address) {
        struct acpi_fadt* fadt;
        uacpi_table_fadt(&fadt);
        if (fadt->flags & ACPI_RESET_REG_SUP) {
            uacpi_status res = uacpi_reboot();
            if (uacpi_unlikely_error(res)) {
                klogf(LOG_FATAL, "acpi", "Reboot failed: %s\n", uacpi_status_to_string(res));
            }
        }
    }
    asm volatile (
        "lidt %0\n"
        "int $0x00\n"
        :: "m"(invalid_idtptr)
    );
    while (1) asm volatile ("hlt");
}

__attribute__((noreturn)) void panic_int(uint64_t rip, uint64_t rbp, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    vpanic_int(rip, rbp, fmt, args);

    va_end(args);
}

__attribute__((noreturn)) void panic(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    uint64_t rbp;
    uint64_t rip;
    asm volatile("mov %%rbp, %0" : "=r"(rbp));
    asm volatile("lea 0(%%rip), %0" : "=r"(rip));
    vpanic_int(rip, rbp, fmt, args);

    va_end(args);
}
