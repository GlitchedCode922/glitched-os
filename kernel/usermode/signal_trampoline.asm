section .rodata
global sigtramp_start
global sigtramp_end

sigtramp_start:
    mov rax, 76
    syscall
    ud2 ; Unreachable
sigtramp_end:
