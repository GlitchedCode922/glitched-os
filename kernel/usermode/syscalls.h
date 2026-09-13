#pragma once
#include "../idt.h"
#include "../uapi/syscalls.h" // IWYU pragma: export

void syscall_init();
void syscall(iframe_t* iframe);
