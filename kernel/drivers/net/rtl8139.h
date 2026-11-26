#pragma once
#include <stdint.h>
#include "io/pci.h"

void rtl8139_init(pci_device_t device);
void rtl8139_irq_handler(uint8_t irq);
int turn_on_rtl8139(int card);
void rtl8139_register();
int rtl8139_read_packet(int card, void** buffer);
int rtl8139_send_packet(int card, void* buffer, int length);
uint8_t* rtl8139_get_mac_address(int card);
