section .text
global _start
extern _libc_init_main
_start:
    xor rbp, rbp

    mov rdi, [rsp]
    lea rsi, [rsp + 8]
    lea rdx, [rsi + rdi * 8 + 8]

    and rsp, -16
    jmp _libc_init_main
