#pragma once
#include <stdint.h>

extern uint8_t has_xsave;
extern uint32_t fpu_memory_size;

void init_fpu();
void save_fpu(void* memory);
void restore_fpu(void* memory);
