#include "nvme.h"
#include "drivers/block.h"
#include "fs/devfs.h"
#include "io/pci.h"
#include "memory/paging.h"
#include "memory/mman.h"
#include "error.h"
#include "spin.h"
#include "log.h"
#include <stdint.h>

typedef volatile struct {
    uint64_t cap;      // 0x00 Controller Capabilities
    uint32_t vs;       // 0x08 Version
    uint32_t intms;    // 0x0C Interrupt Mask Set
    uint32_t intmc;    // 0x10 Interrupt Mask Clear
    uint32_t cc;       // 0x14 Controller Configuration
    uint32_t reserved; // 0x18
    uint32_t csts;     // 0x1C Controller Status
    uint32_t nssr;     // 0x20 NVM Subsystem Reset
    uint32_t aqa;      // 0x24 Admin Queue Attributes
    uint64_t asq;      // 0x28 Admin SQ base (physical)
    uint64_t acq;      // 0x30 Admin CQ base (physical)
    // 0x38..0xFFF: CMB/boot partition regs, reserved
} __attribute__((packed)) nvme_controller_regs_t;

typedef struct {
    uint32_t cdw0;
    uint32_t nsid;
    uint64_t reserved;
    uint64_t mptr;
    uint64_t prp1;
    uint64_t prp2;
    uint32_t cdw10;
    uint32_t cdw11;
    uint32_t cdw12;
    uint32_t cdw13;
    uint32_t cdw14;
    uint32_t cdw15;
} __attribute__((packed)) nvme_sq_entry_t;

typedef struct {
    uint32_t result;
    uint32_t reserved;
    uint16_t sq_head;
    uint16_t sq_id;
    uint16_t cid;
    uint16_t status;
} __attribute__((packed)) nvme_cq_entry_t;

typedef struct {
    nvme_cq_entry_t* cq;
    nvme_sq_entry_t* sq;
    uint64_t sq_phys, cq_phys;
    uint16_t sq_tail, cq_head;
    uint16_t phase;
    uint16_t cid;
    volatile uint32_t* sq_doorbell;
    volatile uint32_t* cq_doorbell;
} nvme_queue_t;

#define NVME_QUEUE_LENGTH 64

#define CC_EN 1u
#define CC_CSS(x) ((x) << 4)
#define CC_MPS(x) ((x) << 7)
#define CC_AMS(x) ((x) << 11)
#define CC_IOSQES(x) ((x) << 16)
#define CC_IOCQES(x) ((x) << 20)

typedef struct {
    nvme_controller_regs_t* regs;
    nvme_queue_t admin;
    nvme_queue_t io;
} nvme_controller_t;

typedef struct {
    nvme_controller_t* controller;
    uint32_t nsid;
    uint64_t block_count;
    uint32_t block_size;
} nvme_namespace_t;

static nvme_controller_t controllers[512];
static int controller_count = 0;
static nvme_namespace_t namespaces[1024];
static int namespace_count = 0;
int nvme_block_driver_index;

static inline volatile uint32_t* nvme_sq_doorbell(nvme_controller_regs_t* regs, int qid) {
    int dstrd = ((regs->cap) >> 32) & 0xF;
    return (volatile uint32_t*)((uint8_t*)regs + 0x1000 + (2 * qid) * (4 << dstrd));
}

static inline volatile uint32_t* nvme_cq_doorbell(nvme_controller_regs_t* regs, int qid) {
    int dstrd = ((regs->cap) >> 32) & 0xF;
    return (volatile uint32_t*)((uint8_t*)regs + 0x1000 + (2 * qid + 1) * (4 << dstrd));
}

int nvme_wait_ready(nvme_controller_regs_t* regs, int wanted) {
    spin(1, (regs->cap >> 24 & 0xFF) * 500,
        uint32_t csts = regs->csts;
        if (csts & 2) return -EIO;
        if ((csts & 1) == wanted) return 0;
    );
    return -EIO;
}

int nvme_submit(nvme_queue_t* queue, nvme_sq_entry_t cmd) {
    cmd.cdw0 = (cmd.cdw0 & 0xFFFF) | ((uint32_t)queue->cid++ << 16);
    queue->sq[queue->sq_tail] = cmd;
    queue->sq_tail = (queue->sq_tail + 1) % NVME_QUEUE_LENGTH;
    __sync_synchronize();
    *queue->sq_doorbell = queue->sq_tail;

    volatile nvme_cq_entry_t* cqe = &queue->cq[queue->cq_head];
    if (spin((cqe->status & 1) != queue->phase, 20000,) < 0) return -EIO;

    uint16_t status = cqe->status >> 1;
    if (++queue->cq_head == NVME_QUEUE_LENGTH) {
        queue->cq_head = 0;
        queue->phase = !queue->phase;
    }
    __sync_synchronize();
    *queue->cq_doorbell = queue->cq_head;
    return status == 0 ? 0 : -EIO;
}

static uint64_t bounce_phys;
static void* bounce;

int nvme_identify(nvme_controller_t* controller, uint32_t nsid, uint32_t cns, void* buffer) {
    uint64_t buf_phys = get_physical_address((uintptr_t)buffer);
    nvme_sq_entry_t cmd = {0};
    cmd.cdw0 = 0x06;
    cmd.nsid = nsid;
    cmd.prp1 = bounce_phys;
    cmd.cdw10 = cns;
    int res = nvme_submit(&controller->admin, cmd);
    if (res < 0) return res;
    memcpy(buffer, bounce, 4096);
    return 0;
}

int nvme_identify_ns(nvme_namespace_t* ns) {
    uint8_t data[4096];
    int res = nvme_identify(ns->controller, ns->nsid, 0, data);
    if (res < 0) return res;
    uint8_t format_index = data[26] & 0xF;
    uint32_t format = *(uint32_t*)(data + 128 + 4 * format_index);
    ns->block_count = *(uint64_t*)data;
    ns->block_size = 1u << ((format >> 16) & 0xFF);
    if (format & 0xFFFF) return -ENOSYS;
    return 0;
}

int nvme_submit_rw(nvme_namespace_t* ns, int write, uint64_t lba, uint32_t len, uint64_t buf_phys) {
    nvme_sq_entry_t cmd = {0};
    cmd.cdw0 = write ? 0x01 : 0x02;
    cmd.nsid = ns->nsid;
    cmd.prp1 = buf_phys;
    cmd.cdw10 = (uint32_t)lba;
    cmd.cdw11 = (uint32_t)(lba >> 32);
    cmd.cdw12 = len - 1;
    return nvme_submit(&ns->controller->io, cmd);
}

int nvme_read_sectors(int drive_number, uint64_t lba, uint8_t *buffer, uint64_t count) {
    if (drive_number < 0 || drive_number >= namespace_count) return -ENODEV;
    if (count == 0) return 0;
    nvme_namespace_t* ns = namespaces + drive_number;

    if (!ns->block_size || ns->block_size > 4096) return -EIO;
    if (lba > ns->block_count || count > ns->block_count - lba) return -EINVAL;
    const uint32_t bounce_sectors = 4096 / ns->block_size;
    if (!bounce_sectors) return -EIO;
    while (count) {
        uint32_t n = count < bounce_sectors ? count : bounce_sectors;
        int res = nvme_submit_rw(ns, 0, lba, count, bounce_phys);
        if (res < 0) return res;
        memcpy(buffer, bounce, n * ns->block_size);
        buffer += n * ns->block_size;
        lba += n;
        count -= n;
    }
    return 0;
}

int nvme_write_sectors(int drive_number, uint64_t lba, const uint8_t *buffer, uint64_t count) {
    if (drive_number < 0 || drive_number >= namespace_count) return -ENODEV;
    if (count == 0) return 0;
    nvme_namespace_t* ns = namespaces + drive_number;

    if (!ns->block_size || ns->block_size > 4096) return -EIO;
    if (lba > ns->block_count || count > ns->block_count - lba) return -EINVAL;
    const uint32_t bounce_sectors = 4096 / ns->block_size;
    if (!bounce_sectors) return -EIO;
    while (count) {
        uint32_t n = count < bounce_sectors ? count : bounce_sectors;
        memcpy(bounce, buffer, n * ns->block_size);
        int res = nvme_submit_rw(ns, 1, lba, count, bounce_phys);
        if (res < 0) return res;
        buffer += n * ns->block_size;
        lba += n;
        count -= n;
    }
    return 0;
}

int64_t nvme_get_drive_size(int drive_number) {
    if (drive_number < 0 || drive_number >= namespace_count) return -ENODEV;
    nvme_namespace_t* ns = namespaces + drive_number;
    return ns->block_count * ns->block_size;
}

int64_t nvme_get_block_size(int drive_number) {
    if (drive_number < 0 || drive_number >= namespace_count) return -ENODEV;
    nvme_namespace_t* ns = namespaces + drive_number;
    return ns->block_size;
}

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

void nvme_init(pci_device_t device) {
    if (controller_count >= 512) return;
    if (namespace_count >= 1024) return;
    klogf(LOG_INFO, "nvme", "Found a NVMe controller (index %d)\n", controller_count);

    // Read PCI BARs and map the controller registers
    uint16_t command = pci_config_read16(device.bus, device.device, device.function, 0x04);
    pci_config_write16(device.bus, device.device, device.function, 0x04, command & ~(1 << 1));

    pci_config_write32(device.bus, device.device, device.function, 0x10, 0xFFFFFFFF);
    pci_config_write32(device.bus, device.device, device.function, 0x14, 0xFFFFFFFF);

    uint32_t bar0 = device.bar[0];
    uint32_t bar1 = device.bar[1];
    uint32_t mask0 = pci_config_read32(device.bus, device.device, device.function, 0x10);
    uint32_t mask1 = pci_config_read32(device.bus, device.device, device.function, 0x14);

    pci_config_write32(device.bus, device.device, device.function, 0x10, bar0);
    pci_config_write32(device.bus, device.device, device.function, 0x14, bar1);

    pci_config_write16(device.bus, device.device, device.function, 0x04, command | 0x06);

    uint64_t paddr = ((uint64_t)bar1 << 32) | (bar0 & ~0xF);
    uint64_t mask = ((uint64_t)mask1 << 32) | (mask0 & ~0xF);
    uint64_t size = ~mask + 1;

    nvme_controller_regs_t* regs = find_available_va(size);
    alloc_mmio_region((uintptr_t)regs, paddr, size, FLAGS_RW | FLAGS_PCD | FLAGS_PWT);
    nvme_controller_t* controller = controllers + controller_count;
    controller->regs = regs;
    regs->cc &= ~CC_EN;
    if (nvme_wait_ready(regs, 0) < 0) return;

    // Create admin queue
    nvme_queue_t* admin = &controller->admin;
    admin->sq_phys = allocate_physical_page();
    admin->cq_phys = allocate_physical_page();
    admin->sq = hhdm_base + admin->sq_phys;
    admin->cq = hhdm_base + admin->cq_phys;
    admin->sq_tail = 0;
    admin->cq_head = 0;
    admin->cid = 0;
    admin->phase = 1;
    admin->sq_doorbell = nvme_sq_doorbell(regs, 0);
    admin->cq_doorbell = nvme_cq_doorbell(regs, 0);

    regs->aqa = ((NVME_QUEUE_LENGTH - 1) << 16) | (NVME_QUEUE_LENGTH - 1);
    regs->asq = admin->sq_phys;
    regs->acq = admin->cq_phys;

    regs->cc = CC_EN | CC_CSS(0) | CC_MPS(0) | CC_AMS(0) | CC_IOSQES(6) | CC_IOCQES(4);
    nvme_wait_ready(regs, 1);
    controller_count++;

    // Find namespaces
    uint32_t active_namespaces[1024];
    int original_ns_count = namespace_count;
    int namespaces_in_controller = 0;
    int res = nvme_identify(controller, 0, 0x02, active_namespaces);
    if (res < 0) {
        // Try probing NSID 1, active namespace list unsupported
        nvme_namespace_t* ns = namespaces + namespace_count;
        ns->controller = controller;
        ns->nsid = 1;
        res = nvme_identify_ns(ns);
        if (res >= 0) {
            namespaces_in_controller++;
            namespace_count++;
        }
    } else {
        for (int i = 0; i < 1024 && active_namespaces[i]; i++) {
            if (namespace_count >= 1024) return;
            nvme_namespace_t* ns = namespaces + namespace_count;
            ns->controller = controller;
            ns->nsid = active_namespaces[i];
            res = nvme_identify_ns(ns);
            if (res >= 0) {
                namespaces_in_controller++;
                namespace_count++;
            }
        }
    }

    if (namespaces_in_controller == 0) return;
    klogf(LOG_INFO, "nvme", "Found %d NVMe namespaces on controller %d\n", namespaces_in_controller, controller_count - 1);

    // Create IO queue
    nvme_queue_t* io = &controller->io;
    const int io_qid = 1;
    io->sq_phys = allocate_physical_page();
    io->cq_phys = allocate_physical_page();
    io->sq = hhdm_base + io->sq_phys;
    io->cq = hhdm_base + io->cq_phys;
    io->sq_tail = 0;
    io->cq_head = 0;
    io->cid = 0;
    io->phase = 1;
    io->sq_doorbell = nvme_sq_doorbell(regs, io_qid);
    io->cq_doorbell = nvme_cq_doorbell(regs, io_qid);

    nvme_sq_entry_t cmd = {0};
    cmd.cdw0 = 0x05;
    cmd.prp1 = io->cq_phys;
    cmd.cdw10 = ((NVME_QUEUE_LENGTH - 1) << 16) | io_qid;
    cmd.cdw11 = 1;
    res = nvme_submit(admin, cmd);
    if (res < 0) {
        klogf(LOG_ERROR, "nvme", "Failed to create IO completion queue for controller %d\n", controller_count - 1);
    }
    klogf(LOG_INFO, "nvme", "Created IO completion queue for controller %d\n", controller_count - 1);

    memset(&cmd, 0, sizeof(cmd));
    cmd.cdw0 = 0x01;
    cmd.prp1 = io->sq_phys;
    cmd.cdw10 = ((NVME_QUEUE_LENGTH - 1) << 16) | io_qid;
    cmd.cdw11 = (io_qid << 16) | 1;
    res = nvme_submit(admin, cmd);
    if (res < 0) {
        klogf(LOG_ERROR, "nvme", "Failed to create IO submission queue for controller %d\n", controller_count - 1);
    }
    klogf(LOG_INFO, "nvme", "Created IO submission queue for controller %d\n", controller_count - 1);

    // Register namespaces as block devices
    for (int i = original_ns_count; i < namespace_count; i++) {
        char name[256] = "block/nvme-hd";
        itoa(i, name + 13);

        block_device_t dev = {
            .major_number = nvme_block_driver_index,
            .minor_number = i,
            .name = name,
            .is_partition = 0,
        };

        devfs_create_directory("block");
        register_block_device(&dev);
    }

    return;
}

void nvme_register() {
    bounce_phys = allocate_physical_page();
    bounce = hhdm_base + bounce_phys;
    block_driver_t nvme_block = {
        .read_sectors = nvme_read_sectors,
        .write_sectors = nvme_write_sectors,
        .get_blockdev_size = nvme_get_drive_size,
        .get_sector_size = nvme_get_block_size,
    };
    nvme_block_driver_index = register_block_driver(&nvme_block);
    if (nvme_block_driver_index < 0) return;
    pci_driver_t nvme_pci = {
        .init = nvme_init,
        .mode = 1,
        .class_code = 0x01,
        .subclass = 0x08,
        .prog_if = 0x02,
    };
    register_pci_driver(nvme_pci);
}
