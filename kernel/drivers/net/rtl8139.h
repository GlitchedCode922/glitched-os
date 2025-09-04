#include <stdint.h>
#include "../../io/pci.h"

void rtl8139_init(pci_device_t device);
void rtl8139_irq_handler(uint8_t irq);
void turn_on_rtl8139(int card);
void register_rtl8139_driver();
int rtl8139_read_packet(int card, void** buffer);
void rtl8139_send_packet(int card, void* buffer, int length);
uint8_t* rtl8139_get_mac_address(int card);
