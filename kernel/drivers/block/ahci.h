#pragma once
#include <stdint.h>
#include "io/pci.h"

void ahci_init(pci_device_t device);
void ahci_register();
