#include "ps2_keyboard.h"
#include "../console.h"
#include <stdint.h>

char scancode_map[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', 
   '7', '8', '9', '0', '-', '=', '\b', '\t', 
   'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 
   'o', 'p', '[', ']', '\n', 0,  // Ctrl
   'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 
   'l', ';', '\'', '`', 0,     // Left Shift
   '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', 
   ',', '.', '/', 0,  // Right Shift
   '*', 0,  ' ', 0,    // Alt, Spacebar
   0, 0, 0, 0, 0, 0, 0, 0, // F1-F10
   0, 0, 0, 0, 0, 0, 0,    // Num lock, Scroll lock
   '7', '8', '9', '-', '4', '5', '6', '+', 
   '1', '2', '3', '0', '.', 0, 0, 0,
   0, 0, 0, 0, 0, 0
};

char scancode_shift_map[128] = {
    0,  27, '!', '@', '#', '$', '%', '^', 
   '&', '*', '(', ')', '_', '+', '\b', '\t', 
   'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 
   'O', 'P', '{', '}', '\n', 0,  // Ctrl
   'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 
   'L', ':', '"', '~', 0,     // Left Shift
   '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', 
   '<', '>', '?', 0,  // Right Shift
   '*', 0,  ' ', 0,    // Alt, Spacebar
   0, 0, 0, 0, 0, 0, 0, 0, // F1-F10
   0, 0, 0, 0, 0, 0, 0,    
   '7', '8', '9', '-', '4', '5', '6', '+', 
   '1', '2', '3', '0', '.', 0, 0, 0,
   0, 0, 0, 0, 0, 0
};

uint8_t shifted = 0;
uint8_t ctrl_pressed = 0;
uint8_t alt_pressed = 0;
uint8_t caps_lock = 0;
uint8_t num_lock = 0;

char input_buffer[INPUT_BUFFER_LINES][INPUT_LINE_LENGTH]; // Buffer for input lines
uint64_t line_index = 0;
uint64_t input_index = 0;

void ps2_interrupt_handler(uint8_t scancode) {
    if (scancode == 0xE0) { // Extended key
        return;
    }

    if (scancode & 0x80) { // Key release
        scancode &= 0x7F; // Clear the high bit
        if (scancode == 0x1D) ctrl_pressed = 0; // Ctrl released
        else if (scancode == 0x38) alt_pressed = 0; // Alt released
        else if (scancode == 0x2A || scancode == 0x36) shifted = 0; // Shift released
    } else { // Key press
        if (scancode == 0x1D) ctrl_pressed = 1; // Ctrl pressed
        else if (scancode == 0x38) alt_pressed = 1; // Alt pressed
        else if (scancode == 0x2A || scancode == 0x36) shifted = 1; // Shift pressed

        char key;
        if (shifted) {
            key = scancode_shift_map[scancode];
        } else {
            key = scancode_map[scancode];
        }

        if (key) {
            if (key == '\b') { // Backspace
                if (input_index > 0) {
                    input_index--;
                    input_buffer[line_index][input_index] = '\0';
                    uint16_t x, y;
                    get_cursor_position(&x, &y);
                    set_cursor_position(x - 1, y);
                    putchar(' ');
                    set_cursor_position(x - 1, y);
                }
            } else if (key == '\n') { // Enter
                input_buffer[line_index][input_index++] = key;
                line_index++;
                input_index = 0; // Reset index for next input
                putchar(key);
            } else if (input_index < sizeof(input_buffer[line_index]) - 1) { // Avoid buffer overflow
                input_buffer[line_index][input_index++] = key; // Add character to buffer
                putchar(key);
            }
        }
    }
}

char* get_input_line() {
    while (line_index == 0);

    static char return_value[INPUT_LINE_LENGTH];
    asm volatile("cli");

    // Copy safely and null-terminate
    for (uint64_t i = 0; i < INPUT_LINE_LENGTH - 1; i++) {
        return_value[i] = input_buffer[0][i];
    }
    return_value[INPUT_LINE_LENGTH - 1] = '\0';

    // Shift remaining lines up
    for (uint8_t i = 0; i <= line_index - 1; i++) {
        for (uint16_t j = 0; j < 256; j++) {
            input_buffer[i][j] = input_buffer[i + 1][j];
        }
    }

    line_index--;
    input_index = 0;
    asm volatile("sti");
    return return_value;
}
