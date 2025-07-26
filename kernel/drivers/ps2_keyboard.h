#pragma once

#include <stdint.h>

void ps2_interrupt_handler(uint8_t scancode);
char* get_input_line();
