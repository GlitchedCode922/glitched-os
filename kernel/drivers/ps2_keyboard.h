#pragma once

#include <stdint.h>

#define INPUT_LINE_LENGTH 1024
#define INPUT_BUFFER_LINES 20

void ps2_interrupt_handler(uint8_t scancode);
char* get_input_line();
