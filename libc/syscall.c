#include "syscall.h"
#include <stdint.h>

uint64_t syscall(uint64_t syscall_number, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    uint64_t result;
    uint64_t r12_stored;
    asm volatile (
        "mov %%r12, %0\n"      // Store r12
        : "=r"(r12_stored)     // output
    );
    asm volatile (
        "mov %1, %%rax\n"      // syscall number
        "mov %2, %%r12\n"      // arg1
        "mov %3, %%rsi\n"      // arg2
        "mov %4, %%rdx\n"      // arg3
        "mov %5, %%r10\n"      // arg4
        "mov %6, %%r8\n"       // arg5
        "int $0x80\n"           // invoke syscall
        "mov %%rax, %0\n"      // store result
        : "=r"(result)         // output
        : "r"(syscall_number), "r"(arg1), "r"(arg2), "r"(arg3), "r"(arg4), "r"(arg5)  // inputs
        : "%rax", "%r12", "%rsi", "%rdx", "%r10", "%r8", "%rcx", "%r11" // clobbered registers
    );
    asm volatile (
        "mov %0, %%r12\n"      // Restore r12
        : 
        : "r"(r12_stored)      // input
    );
    return result;
}
