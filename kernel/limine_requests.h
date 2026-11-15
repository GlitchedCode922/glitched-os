#pragma once
#include "limine.h"

extern volatile struct limine_framebuffer_request framebuffer_request;
extern volatile struct limine_hhdm_request hhdm_request;
extern volatile struct limine_memmap_request memmap_request;
extern volatile struct limine_stack_size_request stack_size_request;
extern volatile struct limine_executable_cmdline_request cmdline_request;
extern volatile struct limine_rsdp_request rsdp_request;
