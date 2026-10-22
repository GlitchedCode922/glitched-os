#pragma once
#include <stdint.h>
#include "pdclib/_PDCLIB_internal.h"

typedef struct {
    uint64_t rbx;
    uint64_t rbp;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
    uint64_t rsp;
    uint64_t rip;
} __attribute__((packed)) __jmp_buf;

typedef __jmp_buf jmp_buf[1];

void _PDCLIB_Noreturn longjmp(jmp_buf buf, int ret);
void _PDCLIB_Noreturn _longjmp(jmp_buf buf, int ret);

int __attribute__((returns_twice)) setjmp(jmp_buf buf);
int __attribute__((returns_twice)) _setjmp(jmp_buf buf);

