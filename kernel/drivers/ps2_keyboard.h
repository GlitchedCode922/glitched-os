#pragma once

#include "tty.h"
#include <stdint.h>

#define INPUT_LINE_LENGTH 1024
#define INPUT_BUFFER_LINES 20

extern uint8_t input_disabled;

void ps2_interrupt_handler(uint8_t scancode);
