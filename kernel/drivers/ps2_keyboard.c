#include "ps2_keyboard.h"
#include "../console.h"
#include "tty.h"
#include <stdint.h>

#define KEY_RELEASE 0x80
#define EXTENDED_KEY 0xE0

tty_t keyboard_tty = {.echo = console_echo, .write = console_write, .termios = {.c_lflag = ICANON | ECHO | ECHOE}};

char scancode_map[128] = {
    0,27,'1','2','3','4','5','6','7','8','9','0','-','=','\x7f','\t',
    'q','w','e','r','t','y','u','i','o','p','[',']','\n',0,
    'a','s','d','f','g','h','j','k','l',';','\'','`',0,
    '\\','z','x','c','v','b','n','m',',','.','/',0,
    '*',0,' ',0
};

char scancode_shift_map[128] = {
    0,27,'!','@','#','$','%','^','&','*','(',')','_','+','\x7f','\t',
    'Q','W','E','R','T','Y','U','I','O','P','{','}','\n',0,
    'A','S','D','F','G','H','J','K','L',':','"','~',0,
    '|','Z','X','C','V','B','N','M','<','>','?',0,
    '*',0,' ',0
};

uint8_t shifted = 0;
uint8_t ctrl_pressed = 0;
uint8_t alt_pressed = 0;
uint8_t extended = 0;

uint8_t input_disabled = 0;

void ps2_interrupt_handler_internal(uint8_t scancode) {
    if (scancode == EXTENDED_KEY) {
        extended = 2;
    } else {
        extended = extended > 0 ? extended - 1 : 0;
    }

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
    if (extended && scancode == 0x48) {
        tty_char_recv(&keyboard_tty, '\033');
        tty_char_recv(&keyboard_tty, '[');
        tty_char_recv(&keyboard_tty, 'A');
        return;
    }
    if (extended && scancode == 0x50) {
        tty_char_recv(&keyboard_tty, '\033');
        tty_char_recv(&keyboard_tty, '[');
        tty_char_recv(&keyboard_tty, 'B');
        return;
    }
    if (extended && scancode == 0x4B) {
        tty_char_recv(&keyboard_tty, '\033');
        tty_char_recv(&keyboard_tty, '[');
        tty_char_recv(&keyboard_tty, 'D');
        return;
    }
    if (extended && scancode == 0x4D) {
        tty_char_recv(&keyboard_tty, '\033');
        tty_char_recv(&keyboard_tty, '[');
        tty_char_recv(&keyboard_tty, 'C');
        return;
    }

    char key = shifted ? scancode_shift_map[scancode] : scancode_map[scancode];
    if (!key) return;

    if (ctrl_pressed) {
        if (key == ' ') key = '\0';
        else if (key == '?') key = '\x7f';
        else key &= 0x1F;
    }

    tty_char_recv(&keyboard_tty, key);
}

void ps2_interrupt_handler(uint8_t scancode) {
    if (input_disabled) return;
    input_disabled++;
    ps2_interrupt_handler_internal(scancode);
    input_disabled--;
}
