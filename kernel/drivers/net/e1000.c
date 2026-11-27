#include "e1000.h"
#include "drivers/net.h"
#include "io/pci.h"
#include "net/ethernet.h"
#include "util/string.h"
#include "util/log.h"
#include "util/spin.h"
#include "error.h"
#include "memory/paging.h"
#include "memory/mman.h"
#include <stddef.h>
#include <stdint.h>

typedef volatile struct {
    uint32_t ctrl;          /* 0x0000 */
    uint8_t _pad0[4];       /* 0x0004 */
    uint32_t status;        /* 0x0008 */
    uint8_t _pad1[4];       /* 0x000C */
    uint32_t eecd;          /* 0x0010 */
    uint32_t eerd;          /* 0x0014 */
    uint32_t ctrl_ext;      /* 0x0018 */
    uint8_t _pad2[4];       /* 0x001C */
    uint32_t mdic;          /* 0x0020 */
    uint8_t _pad3[0x9C];    /* 0x0024 */
    uint32_t icr;           /* 0x00C0 */
    uint32_t itr;           /* 0x00C4 */
    uint32_t ics;           /* 0x00C8 */
    uint8_t _pad4[4];       /* 0x00CC */
    uint32_t ims;           /* 0x00D0 */
    uint8_t _pad5[4];       /* 0x00D4 */
    uint32_t imc;           /* 0x00D8 */
    uint8_t _pad6[0x24];    /* 0x00DC */
    uint32_t rctl;          /* 0x0100 */
    uint8_t _pad7[0x2FC];   /* 0x0104 */
    uint32_t tctl;          /* 0x0400 */
    uint8_t _pad8[0xC];     /* 0x0404 */
    uint32_t tipg;          /* 0x0410 */
    uint8_t _pad9[0x23EC];  /* 0x0414 */
    uint32_t rdbal;         /* 0x2800 */
    uint32_t rdbah;         /* 0x2804 */
    uint32_t rdlen;         /* 0x2808 */
    uint8_t _pad10[4];      /* 0x280C */
    uint32_t rdh;           /* 0x2810 */
    uint8_t _pad11[4];      /* 0x2814 */
    uint32_t rdt;           /* 0x2818 */
    uint8_t _pad12[0xFE4];  /* 0x281C */
    uint32_t tdbal;         /* 0x3800 */
    uint32_t tdbah;         /* 0x3804 */
    uint32_t tdlen;         /* 0x3808 */
    uint8_t _pad13[4];      /* 0x380C */
    uint32_t tdh;           /* 0x3810 */
    uint8_t _pad14[4];      /* 0x3814 */
    uint32_t tdt;           /* 0x3818 */
    uint8_t _pad15[0x19E4]; /* 0x381C */
    uint32_t mta[128];      /* 0x5200 */
    uint32_t ral0;          /* 0x5400 */
    uint32_t rah0;          /* 0x5404 */
} __attribute__((packed)) e1000_regs_t;

typedef volatile struct {
    uint64_t addr;
    uint16_t len;
    uint16_t checksum;
    uint8_t status;
    uint8_t errors;
    uint16_t special;
} __attribute__((packed)) rx_desc_t;

typedef volatile struct {
    uint64_t addr;
    uint16_t len;
    uint8_t cso;
    uint8_t cmd;
    uint8_t status;
    uint8_t css;
    uint16_t special;
} __attribute__((packed)) tx_desc_t;

#define CTRL_RST    (1 << 26)
#define CTRL_SLU    (1 << 6)
#define CTRL_ASDE   (1 << 5)
#define CTRL_FRCSPD (1 << 11)
#define CTRL_FRCDPX (1 << 12)

#define DESC_DD     (1)
#define RX_EOP      (1 << 1)
#define TX_CMD_EOP  (1)
#define TX_CMD_IFCS (1 << 1)
#define TX_CMD_RS   (1 << 3)

#define RCTL_EN     (1 << 1)
#define RCTL_BAM    (1 << 15)
#define RCTL_SECRC  (1 << 26)

#define TCTL_EN     (1 << 1)
#define TCTL_PSP    (1 << 3)

typedef struct {
    e1000_regs_t* regs;
    uint8_t mac[6];
    rx_desc_t* rx_ring;
    tx_desc_t* tx_ring;
    int rx_current;
    int tx_current;
    uint64_t tx_buffers[32];
} e1000_t;

static e1000_t cards[1024];
static int card_count = 0;
int e1000_driver_index = 0;

pci_device_id_t e1000_device_ids[] = {
    {0x8086, 0x100E},
};

int e1000_get_mac(int card, uint8_t mac[6]) {
    if (card < 0 || card >= card_count) return -ENODEV;

    for (int i = 0; i < 6; i++) {
        mac[i] = cards[card].mac[i];
    }

    return 0;
}

int e1000_send(int card, void* buffer, int length) {
    if (card < 0 || card >= card_count) return -EINVAL;
    int tx_current = cards[card].tx_current;
    tx_desc_t* desc = cards[card].tx_ring + tx_current;
    uint64_t tx_buffer = cards[card].tx_buffers[tx_current];
    if (spin(!(desc->status & DESC_DD), 1000,) < 0) return -EIO;
    memcpy(hhdm_base + tx_buffer, buffer, length);

    desc->addr = tx_buffer;
    desc->len = length;
    desc->cso = 0;
    desc->cmd = TX_CMD_EOP | TX_CMD_IFCS | TX_CMD_RS;
    desc->css = 0;
    desc->special = 0;
    desc->status = 0;

    tx_current = (tx_current + 1) % 32;
    cards[card].tx_current = tx_current;
    __sync_synchronize();
    cards[card].regs->tdt = tx_current;

    return 0;
}

void e1000_isr(uint8_t irq) {
    for (int i = 0; i < card_count; i++) {
        rx_desc_t* rx_ring = cards[i].rx_ring;
        uint32_t cause = cards[i].regs->icr;
        if (!cause) continue;
        while (rx_ring[cards[i].rx_current].status & DESC_DD) {
            rx_desc_t* desc = rx_ring + cards[i].rx_current;
            if ((desc->status & RX_EOP) && !desc->errors && desc->len <= 2048) {
                frame_received(
                    get_global_if_index(e1000_driver_index, i),
                    hhdm_base + desc->addr, desc->len
                );
            }
            desc->len = 0;
            desc->checksum = 0;
            desc->status = 0;
            desc->errors = 0;
            desc->special = 0;
            int slot = cards[i].rx_current;
            cards[i].rx_current = (cards[i].rx_current + 1) % 32;
            __sync_synchronize();
            cards[i].regs->rdt = slot;
        }
    }
}

void e1000_init(pci_device_t device) {
    if (card_count >= 1024) return;
    klog(LOG_INFO, "e1000", "Found an e1000 network card\n");

    uint32_t command = pci_config_read32(device.bus, device.device, device.function, 0x04);
    pci_config_write32(device.bus, device.device, device.function, 0x04, command | 0x06);

    uint32_t bar0 = device.bar[0] & ~0xF;
    e1000_regs_t* regs = find_available_va(0x20000);
    alloc_mmio_region((uintptr_t)regs, bar0, 0x20000, FLAGS_RW | FLAGS_PCD | FLAGS_PWT);

    regs->imc = 0xFFFFFFFF;
    (void)regs->icr;

    regs->ctrl |= CTRL_RST;
    if (spin(regs->ctrl & CTRL_RST, 2000,) < 0) {
        klog(LOG_ERROR, "e1000", "Failed to initialize e1000 card (reset timed out)\n");
        return;
    }
    spin(1, 10,);

    regs->imc = 0xFFFFFFFF;
    (void)regs->icr;

    e1000_t* card = cards + card_count;
    card->regs = regs;
    uint32_t ral0 = regs->ral0;
    uint32_t rah0 = regs->rah0;
    card->mac[0] = ral0 & 0xFF;
    card->mac[1] = (ral0 >> 8) & 0xFF;
    card->mac[2] = (ral0 >> 16) & 0xFF;
    card->mac[3] = ral0 >> 24;
    card->mac[4] = rah0 & 0xFF;
    card->mac[5] = (rah0 >> 8) & 0xFF;

    if (!(rah0 & (1 << 31))) {
        klog(LOG_ERROR, "e1000", "Invalid MAC address\n");
        return;
    }

    regs->ctrl |= CTRL_SLU;
    regs->ctrl |= CTRL_ASDE;
    regs->ctrl &= ~CTRL_FRCSPD;
    regs->ctrl &= ~CTRL_FRCDPX;

    for (int i = 0; i < 128; i++) regs->mta[i] = 0;

    uint64_t rx_phys = allocate_physical_region(PAGE_ALIGN(32 * sizeof(rx_desc_t)) / PAGE_SIZE);
    rx_desc_t* rx_ring = hhdm_base + rx_phys;
    card->rx_ring = rx_ring;

    for (int i = 0; i < 32; i++) {
        rx_ring[i].addr = allocate_physical_page();
        rx_ring[i].len = 0;
        rx_ring[i].checksum = 0;
        rx_ring[i].status = 0;
        rx_ring[i].errors = 0;
        rx_ring[i].special = 0;
    }

    regs->rdbal = (uint32_t)rx_phys;
    regs->rdbah = (uint32_t)(rx_phys >> 32);
    regs->rdlen = 32 * sizeof(rx_desc_t);
    regs->rdh = 0;
    regs->rdt = 32 - 1;
    card->rx_current = 0;
    __sync_synchronize();

    regs->rctl = RCTL_EN | RCTL_BAM | RCTL_SECRC;

    uint64_t tx_phys = allocate_physical_region(PAGE_ALIGN(32 * sizeof(tx_desc_t)) / PAGE_SIZE);
    tx_desc_t* tx_ring = hhdm_base + tx_phys;
    card->tx_ring = tx_ring;

    for (int i = 0; i < 32; i++) {
        card->tx_buffers[i] = allocate_physical_page();
        tx_ring[i].status = DESC_DD;
    }

    regs->tdbal = (uint32_t)tx_phys;
    regs->tdbah = (uint32_t)(tx_phys >> 32);
    regs->tdlen = 32 * sizeof(rx_desc_t);
    regs->tdh = 0;
    regs->tdt = 0;
    card->tx_current = 0;
    regs->tipg = 0x0060200A;
    __sync_synchronize();

    regs->tctl = TCTL_EN | TCTL_PSP | (0x0F << 4) | (0x40 << 12);

    regs->ims = (1 << 7);
    if (spin(!(regs->status & (1 << 1)), 1000,) < 0) {
        klog(LOG_WARN, "e1000", "Network card has no link\n");
        return;
    }
    card_count++;
    klog(LOG_INFO, "e1000", "Successfully initialized e1000 network card\n");
    register_net_interface(e1000_driver_index, card_count - 1);
}

void e1000_register() {
    net_driver_t e1000_net = {
        .get_mac_address = e1000_get_mac,
        .send_packet = e1000_send,
    };
    e1000_driver_index = register_net_driver(e1000_net);
    pci_driver_t e1000_pci = {
        .init = e1000_init,
        .isr = e1000_isr,
        .mode = 0,
        .device_ids = e1000_device_ids,
        .device_id_count = sizeof(e1000_device_ids) / sizeof(pci_device_id_t),
    };
    register_pci_driver(e1000_pci);
}
