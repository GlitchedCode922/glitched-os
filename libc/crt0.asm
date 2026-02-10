section .text
global _start
extern main
_start:
    ; Jump to the main function
    jmp main
