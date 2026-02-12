#include "ps2_keyboard.h"
#include "../console.h"
#include <stdint.h>

#define KEY_RELEASE 0x80
#define EXTENDED_KEY 0xE0

char scancode_map[128] = {
    0,27,'1','2','3','4','5','6','7','8','9','0','-','=','\b','\t',
    'q','w','e','r','t','y','u','i','o','p','[',']','\n',0,
    'a','s','d','f','g','h','j','k','l',';','\'','`',0,
    '\\','z','x','c','v','b','n','m',',','.','/',0,
    '*',0,' ',0
};

char scancode_shift_map[128] = {
    0,27,'!','@','#','$','%','^','&','*','(',')','_','+','\b','\t',
    'Q','W','E','R','T','Y','U','I','O','P','{','}','\n',0,
    'A','S','D','F','G','H','J','K','L',':','"','~',0,
    '|','Z','X','C','V','B','N','M','<','>','?',0,
    '*',0,' ',0
};

uint8_t shifted = 0;
uint8_t ctrl_pressed = 0;
uint8_t alt_pressed = 0;

char input_buffer[INPUT_BUFFER_LINES][INPUT_LINE_LENGTH];
uint64_t line_index = 0;
uint64_t input_index = 0;
uint64_t input_length = 0;

void set_cursor_linear(int32_t pos) {
    if (pos < 0) pos = 0;
    int32_t max = width * height - 1;
    if (pos > max) pos = max;

    uint16_t y = pos / width;
    uint16_t x = pos % width;
    set_cursor_position(x, y);
}

int32_t get_cursor_linear() {
    uint16_t x, y;
    get_cursor_position(&x, &y);
    return y * width + x;
}

void offset_cursor(int16_t offset) {
    set_cursor_linear(get_cursor_linear() + offset);
}

void redraw_tail() {
    uint16_t cx, cy;
    get_cursor_position(&cx, &cy);

    for (uint64_t i = input_index; i < input_length; i++) {
        putchar(input_buffer[line_index][i]);
    }

    putchar(' '); // erase leftover

    set_cursor_position(cx, cy);
}

void insert_char(char c) {
    if (input_length >= INPUT_LINE_LENGTH - 1) return;

    for (int64_t i = input_length; i > (int64_t)input_index; i--) {
        input_buffer[line_index][i] = input_buffer[line_index][i - 1];
    }

    input_buffer[line_index][input_index] = c;
    input_index++;
    input_length++;

    putchar(c);
    redraw_tail();
}

void backspace_char() {
    if (input_index == 0) return;

    input_index--;
    input_length--;

    for (uint64_t i = input_index; i < input_length; i++) {
        input_buffer[line_index][i] = input_buffer[line_index][i + 1];
    }

    offset_cursor(-1);
    redraw_tail();
}

void ps2_interrupt_handler(uint8_t scancode) {

    if (scancode == EXTENDED_KEY) return;

    if (scancode & KEY_RELEASE) {
        scancode &= 0x7F;
        if (scancode == 0x2A || scancode == 0x36) shifted = 0;
        else if (scancode == 0x1D) ctrl_pressed = 0;
        else if (scancode == 0x38) alt_pressed = 0;
        return;
    }

    if (scancode == 0x2A || scancode == 0x36) { shifted = 1; return; }
    if (scancode == 0x1D) { ctrl_pressed = 1; return; }
    if (scancode == 0x38) { alt_pressed = 1; return; }

    /* Arrow keys */
    if (scancode == 0x4B) {
        if (input_index > 0) {
            input_index--;
            offset_cursor(-1);
        }
        return;
    }
    if (scancode == 0x4D) {
        if (input_index < input_length) {
            input_index++;
            offset_cursor(1);
        }
        return;
    }

    char key = shifted ? scancode_shift_map[scancode]
                       : scancode_map[scancode];
    if (!key) return;

    if (key == '\b') {
        backspace_char();
    }
    else if (key == '\n') {
        input_buffer[line_index][input_length++] = '\n';
        input_buffer[line_index][input_length] = '\0';
        putchar('\n');

        line_index++;
        input_index = 0;
        input_length = 0;
    }
    else {
        insert_char(key);
    }
}

uint64_t get_input(char* buf, uint64_t max_size, uint8_t blocking) {
    if (!buf || max_size == 0) return 0;

    // wait if blocking
    if (blocking) {
        while (line_index == 0);
    }

    asm volatile("cli");

    // nothing available
    if (line_index == 0 && input_length == 0) {
        asm volatile("sti");
        return 0;
    }

    uint64_t copied = 0;

    // read from first queued line
    while (copied < max_size && line_index > 0) {
        char *line = input_buffer[0];

        // determine length of first line
        uint64_t len = 0;
        while (len < INPUT_LINE_LENGTH && line[len] != '\0')
            len++;

        if (len == 0) break;

        uint64_t to_copy = len;
        if (to_copy > (max_size - copied))
            to_copy = max_size - copied;

        // copy bytes
        for (uint64_t i = 0; i < to_copy; i++) {
            buf[copied + i] = line[i];
        }
        copied += to_copy;

        // shift remaining chars in this line
        for (uint64_t i = 0; i < len - to_copy; i++) {
            line[i] = line[i + to_copy];
        }
        line[len - to_copy] = '\0';

        // if line emptied, pop it
        if (line[0] == '\0') {
            for (uint64_t i = 0; i < line_index - 1; i++) {
                for (uint64_t j = 0; j < INPUT_LINE_LENGTH; j++) {
                    input_buffer[i][j] = input_buffer[i + 1][j];
                }
            }
            line_index--;
        }

        // partial read stop
        if (copied >= max_size) break;
    }

    asm volatile("sti");
    return copied;
}
