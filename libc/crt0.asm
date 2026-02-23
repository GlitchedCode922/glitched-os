section .text
global _start
extern main
_start:
    pop rdi
    lea rsi, [rsp]
    ; Call the main function
    call main
    mov r12, rax
    mov rax, 0
    int 0x80 ; Exit
