#pragma once
#include <stdint.h>
#include <stddef.h>

#define COM1_PORT 0x3F8
#define COM2_PORT 0x2F8
#define COM3_PORT 0x3E8
#define COM4_PORT 0x2E8

void serial_init();
void serial_interrupt_handler(uint8_t irq);
int serial_port_exists(int port);
int serial_getc(int port, uint8_t* data, int blocking);
int serial_read(int port, char* buffer, size_t size, int blocking);
int serial_putc(int port, uint8_t data);
int serial_write(int port, const char* buffer, size_t size);
