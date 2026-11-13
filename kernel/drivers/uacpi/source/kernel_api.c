#include "uacpi/kernel_api.h"
#include "io/8259pic.h"
#include "io/pci.h"
#include "io/ports.h"
#include "memory/paging.h"
#include "memory/mman.h"
#include "uacpi/log.h"
#include "console.h"
#include "uacpi/status.h"
#include "uacpi/types.h"
#include "usermode/scheduler.h"
#include "limine.h"
#include <stdint.h>
#include <stdatomic.h>

void *uacpi_kernel_map(uacpi_phys_addr addr, uacpi_size len) {
    uintptr_t page_offset = addr - (addr & PAGE_MASK);
    uintptr_t page_aligned = addr - page_offset;
    void* va = find_available_va(page_offset + len);
    alloc_mmio_region((uintptr_t)va, page_aligned, page_offset + len, FLAGS_RW | FLAGS_PCD | FLAGS_PWT);
    return va + page_offset;
}

void uacpi_kernel_unmap(void *addr, uacpi_size len) {
    kfree((void*)((uintptr_t)addr & PAGE_MASK));
}

void uacpi_kernel_log(uacpi_log_level ll, const uacpi_char* string) {
    if (ll > UACPI_LOG_ERROR) return;
    kprintf("%s", string);
}

uacpi_status uacpi_kernel_pci_device_open(uacpi_pci_address address, uacpi_handle *out_handle) {
    if (address.segment != 0) return UACPI_STATUS_NOT_FOUND;
    for (size_t i = 0; i < MAX_PCI_DEVICES; i++) {
        if (pci_devices[i].bus != address.bus) continue;
        if (pci_devices[i].device != address.device) continue;
        if (pci_devices[i].function != address.function) continue;
        *out_handle = (void*)i;
        return UACPI_STATUS_OK;
    }
    return UACPI_STATUS_NOT_FOUND;
}

void uacpi_kernel_pci_device_close(uacpi_handle handle) {
    // no-op
    (void)handle;
}

uacpi_status uacpi_kernel_pci_read8(uacpi_handle handle, uacpi_size offset, uacpi_u8 *value) {
    size_t dev_id = (size_t)handle;
    uint8_t bus = pci_devices[dev_id].bus;
    uint8_t device = pci_devices[dev_id].bus;
    uint8_t function = pci_devices[dev_id].bus;
    *value = pci_config_read8(bus, device, function, offset);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_read16(uacpi_handle handle, uacpi_size offset, uacpi_u16 *value) {
    size_t dev_id = (size_t)handle;
    uint8_t bus = pci_devices[dev_id].bus;
    uint8_t device = pci_devices[dev_id].bus;
    uint8_t function = pci_devices[dev_id].bus;
    *value = pci_config_read16(bus, device, function, offset);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_read32(uacpi_handle handle, uacpi_size offset, uacpi_u32 *value) {
    size_t dev_id = (size_t)handle;
    uint8_t bus = pci_devices[dev_id].bus;
    uint8_t device = pci_devices[dev_id].bus;
    uint8_t function = pci_devices[dev_id].bus;
    *value = pci_config_read32(bus, device, function, offset);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_write8(uacpi_handle handle, uacpi_size offset, uacpi_u8 value) {
    size_t dev_id = (size_t)handle;
    uint8_t bus = pci_devices[dev_id].bus;
    uint8_t device = pci_devices[dev_id].bus;
    uint8_t function = pci_devices[dev_id].bus;
    pci_config_write8(bus, device, function, offset, value);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_write16(uacpi_handle handle, uacpi_size offset, uacpi_u16 value) {
    size_t dev_id = (size_t)handle;
    uint8_t bus = pci_devices[dev_id].bus;
    uint8_t device = pci_devices[dev_id].bus;
    uint8_t function = pci_devices[dev_id].bus;
    pci_config_write16(bus, device, function, offset, value);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_write32(uacpi_handle handle, uacpi_size offset, uacpi_u32 value) {
    size_t dev_id = (size_t)handle;
    uint8_t bus = pci_devices[dev_id].bus;
    uint8_t device = pci_devices[dev_id].bus;
    uint8_t function = pci_devices[dev_id].bus;
    pci_config_write32(bus, device, function, offset, value);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_map(uacpi_io_addr base, uacpi_size len, uacpi_handle *out_handle) {
    *out_handle = (void*)base;
    return UACPI_STATUS_OK;
}

void uacpi_kernel_io_unmap(uacpi_handle handle) {
    // no-op
    (void)handle;
}

uacpi_status uacpi_kernel_io_read8(uacpi_handle handle, uacpi_size offset, uacpi_u8 *out_value) {
    *out_value = inb((uacpi_io_addr)handle);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_read16(uacpi_handle handle, uacpi_size offset, uacpi_u16 *out_value) {
    *out_value = inw((uacpi_io_addr)handle);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_read32(uacpi_handle handle, uacpi_size offset, uacpi_u32 *out_value) {
    *out_value = inl((uacpi_io_addr)handle);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_write8(uacpi_handle handle, uacpi_size offset, uacpi_u8 value) {
    outb((uacpi_io_addr)handle, value);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_write16(uacpi_handle handle, uacpi_size offset, uacpi_u16 value) {
    outw((uacpi_io_addr)handle, value);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_write32(uacpi_handle handle, uacpi_size offset, uacpi_u32 value) {
    outl((uacpi_io_addr)handle, value);
    return UACPI_STATUS_OK;
}

void *uacpi_kernel_alloc(uacpi_size size) {return kmalloc(size);}
void *uacpi_kernel_alloc_zeroed(uacpi_size size) {return kmalloc(size);}
void uacpi_kernel_free(void *mem) {kfree(mem);}

uacpi_u64 uacpi_kernel_get_nanoseconds_since_boot(void) {
    return vvar_vaddr->uptime_ms * 1000000;
}

void uacpi_kernel_stall(uacpi_u8 usec) {
    uint32_t ms = (usec + 1000 - 1) / 1000;
    uint64_t start = vvar_vaddr->uptime_ms;
    while (vvar_vaddr->uptime_ms < start + ms) asm volatile("pause");
}

void uacpi_kernel_sleep(uacpi_u64 ms) {
    uint64_t start = vvar_vaddr->uptime_ms;
    while (vvar_vaddr->uptime_ms < start + ms) asm volatile("pause");
}

uacpi_handle uacpi_kernel_create_semaphore(uacpi_u32 initial_units) {
    uint64_t* semaphore = kmalloc(8);
    *semaphore = initial_units;
    return semaphore;
}

void uacpi_kernel_free_semaphore(uacpi_handle semaphore) {
    kfree(semaphore);
}

uacpi_thread_id uacpi_kernel_get_thread_id(void) {
    return (uacpi_thread_id)(uintptr_t)(current_task->pid + 1);
}

uacpi_interrupt_state uacpi_kernel_disable_interrupts(void) {
    asm volatile("cli");
    return 0;
}

void uacpi_kernel_restore_interrupts(uacpi_interrupt_state state) {
    asm volatile("sti");
}

uacpi_status uacpi_kernel_wait_for_semaphore(uacpi_handle handle, uacpi_u16 timeout) {
    uint64_t* semaphore = handle;
    if (*semaphore) {
        (*semaphore)--;
        return UACPI_STATUS_OK;
    }

    if (timeout == 0x0000) {
        return UACPI_STATUS_TIMEOUT;
    } else if (timeout == 0xFFFF) {
        while (*semaphore == 0) asm volatile("pause");
        (*semaphore)--;
        return UACPI_STATUS_OK;
    } else {
        uint64_t start = vvar_vaddr->uptime_ms;
        while (vvar_vaddr->uptime_ms < start + timeout) asm volatile("pause");
        (*semaphore)--;
        return UACPI_STATUS_OK;
    }
}

void uacpi_kernel_signal_semaphore(uacpi_handle semaphore) {
    (*(uint64_t*)semaphore)++;
}

uacpi_status uacpi_kernel_handle_firmware_request(uacpi_firmware_request* req) {
    if (req->type == UACPI_FIRMWARE_REQUEST_TYPE_FATAL) {
        kprintf("acpi: fatal error: type=%x code=%x arg=%x\n", req->fatal.type, req->fatal.code, req->fatal.arg);
    }
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_install_interrupt_handler(
    uacpi_u32 irq, uacpi_interrupt_handler handler, uacpi_handle ctx,
    uacpi_handle *out_irq_handle
) {
    if (irq > 15) return UACPI_STATUS_INVALID_ARGUMENT;
    irq_handlers[irq].arg = ctx;
    irq_handlers[irq].handler = handler;
    *out_irq_handle = (uacpi_handle)(uintptr_t)irq;
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_uninstall_interrupt_handler(uacpi_interrupt_handler handler, uacpi_handle irq_handle) {
    irq_handlers[(uintptr_t)irq_handle].arg = NULL;
    irq_handlers[(uintptr_t)irq_handle].handler = NULL;
    return UACPI_STATUS_OK;
}

uacpi_handle uacpi_kernel_create_spinlock(void) {
    atomic_flag* lock = kmalloc(sizeof(atomic_flag));
    atomic_flag_clear(lock);
    return lock;
}

void uacpi_kernel_free_spinlock(uacpi_handle lock) {
    kfree(lock);
}

uacpi_cpu_flags uacpi_kernel_lock_spinlock(uacpi_handle lock) {
    asm volatile("cli");
    while (atomic_flag_test_and_set((atomic_flag*)lock)) asm volatile("pause");
    return (uacpi_cpu_flags)0;
}

void uacpi_kernel_unlock_spinlock(uacpi_handle lock, uacpi_cpu_flags _) {
    atomic_flag_clear((atomic_flag*)lock);
    asm volatile("sti");
}

uacpi_handle uacpi_kernel_create_work_item(void) {
    return (void*)1;
}

void uacpi_kernel_free_work_item(uacpi_handle _) {
    // no-op
    (void)_;
}

void uacpi_kernel_schedule_work(
    uacpi_work_type type, uacpi_handle work_item, uacpi_work_handler handler,
    uacpi_handle ctx
) {
    uint64_t rflags;
    asm volatile("pushfq\n"
        "popq %0"
        : "=r"(rflags)
        :: "memory"
    );
    asm volatile("sti"); // Prevent deadlocks
    handler(ctx);
    if (!(rflags & (1 << 9))) asm volatile("cli");
}

uacpi_status uacpi_kernel_wait_for_work_completion(void) {
    return UACPI_STATUS_OK;
}

__attribute__((used, section(".limine_requests")))
static volatile struct limine_rsdp_request rsdp_request = {
    .id = LIMINE_RSDP_REQUEST,
    .revision = 0
};

uacpi_status uacpi_kernel_get_rsdp(uacpi_phys_addr *out_rsdp_address) {
    *out_rsdp_address = (uintptr_t)rsdp_request.response->address;
    return UACPI_STATUS_OK;
}
