#include "power.h"
#include "uacpi/sleep.h"
#include "uacpi/status.h"
#include "uacpi/acpi.h"
#include "uacpi/tables.h"
#include "panic.h"

static const uint16_t invalid_idtptr[5] = {0};

void __attribute__((noreturn)) shutdown() {
    uacpi_status res = uacpi_enter_sleep_state_simple(UACPI_SLEEP_STATE_S5);
    if (uacpi_unlikely_error(res)) {
        panic("Shutdown failed: ACPI error: %s", uacpi_status_to_string(res));
    }
    panic("Shutdown returned unexpectedly");
}

void __attribute__((noreturn)) reboot() {
    struct acpi_fadt* fadt;
    uacpi_table_fadt(&fadt);
    if (fadt->flags & ACPI_RESET_REG_SUP) {
        uacpi_status res = uacpi_reboot();
        if (uacpi_unlikely_error(res)) {
            panic("Reboot failed: ACPI error: %s", uacpi_status_to_string(res));
        }
    } else {
        while(1);
        asm volatile (
            "lidt %0\n"
            "int $0x00\n"
            :: "m"(invalid_idtptr)
        );
    }
    panic("Reboot returned unexpectedly");
}
