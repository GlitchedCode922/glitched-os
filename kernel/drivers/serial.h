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
int64_t serial_write(void* data, const uint8_t* buffer, uint64_t size);
