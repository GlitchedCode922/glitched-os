#include "tty.h"
#include <stddef.h>
#include "../memory/mman.h"

static inline int is_printable(char c) {
    return c >= 0x20 && c <= 0x7E;
}

void tty_char_recv(tty_t *tty, char c) {
    int erased = 0;
    if (tty->termios.c_iflag & ISTRIP) c &= 0b01111111;
    if (tty->termios.c_iflag & ICRNL && c == '\r') c = '\n';
    if (tty->termios.c_lflag & ICANON) {
        if (c == '\x7f') {
            if (tty->line_index > 0) {
                tty->line_index--;
                erased = 1;
            }
        } else if (c == '\n') {
            tty->line_buffer[tty->line_index] = c;
            tty->line_index++;
            for (int i = 0; i < tty->line_index; i++) {
                if ((tty->write_head + 1) % 4096 == tty->read_head) return;
                tty->read_buffer[tty->write_head++] = tty->line_buffer[i];
                tty->write_head %= 4096;
            }
            tty->line_index = 0;
        } else {
            if (tty->line_index >= 1022) return;
            tty->line_buffer[tty->line_index] = c;
            tty->line_index++;
        }
    } else {
        if ((tty->write_head + 1) % 4096 == tty->read_head) return;
        tty->read_buffer[tty->write_head++] = c;
        tty->write_head %= 4096;
    }
    if (tty->termios.c_lflag & ECHOE && tty->termios.c_lflag & ICANON && c == '\x7f') {
        const char erase[3] = "\b \b";
        if (erased) { 
            tty->echo(tty, erase, 3);
            if (!is_printable(tty->line_buffer[tty->line_index])) {
                tty->echo(tty, erase, 3);
            }
        };
    } else if (tty->termios.c_lflag & ECHO) {
        if (is_printable(c) || c == '\n') {
            tty->echo(tty, &c, 1);
        } else {
            char caret[2];
            caret[0] = '^';
            caret[1] = c + 0x40;
            tty->echo(tty, caret, 2);
        }
    }
}

size_t tty_read(tty_t *tty, char *buffer, size_t len, int block) {
    if (block) while (tty->read_head == tty->write_head);
    int bytes_read = 0;
    for (bytes_read = 0; bytes_read < len; bytes_read++) {
        if (tty->read_head == tty->write_head) return bytes_read;
        buffer[bytes_read] = tty->read_buffer[tty->read_head++];
        tty->read_head %= 4096;
    }
    return bytes_read;
}

size_t tty_write(tty_t *tty, const char *buffer, size_t len) {
    if (tty->termios.c_oflag & OPOST) {
        for (int i = 0; i < len; i++) {
            char c = buffer[i];
            if (tty->termios.c_oflag & ONLCR && c == '\n') {
                tty->write(tty, "\r\n", 2);
            } else if (tty->termios.c_oflag & OCRNL && c == '\r') {
                tty->write(tty, "\n", 1);
            } else {
                tty->write(tty, &c, 1);
            }
        }
        return len;
    } else {
        return tty->write(tty, buffer, len);
    }
}
