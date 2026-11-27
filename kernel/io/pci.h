#pragma once
#include <stdint.h>

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC
#define MAX_PCI_DEVICES    256
#define MAX_PCI_DRIVERS    32

// Basic PCI device structure
typedef struct {
    uint8_t  bus;
    uint8_t  device;
    uint8_t  function;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t  class_code;
    uint8_t  subclass;
    uint8_t  prog_if;
    uint8_t  irq;
    uint32_t bar[6];
} pci_device_t;

typedef struct {
    uint16_t vendor;
    uint16_t device;
} pci_device_id_t;

typedef struct {
    void (*init)(pci_device_t);
    void (*isr)(uint8_t);
    int mode; // 0: device IDs, 1: class
    pci_device_id_t* device_ids;
    int device_id_count;
    uint8_t class_code;
    uint8_t subclass;
    uint8_t prog_if;
} pci_driver_t;

extern pci_device_t pci_devices[MAX_PCI_DEVICES];

void register_pci_driver(pci_driver_t driver);
void enumerate_pci();
void print_pci_devices();
void pci_irq_handler(uint8_t irq);
uint8_t pci_config_read8(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
uint16_t pci_config_read16(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
uint32_t pci_config_read32(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
void pci_config_write8(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint8_t value);
void pci_config_write16(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint16_t value);
void pci_config_write32(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value);
