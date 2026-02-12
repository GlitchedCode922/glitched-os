#pragma once

#include <stdint.h>

#define INPUT_LINE_LENGTH 1024
#define INPUT_BUFFER_LINES 20

void ps2_interrupt_handler(uint8_t scancode);
uint64_t get_input(char* buf, uint64_t max_size, uint8_t blocking);
