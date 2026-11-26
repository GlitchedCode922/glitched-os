#include "drivers/block/ahci.h"
#include "drivers/block.h"
#include "fs/devfs.h"
#include "io/pci.h"
#include "memory/mman.h"
#include "memory/paging.h"
#include "panic.h"
#include "error.h"
#include "spin.h"
#include "util/log.h"
#include "uapi/ioctl.h"
#include "util/string.h"
#include <stddef.h>
#include <stdint.h>

typedef volatile struct hba_port {
    uint32_t clb;       // 0x00, command list base address, 1K-byte aligned
    uint32_t clbu;      // 0x04, command list base address upper 32 bits
    uint32_t fb;        // 0x08, FIS base address, 256-byte aligned
    uint32_t fbu;       // 0x0C, FIS base address upper 32 bits
    uint32_t is;        // 0x10, interrupt status
    uint32_t ie;        // 0x14, interrupt enable
    uint32_t cmd;       // 0x18, command and status
    uint32_t rsv0;      // 0x1C, Reserved
    uint32_t tfd;       // 0x20, task file data
    uint32_t sig;       // 0x24, signature
    uint32_t ssts;      // 0x28, SATA status (SCR0:SStatus)
    uint32_t sctl;      // 0x2C, SATA control (SCR2:SControl)
    uint32_t serr;      // 0x30, SATA error (SCR1:SError)
    uint32_t sact;      // 0x34, SATA active (SCR3:SActive)
    uint32_t ci;        // 0x38, command issue
    uint32_t sntf;      // 0x3C, SATA notification (SCR4:SNotification)
    uint32_t fbs;       // 0x40, FIS-based switch control
    uint32_t rsv1[11];  // 0x44 ~ 0x6F, Reserved
    uint32_t vendor[4]; // 0x70 ~ 0x7F, vendor specific
} __attribute__((packed)) hba_port_t;

typedef volatile struct hba_mem {
    // 0x00 - 0x2B, Generic Host Control
    uint32_t cap;       // 0x00, Host capability
    uint32_t ghc;       // 0x04, Global host control
    uint32_t is;        // 0x08, Interrupt status
    uint32_t pi;        // 0x0C, Port implemented
    uint32_t vs;        // 0x10, Version
    uint32_t ccc_ctl;   // 0x14, Command completion coalescing control
    uint32_t ccc_pts;   // 0x18, Command completion coalescing ports
    uint32_t em_loc;    // 0x1C, Enclosure management location
    uint32_t em_ctl;    // 0x20, Enclosure management control
    uint32_t cap2;      // 0x24, Host capabilities extended
    uint32_t bohc;      // 0x28, BIOS/OS handoff control and status

    // 0x2C - 0x9F, Reserved
    uint8_t rsv[0xA0-0x2C];

    // 0xA0 - 0xFF, Vendor specific registers
    uint8_t vendor[0x100-0xA0];

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
    uint64_t sector_count;
    hba_mem_t* controller;
    hba_port_t* port;
    uint32_t sector_size;
    int is_atapi;
    int sclo;
} ahci_drive_t;

#define HBA_CMD_ST    0x0001
#define HBA_CMD_FRE   0x0010
#define HBA_CMD_FR    0x4000
#define HBA_CMD_CR    0x8000

static ahci_drive_t drives[1024] = {0};
static int drive_count = 0;
static int sata_count = 0;
static int atapi_count = 0;
int ahci_driver_index;

#define HBA_CMD_ATAPI (1u << 24)

static int ahci_port_recover(hba_port_t* port, int sclo) {
    port->cmd &= ~HBA_CMD_ST;
    if (spin(port->cmd & HBA_CMD_CR, 500,) < 0) return -EIO;
    port->serr = 0xFFFFFFFF;
    port->is = 0xFFFFFFFF;

    if (port->tfd & (0x80 | 0x08)) {
        if (sclo) {
            port->cmd |= (1 << 3);                       // CLO
            spin(port->cmd & (1 << 3), 500,);            // HBA clears it when done
        }
        if (port->tfd & (0x80 | 0x08)) {                 // still stuck: COMRESET
            port->sctl = (port->sctl & ~0xFu) | 1;       // DET = 1
            spin(1, 2,);
            port->sctl &= ~0xFu;
            spin((port->ssts & 0xF) != 3, 1000,);
            port->serr = 0xFFFFFFFF;
            spin(port->tfd & (0x80 | 0x08), 1000,);
        }
    }
    port->cmd |= HBA_CMD_ST;
    return 0;
}

int ahci_issue(hba_port_t* port, int slot, fis_h2d_t* fis, uint64_t buf_phys,
    uint32_t bytes, int write, uint8_t* atapi_packet, int has_sclo)
{
    uint64_t clb = ((uint64_t)port->clbu << 32) | (port->clb & ~0x3FF);
    hba_cmd_header_t* hdr = hhdm_base + clb;
    uint64_t ctba = ((uint64_t)hdr[slot].ctbau << 32) | (hdr[slot].ctba & ~0x7F);
    hba_cmd_table_t* table = hhdm_base + ctba;

    hdr[slot].cfl = 5;
    hdr[slot].w = write;
    hdr[slot].prdtl = buf_phys != 0 ? 1 : 0;
    hdr[slot].prdbc = 0;
    hdr[slot].a = atapi_packet != NULL;

    memset(table, 0, sizeof(hba_cmd_table_t));
    table->prdt[0].dba = (uint32_t)buf_phys;
    table->prdt[0].dbau = buf_phys >> 32;
    table->prdt[0].dbc = bytes - 1;
    table->prdt[0].i = 1;

    memcpy(table->cfis, fis, sizeof(fis_h2d_t));
    if (atapi_packet) memcpy(table->acmd, atapi_packet, 16);

    if (spin(port->tfd & (0x80 | 0x08), 5000,) < 0) {
        ahci_port_recover(port, has_sclo);
        return -EIO;
    }
    port->ci |= 1 << slot;

    int res = spin(port->ci & (1 << slot),
        10000,
        if (port->is & (1 << 30)) {
            ahci_port_recover(port, has_sclo);
            return -EIO;
        }
    );
    if (res < 0 || port->is & (1 << 30)) {
        port->serr = 0xFFFFFFFF;
        port->is = 0xFFFFFFFF;
        return -EIO;
    }
    port->serr = 0xFFFFFFFF;
    port->is = 0xFFFFFFFF;
    return 0;
}

int ahci_ata_issue(hba_port_t* port, int slot, uint8_t cmd, uint64_t lba,
    uint32_t count, uint64_t buf_phys, uint32_t bytes, int write, int sclo)
{
    fis_h2d_t fis = {
        .type = 0x27,
        .c = 1,
        .command = cmd,
        .device = 1 << 6,
        .lba0 = lba,
        .lba1 = lba >> 8,
        .lba2 = lba >> 16,
        .lba3 = lba >> 24,
        .lba4 = lba >> 32,
        .lba5 = lba >> 40,
        .count = count,
    };
    return ahci_issue(port, slot, &fis, buf_phys, bytes, write, NULL, sclo);
}

static uint64_t bounce_phys;
static uint8_t* bounce;

int ahci_atapi_issue(ahci_drive_t* drive, int slot, uint8_t packet[16],
    uint64_t buf, uint32_t bytes, int write, int sclo)
{
    fis_h2d_t fis = {
        .type = 0x27,
        .c = 1,
        .command = 0xA0,
        .featurel = (buf && bytes) ? 1 : 0,
        .lba1 = 0xFF,
        .lba2 = 0xFF,
    };
    return ahci_issue(drive->port, slot, &fis, buf, bytes, write, packet, sclo);
}

static int ahci_atapi_start(ahci_drive_t* drive) {
    uint8_t packet[16] = {0x1B, 0, 0, 0, 0x01};
    return ahci_atapi_issue(drive, 0, packet, 0, 0, 0, drive->sclo);
}

int ahci_atapi_sense(ahci_drive_t* drive, uint8_t* key, uint8_t* asc, uint8_t* ascq) {
    uint8_t packet[16] = {0x03, 0, 0, 0, 18};
    memset(bounce, 0, 18);
    int res = ahci_atapi_issue(drive, 0, packet, bounce_phys, 18, 0, drive->sclo);
    if (res < 0) return res;
    *key = bounce[2] & 0xF;
    *asc = bounce[12];
    *ascq = bounce[13];
    return 0;
}

int ahci_atapi_read_capacity(ahci_drive_t* drive) {
    drive->sector_count = 0;
    drive->sector_size = 2048;
    uint8_t packet[16] = {0x25};
    memset(bounce, 0, 8);
    int res = ahci_atapi_issue(drive, 0, packet, bounce_phys, 8, 0, drive->sclo);
    if (res < 0) return res;

    uint32_t last_lba = (bounce[0] << 24) | (bounce[1] << 16) | (bounce[2] << 8) | bounce[3];
    uint32_t block_size = (bounce[4] << 24) | (bounce[5] << 16) | (bounce[6] << 8) | bounce[7];
    if (block_size == 0) return -EIO;

    drive->sector_count = (uint64_t)last_lba + 1;
    drive->sector_size = block_size;
    return 0;
}

int ahci_atapi_ready(ahci_drive_t* drive, int* changed) {
    uint8_t tur[16] = {0};
    int started = 0;
    *changed = 0;

    for (int i = 0; i < 10; i++) {
        if (ahci_atapi_issue(drive, 0, tur, 0, 0, 0, drive->sclo) == 0) return 0;

        uint8_t key, asc, ascq;
        if (ahci_atapi_sense(drive, &key, &asc, &ascq) < 0) return -EIO;

        if (key == 0x06) {
            if (asc == 0x28) *changed = 1;
            continue;
        }
        if (key == 0x02 && asc == 0x04) {
            if (ascq == 0x02 && !started) {
                started = 1;
                ahci_atapi_start(drive);
            }
            continue;
        }
        if (key == 0x02 && asc == 0x3A) {
            *changed = 1;
            if (i < 3) continue;
            if (!started) {
                started = 1;
                ahci_atapi_start(drive);
                continue;
            }
            return -ENOMEDIUM;
        }
        return -EIO;
    }
    return -EIO;
}

int ahci_atapi_refresh(ahci_drive_t* drive) {
    int changed;
    int res = ahci_atapi_ready(drive, &changed);
    if (res < 0) {
        drive->sector_count = 0;
        return res;
    }
    if (changed || drive->sector_count == 0) {
        res = ahci_atapi_read_capacity(drive);
        if (res < 0) return res;
    }
    return 0;
}

int ahci_read_sectors(int drive_number, uint64_t lba, uint8_t *buffer, uint64_t count) {
    if (drive_number < 0 || drive_number >= drive_count) return -ENODEV;
    if (count == 0) return 0;
    ahci_drive_t* drive = &drives[drive_number];
    if (drive->is_atapi) {
        int res = ahci_atapi_refresh(drive);
        if (res < 0) return res;
    }
    if (!drive->sector_size || drive->sector_size > 4096) return -EIO;
    if (lba > drive->sector_count || count > drive->sector_count - lba) return -EINVAL;
    const uint32_t bounce_sectors = 4096 / drive->sector_size;
    if (!bounce_sectors) return -EIO;
    while (count) {
        uint32_t n = count < bounce_sectors ? count : bounce_sectors;
        int res;
        if (drive->is_atapi) {
            uint8_t packet[16] = {0xA8, 0,
            (lba >> 0x18) & 0xFF, (lba >> 0x10) & 0xFF,
            (lba >> 0x08) & 0xFF, (lba >> 0x00) & 0xFF,
            (n >> 0x18) & 0xFF, (n >> 0x10) & 0xFF,
            (n >> 0x08) & 0xFF, (n >> 0x00) & 0xFF};
            res = ahci_atapi_issue(drive, 0, packet, bounce_phys, n * drive->sector_size, 0, drive->sclo);
        } else {
            res = ahci_ata_issue(drive->port, 0, 0x25, lba, n, bounce_phys, n * drive->sector_size, 0, drive->sclo);
        }
        if (res < 0) return res;
        memcpy(buffer, bounce, n * drive->sector_size);
        buffer += n * drive->sector_size;
        lba += n;
        count -= n;
    }
    return 0;
}

int ahci_write_sectors(int drive_number, uint64_t lba, const uint8_t *buffer, uint64_t count) {
    if (drive_number < 0 || drive_number >= drive_count) return -ENODEV;
    if (count == 0) return 0;
    ahci_drive_t* drive = &drives[drive_number];
    if (drive->is_atapi) {
        int res = ahci_atapi_refresh(drive);
        if (res < 0) return res;
    }
    if (!drive->sector_size || drive->sector_size > 4096) return -EIO;
    if (lba > drive->sector_count || count > drive->sector_count - lba) return -EINVAL;
    const uint32_t bounce_sectors = 4096 / drive->sector_size;
    if (!bounce_sectors) return -EIO;
    while (count) {
        uint32_t n = count < bounce_sectors ? count : bounce_sectors;
        memcpy(bounce, buffer, n * drive->sector_size);
        int res;
        if (drive->is_atapi) {
            uint8_t packet[16] = {0xAA, 0,
            (lba >> 0x18) & 0xFF, (lba >> 0x10) & 0xFF,
            (lba >> 0x08) & 0xFF, (lba >> 0x00) & 0xFF,
            (n >> 0x18) & 0xFF, (n >> 0x10) & 0xFF,
            (n >> 0x08) & 0xFF, (n >> 0x00) & 0xFF};
            res = ahci_atapi_issue(drive, 0, packet, bounce_phys, n * drive->sector_size, 1, drive->sclo);
        } else {
            res = ahci_ata_issue(drive->port, 0, 0x35, lba, n, bounce_phys, n * drive->sector_size, 1, drive->sclo);
        }
        if (res < 0) return res;
        buffer += n * drive->sector_size;
        lba += n;
        count -= n;
    }
    return 0;
}

int64_t ahci_get_drive_size(int drive_number) {
    if (drive_number < 0 || drive_number >= drive_count) return -ENODEV;
    ahci_drive_t* drive = &drives[drive_number];
    if (drive->is_atapi) {
        int res = ahci_atapi_refresh(drive);
        if (res < 0) return res;
    }
    return drive->sector_count * drive->sector_size;
}

int64_t ahci_get_sector_size(int drive_number) {
    if (drive_number < 0 || drive_number >= drive_count) return -ENODEV;
    ahci_drive_t* drive = &drives[drive_number];
    if (drive->is_atapi) {
        int res = ahci_atapi_refresh(drive);
        if (res < 0) return res;
    }
    return drive->sector_size;
}

int ahci_atapi_load_eject(int drive_number, int load) {
    if (drive_number < 0 || drive_number >= drive_count) return -ENODEV;
    uint8_t to_load = load ? 0x01 : 0x00;
    uint8_t packet[16] = {0};
    packet[0] = 0x1B;
    packet[4] = 0x02 | to_load;
    ahci_drive_t* drive = &drives[drive_number];
    if (!drive->is_atapi) return -ENOTTY;
    return ahci_atapi_issue(drive, 0, packet, 0, 0, 0, drive->sclo);
}

int ahci_ioctl(int drive, uint64_t request, uint64_t arg) {
    switch (request) {
        case CDROM_LOAD:
            return ahci_atapi_load_eject(drive, 1);
        case CDROM_EJECT:
            return ahci_atapi_load_eject(drive, 0);
        default:
            return -ENOTTY;
    }
}

void ahci_init(pci_device_t device) {
    if (drive_count >= 1024) return;
    klog(LOG_INFO, "ahci", "Found an AHCI controller\n");

    uint32_t command = pci_config_read32(device.bus, device.device, device.function, 0x04);
    pci_config_write32(device.bus, device.device, device.function, 0x04, command | 0x06);

    uint32_t bar5 = device.bar[5] & ~0xF;
    hba_mem_t* hba = find_available_va(0x1100);
    alloc_mmio_region((uintptr_t)hba, bar5, 0x1100, FLAGS_RW | FLAGS_PCD | FLAGS_PWT);

    hba->ghc |= (1 << 31);
    if (hba->cap2 & 0x1) {
        hba->bohc |= 0x2;
        if (spin(hba->bohc & 0x1, 5000,) < 0) return;
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
        uint32_t det = port->ssts & 0xF;
        uint32_t ipm = (port->ssts >> 8) & 0xF;
        if (det != 3 || ipm != 1) continue;
        int is_atapi;
        if (port->sig == 0x00000101) {
            is_atapi = 0;
            klog(LOG_INFO, "ahci", "Found an AHCI SATA drive\n");
        } else if (port->sig == 0xEB140101) {
            is_atapi = 1;
            klog(LOG_INFO, "ahci", "Found an AHCI ATAPI device\n");
        } else {
            continue; // Other device types are not implemented
        }

        port->cmd &= ~HBA_CMD_ST;
        if (spin(port->cmd & HBA_CMD_CR, 1000,) < 0) {
            klog(LOG_ERROR, "ahci", "Failed to stop port engine\n");
            continue;
        }
        port->cmd &= ~HBA_CMD_FRE;
        if (spin(port->cmd & HBA_CMD_FR, 1000,) < 0) {
            klog(LOG_ERROR, "ahci", "Failed to stop port engine\n");
            continue;
        }

        if (is_atapi) port->cmd |= HBA_CMD_ATAPI;
        else port->cmd &= ~HBA_CMD_ATAPI;

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
        if (spin(!(port->cmd & HBA_CMD_FR), 1000,) < 0) {
            klog(LOG_ERROR, "ahci", "Failed to start port engine\n");
            continue;
        }
        port->cmd |= HBA_CMD_ST;
        if (spin(!(port->cmd & HBA_CMD_CR), 1000,) < 0) {
            klog(LOG_ERROR, "ahci", "Failed to start port engine\n");
            continue;
        }

        drives[drive_count].controller = hba;
        drives[drive_count].port = port;
        drives[drive_count].sclo = sclo;
        drives[drive_count].is_atapi = is_atapi;

        uint16_t id[256];
        uintptr_t data_phys = get_physical_address((uintptr_t)id);
        int res = ahci_ata_issue(port, 0, is_atapi ? 0xA1 : 0xEC, 0, 0, data_phys, 512, 0, sclo);
        if (res < 0) {
            klog(LOG_ERROR, "ahci", "Failed to identify device\n");
            continue;
        }

        if (is_atapi) {
            drives[drive_count].sector_count = 0;
            drives[drive_count].sector_size = 2048;
            ahci_atapi_read_capacity(&drives[drive_count]);
        } else {
            int lba48 = id[83] & (1 << 10);
            uint64_t sectors = lba48 ? *(uint64_t*)&id[100] : *(uint32_t*)&id[60];
            uint32_t logical = 512;
            if ((id[106] & 0xC000) == 0x4000 && (id[106] & (1 << 12))) {
                logical = ((uint32_t)id[117] | (uint32_t)id[118] << 16) * 2;
            }
            drives[drive_count].sector_count = sectors;
            drives[drive_count].sector_size = logical;
        }
        drive_count++;

        char name[256];
        if (is_atapi) {
            memcpy(name, "block/ahci-cd", 13);
            itoa(atapi_count++, name + 13);
        } else {
            memcpy(name, "block/ahci-hd", 13);
            itoa(sata_count++, name + 13);
        }
        block_device_t dev = {
            .major_number = ahci_driver_index,
            .minor_number = drive_count - 1,
            .name = name,
            .is_partition = drives[drive_count - 1].is_atapi ? 1 : 0,
        };
        devfs_create_directory("block");
        register_block_device(&dev);
    }
}

void ahci_register() {
    block_driver_t ahci_driver_block = {
        .read_sectors = ahci_read_sectors,
        .write_sectors = ahci_write_sectors,
        .ioctl = ahci_ioctl,
        .get_blockdev_size = ahci_get_drive_size,
        .get_sector_size = ahci_get_sector_size,
    };
    ahci_driver_index = register_block_driver(&ahci_driver_block);
    if (ahci_driver_index < 0) {
        klogf(LOG_ERROR, "ahci", "Failed to register the AHCI block driver, errno: %d\n", ahci_driver_index);
        return;
    }
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
