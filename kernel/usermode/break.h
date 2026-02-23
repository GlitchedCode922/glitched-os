#pragma once
#include <stdint.h>

void* set_brk(void* addr);
void* sbrk(intptr_t increment);
