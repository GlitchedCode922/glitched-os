#pragma once
#include <stdint.h>
#include <stddef.h>
#include "tty.h"

#define COM1_PORT 0x3F8
#define COM2_PORT 0x2F8
#define COM3_PORT 0x3E8
#define COM4_PORT 0x2E8

extern tty_t serial_ttys[4];

void serial_init();
void serial_interrupt_handler(uint8_t irq);
int serial_port_exists(int port);
size_t serial_write(tty_t* tty, const char* buffer, size_t size);
