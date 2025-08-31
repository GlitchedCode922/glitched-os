#include "pci.h"
#include <stdint.h>
#include "ports.h"
#include <stddef.h>
#include "../console.h"

pci_device_t pci_devices[MAX_PCI_DEVICES];
size_t pci_device_count = 0;

uint32_t pci_config_read(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    uint32_t address = (1U << 31)             // enable bit
                     | (bus << 16)
                     | (device << 11)
                     | (function << 8)
                     | (offset & 0xFC);      // align to 4 bytes
    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

void pci_config_write(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value) {
    uint32_t address = (1U << 31)
                     | (bus << 16)
                     | (device << 11)
                     | (function << 8)
                     | (offset & 0xFC);
    outl(PCI_CONFIG_ADDRESS, address);
    outl(PCI_CONFIG_DATA, value);
}

void enumerate_pci() {
    pci_device_count = 0;

    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t device = 0; device < 32; device++) {
            for (uint8_t function = 0; function < 8; function++) {
                uint32_t id = pci_config_read(bus, device, function, 0x00);
                uint16_t vendor_id = id & 0xFFFF;
                if (vendor_id == 0xFFFF) continue;  // No device

                uint16_t device_id = (id >> 16) & 0xFFFF;
                uint32_t class_reg = pci_config_read(bus, device, function, 0x08);
                uint8_t class_code = (class_reg >> 24) & 0xFF;
                uint8_t irq = pci_config_read(bus, device, function, 0x3C) & 0xFF;

                // Save device info
                if (pci_device_count < MAX_PCI_DEVICES) {
                    pci_device_t *dev = &pci_devices[pci_device_count++];

                    dev->bus = bus;
                    dev->device = device;
                    dev->function = function;
                    dev->vendor_id = vendor_id;
                    dev->device_id = device_id;
                    dev->class_code = class_code;
                    dev->irq = irq;

                    // Read BARs (6 possible)
                    for (int bar = 0; bar < 6; bar++) {
                        dev->bar[bar] = pci_config_read(bus, device, function, 0x10 + bar * 4);
                    }
                }
            }
        }
    }
}

// Debugging function to print PCI devices
void print_pci_devices() {
    for (size_t i = 0; i < pci_device_count; i++) {
        pci_device_t *dev = &pci_devices[i];
        kprintf("PCI Device: Bus %d, Dev %d, Func %d | Vendor 0x%x, Device 0x%x, Class 0x%x, IRQ %d\n",
               dev->bus, dev->device, dev->function,
               dev->vendor_id, dev->device_id, dev->class_code, dev->irq);
        for (int bar = 0; bar < 6; bar++) {
            kprintf("    BAR%d: 0x%x\n", bar, dev->bar[bar]);
        }
    }
}

