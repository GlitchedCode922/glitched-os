#include "kbd.h"
#include "ps2_keyboard.h"

char* kbdinput() {
    return get_input_line();
}
