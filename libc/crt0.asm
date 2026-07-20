section .text
global _start
extern main
_start:
    xor rbp, rbp

    mov rdi, [rsp]
    lea rsi, [rsp + 8]
    lea rdx, [rsi + rdi * 8 + 8]

    and rsp, -16
    call main
    mov edi, eax
    mov rax, 0

    syscall ; Exit
