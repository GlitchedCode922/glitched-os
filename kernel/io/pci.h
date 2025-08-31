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
    uint8_t  irq;
    uint32_t bar[6];
} pci_device_t;

typedef struct {
    void (*init)(pci_device_t);
    void (*isr)(uint8_t);
    uint16_t vendor;
    uint16_t device;
} pci_driver_t;

void register_pci_driver(pci_driver_t driver);
void enumerate_pci();
void print_pci_devices();
void pci_irq_handler(uint8_t irq);
uint32_t pci_config_read(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
void pci_config_write(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value);
