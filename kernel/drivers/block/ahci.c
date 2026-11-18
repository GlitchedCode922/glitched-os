#include "drivers/block/ahci.h"
#include "drivers/block.h"
#include "fs/devfs.h"
#include "io/pci.h"
#include "memory/mman.h"
#include "memory/paging.h"
#include "panic.h"
#include "error.h"
#include <stdint.h>

typedef volatile struct {
    uint32_t clb;		// 0x00, command list base address, 1K-byte aligned
    uint32_t clbu;		// 0x04, command list base address upper 32 bits
    uint32_t fb;		// 0x08, FIS base address, 256-byte aligned
    uint32_t fbu;		// 0x0C, FIS base address upper 32 bits
    uint32_t is;		// 0x10, interrupt status
    uint32_t ie;		// 0x14, interrupt enable
    uint32_t cmd;		// 0x18, command and status
    uint32_t rsv0;		// 0x1C, Reserved
    uint32_t tfd;		// 0x20, task file data
    uint32_t sig;		// 0x24, signature
    uint32_t ssts;		// 0x28, SATA status (SCR0:SStatus)
    uint32_t sctl;		// 0x2C, SATA control (SCR2:SControl)
    uint32_t serr;		// 0x30, SATA error (SCR1:SError)
    uint32_t sact;		// 0x34, SATA active (SCR3:SActive)
    uint32_t ci;		// 0x38, command issue
    uint32_t sntf;		// 0x3C, SATA notification (SCR4:SNotification)
    uint32_t fbs;		// 0x40, FIS-based switch control
    uint32_t rsv1[11];	// 0x44 ~ 0x6F, Reserved
    uint32_t vendor[4];	// 0x70 ~ 0x7F, vendor specific
} __attribute__((packed)) hba_port_t;

typedef volatile struct {
    // 0x00 - 0x2B, Generic Host Control
    uint32_t cap;		// 0x00, Host capability
    uint32_t ghc;		// 0x04, Global host control
    uint32_t is;		// 0x08, Interrupt status
    uint32_t pi;		// 0x0C, Port implemented
    uint32_t vs;		// 0x10, Version
    uint32_t ccc_ctl;	// 0x14, Command completion coalescing control
    uint32_t ccc_pts;	// 0x18, Command completion coalescing ports
    uint32_t em_loc;		// 0x1C, Enclosure management location
    uint32_t em_ctl;		// 0x20, Enclosure management control
    uint32_t cap2;		// 0x24, Host capabilities extended
    uint32_t bohc;		// 0x28, BIOS/OS handoff control and status

    // 0x2C - 0x9F, Reserved
    uint8_t  rsv[0xA0-0x2C];

    // 0xA0 - 0xFF, Vendor specific registers
    uint8_t  vendor[0x100-0xA0];

    // 0x100 - 0x10FF, Port control registers
    hba_port_t ports[0];
} __attribute__((packed)) hba_mem_t;

typedef volatile struct {
    uint8_t cfl : 5;
    uint8_t a : 1;
    uint8_t w : 1;
    uint8_t p : 1;
    uint8_t r : 1;
    uint8_t b : 1;
    uint8_t c : 1;
    uint8_t rsv0 : 1;
    uint8_t pmp : 4;
    uint16_t prdtl;
    uint32_t prdbc;
    uint32_t ctba;
    uint32_t ctbau;
    uint32_t rsv1[4];
} __attribute__((packed)) hba_cmd_header_t;

typedef struct {
    uint8_t type;
    uint8_t pmport : 4;
    uint8_t rsv0 : 3;
    uint8_t c : 1;
    uint8_t command, featurel;
    uint8_t lba0, lba1, lba2, device;
    uint8_t lba3, lba4, lba5, featureh;
    uint16_t count;
    uint8_t icc, control;
    uint32_t rsv1;
} __attribute__((packed)) fis_h2d_t;

typedef struct {
    uint32_t dba, dbau, rsv0;
    uint32_t dbc : 22;
    uint32_t rsv1 : 9;
    uint32_t i : 1;
} __attribute__((packed)) hba_prdt_t;

typedef struct {
    uint8_t cfis[64];
    uint8_t acmd[16];
    uint8_t rsv[48];
    hba_prdt_t prdt[8];
} __attribute__((packed)) hba_cmd_table_t;

typedef struct {
    uint16_t identify[256];
    uint64_t sector_count;
    hba_mem_t* controller;
    hba_port_t* port;
    uint32_t sector_size;
} ahci_drive_t;

#define HBA_CMD_ST    0x0001
#define HBA_CMD_FRE   0x0010
#define HBA_CMD_FR    0x4000
#define HBA_CMD_CR    0x8000

static ahci_drive_t drives[1024] = {0};
static int drive_count = 0;
int ahci_driver_index;

int ahci_issue(hba_port_t* port, int slot, uint8_t cmd, uint64_t lba,
    uint32_t count, uint64_t buf_phys, uint32_t bytes, int write)
{
    uint64_t clb = ((uint64_t)port->clbu << 32) | (port->clb & ~0x3FF);
    hba_cmd_header_t* hdr = hhdm_base + clb;
    uint64_t ctba = ((uint64_t)hdr[slot].ctbau << 32) | (hdr[slot].ctba & ~0x7F);
    hba_cmd_table_t* table = hhdm_base + ctba;

    hdr[slot].cfl = 5;
    hdr[slot].w = write;
    hdr[slot].prdtl = 1;
    hdr[slot].prdbc = 0;

    memset(table, 0, sizeof(hba_cmd_table_t));
    table->prdt[0].dba = (uint32_t)buf_phys;
    table->prdt[0].dbau = buf_phys >> 32;
    table->prdt[0].dbc = bytes - 1;
    table->prdt[0].i = 1;

    fis_h2d_t* f = (fis_h2d_t*)table->cfis;
    f->type = 0x27;
    f->c = 1;
    f->command = cmd;
    f->device = 1 << 6;
    f->lba0 = lba;
    f->lba1 = lba >> 8;
    f->lba2 = lba >> 16;
    f->lba3 = lba >> 24;
    f->lba4 = lba >> 32;
    f->lba5 = lba >> 40;
    f->count = count;

    while (port->tfd & (0x80 | 0x08)) asm volatile("pause");
    port->ci |= 1 << slot;

    while (port->ci & (1 << slot)) {
        if (port->is & (1 << 30)) return -EIO;
    }
    if (port->is & (1 << 30)) return -EIO;
    port->is = 0xFFFFFFFF;
    return 0;
}

int ahci_identify(hba_port_t* port, uint16_t data[256]) {
    uintptr_t data_phys = get_physical_address((uintptr_t)data);
    return ahci_issue(port, 0, 0xEC, 0, 0, data_phys, 512, 0);
}

static uint64_t bounce_phys;
static uint8_t* bounce;

int ahci_read_sectors(int drive_number, uint64_t lba, uint8_t *buffer, uint64_t count) {
    if (drive_number < 0 || drive_number >= drive_count) return -EINVAL;
    if (count == 0) return 0;
    ahci_drive_t* drive = &drives[drive_number];
    if (lba + count > drive->sector_count) return -EINVAL;
    const int bounce_sectors = 4096 / drive->sector_size;
    while (count) {
        uint32_t n = count < bounce_sectors ? count : bounce_sectors;
        int res = ahci_issue(drive->port, 0, 0x25, lba, n, bounce_phys, n * drive->sector_size, 0);
        if (res < 0) return res;
        memcpy(buffer, bounce, n * drive->sector_size);
        buffer += n * drive->sector_size;
        lba += n;
        count -= n;
    }
    return 0;
}

int ahci_write_sectors(int drive_number, uint64_t lba, const uint8_t *buffer, uint64_t count) {
    if (drive_number < 0 || drive_number >= drive_count) return -EINVAL;
    if (count == 0) return 0;
    ahci_drive_t* drive = &drives[drive_number];
    if (lba + count > drive->sector_count) return -EINVAL;
    const int bounce_sectors = 4096 / drive->sector_size;
    while (count) {
        uint32_t n = count < bounce_sectors ? count : bounce_sectors;
        memcpy(bounce, buffer, n * drive->sector_size);
        int res = ahci_issue(drive->port, 0, 0x35, lba, n, bounce_phys, n * drive->sector_size, 1);
        if (res < 0) return res;
        buffer += n * drive->sector_size;
        lba += n;
        count -= n;
    }
    return 0;
}

int64_t ahci_get_drive_size(int drive_number) {
    if (drive_number < 0 || drive_number >= drive_count) return -EINVAL;
    return drives[drive_number].sector_count * drives[drive_number].sector_size;
}

int64_t ahci_get_sector_size(int drive_number) {
    if (drive_number < 0 || drive_number >= drive_count) return -EINVAL;
    return drives[drive_number].sector_size;
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

void ahci_init(pci_device_t device) {
    if (drive_count >= 1024) return;

    uint32_t command = pci_config_read32(device.bus, device.device, device.function, 0x04);
    pci_config_write32(device.bus, device.device, device.function, 0x04, command | 0x06);

    uint32_t bar5 = device.bar[5] & ~0xF;
    hba_mem_t* hba = find_available_va(0x1100);
    alloc_mmio_region((uintptr_t)hba, bar5, 0x1100, FLAGS_RW | FLAGS_PCD | FLAGS_PWT);

    hba->ghc |= (1 << 31);
    if (hba->cap2 & 0x1) {
        hba->bohc |= 0x2;
        while (hba->bohc & 0x1) asm volatile("pause");
    }

    int num_slots = ((hba->cap >> 8) & 0x1F) + 1;
    int s64a = hba->cap >> 31;
    int scnq = (hba->cap >> 30) & 0x1;
    int sss  = (hba->cap >> 27) & 0x1;
    int sclo = (hba->cap >> 24) & 0x1;

    for (int i = 0; i < 32; i++) {
        if (!(hba->pi & (1 << i))) continue;
        if (drive_count >= 1024) continue;
        hba_port_t* port = &hba->ports[i];
        uint32_t ssts = port->ssts;
        if ((ssts & 0xF) == 3 && ((ssts >> 8) & 0xF) == 1 && port->sig == 0x101) {
            // Found a SATA device
        } else {
            continue; // SATAPI and other device types are not implemented
        }

        port->cmd &= ~HBA_CMD_ST;
        port->cmd &= ~HBA_CMD_FRE;
        while (port->cmd & (HBA_CMD_FR | HBA_CMD_CR)) asm volatile("pause");

        uintptr_t dma_page = allocate_physical_page();
        if (dma_page >> 32 && !s64a) panic("AHCI Driver: Couldn't get a physical page under 4GiB");
        memset(hhdm_base + dma_page, 0, 4096);
        uintptr_t cmd_list = dma_page;
        uintptr_t received_fis = dma_page + 0x400;
        uintptr_t cmd_table = dma_page + 0x500;
        port->clb = (uint32_t)cmd_list;
        port->clbu = cmd_list >> 32;
        port->fb = (uint32_t)received_fis;
        port->fbu = received_fis >> 32;
        hba_cmd_header_t* cmd_hdr = hhdm_base + cmd_list;
        cmd_hdr[0].prdtl = 8; // Only one slot used
        cmd_hdr[0].ctba = (uint32_t)cmd_table;
        cmd_hdr[0].ctbau = cmd_table >> 32;

        port->serr = 0xFFFFFFFF;
        port->is = 0xFFFFFFFF;
        port->cmd |= HBA_CMD_FRE;
        port->cmd |= HBA_CMD_ST;

        drives[drive_count].controller = hba;
        drives[drive_count].port = port;

        uint16_t* id = drives[drive_count].identify;
        int res = ahci_identify(port, id);
        if (res < 0) continue;

        int lba48 = id[83] & (1 << 10);
        uint64_t sectors = lba48 ? *(uint64_t*)&id[100] : *(uint64_t*)&id[60];
        uint32_t logical = 512;
        if ((id[106] & 0xC000) == 0x4000 && (id[106] & (1 << 12))) {
            logical = ((uint32_t)id[117] | (uint32_t)id[118] << 16) * 2;
        }
        drives[drive_count].sector_count = sectors;
        drives[drive_count].sector_size = logical;
        drive_count++;

        char name[256] = "block/sd";
        itoa(drive_count - 1, name + 8);
        block_device_t dev = {
            .major_number = ahci_driver_index,
            .minor_number = drive_count - 1,
            .name = name,
            .is_partition = 0,
        };
        devfs_create_directory("block");
        register_block_device(&dev);
    }
}

void ahci_register() {
    block_driver_t ahci_driver_block = {
        .read_sectors = ahci_read_sectors,
        .write_sectors = ahci_write_sectors,
        .get_blockdev_size = ahci_get_drive_size,
        .get_sector_size = ahci_get_sector_size,
    };
    ahci_driver_index = register_block_driver(&ahci_driver_block);
    if (ahci_driver_index < 0) return;
    bounce_phys = allocate_physical_page();
    bounce = hhdm_base + bounce_phys;
    pci_driver_t ahci_driver = {
        .mode = 1,
        .class_code = 0x01,
        .subclass = 0x06,
        .prog_if = 0x01,
        .init = ahci_init,
    };
    register_pci_driver(ahci_driver);
}
