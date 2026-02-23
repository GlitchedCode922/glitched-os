section .text
global jump_to_user
global context_switch
jump_to_user:
    cli                     ; Clear interrupts during switch
    mov rbp, 0
    ; Set user data segments
    mov ax, 0x23            ; User data segment selector (RPL=3)
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    ; Prepare stack for iretq
    push 0x23               ; User SS
    push rsi                ; User RSP
    push 0x202              ; RFLAGS
    push 0x1B               ; User CS
    push rdi                ; User RIP
    iretq                   ; Switch to user mode

context_switch:
    mov rsp, rdi
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rbp
    pop rdx
    pop rcx
    pop rbx
    pop rax
    add rsp, 16
    iretq
