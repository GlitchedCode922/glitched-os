#pragma once
#include <stdint.h>
#include "../idt.h"

#define PIC1_CMD 0x20
#define PIC1_DATA 0x21
#define PIC2_CMD 0xA0
#define PIC2_DATA 0xA1

#define ICW1_INIT 0x10
#define ICW1_ICW4 0x01

void pic_init();
void pic_enable_irq(int irq);
void pic_disable_irq(int irq);
void pic_send_eoi(int irq);
void irq_handler(int irq, iframe_t* iframe);
void irq0_handler(iframe_t* iframe);
void irq1_handler();
void irq7_handler();
void irq15_handler();
void pic_remap(int offset1, int offset2);
