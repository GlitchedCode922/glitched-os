#pragma once
#include <stdint.h>

extern void* initial_brk;
extern void* brk;

void* set_brk(void* addr);
void* sbrk(intptr_t increment);
