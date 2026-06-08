section .data
user_rsp: dq 0
section .text
global syscall_entry
extern tss
syscall_entry:
    mov [user_rsp], rsp
    mov rsp, [tss + 4]
    ; Construct interrupt frame
    push 0x23
    push qword [user_rsp]
    push r11
    push 0x1B
    push rcx
    push 0
    push 128
    push rax
    push rbx
    push rcx
    push rdx
    push rbp
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    extern syscall
    mov rdi, rsp
    call syscall
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
