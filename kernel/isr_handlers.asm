section .text
global isr_common
global isr0
global isr1
global isr2
global isr3
global isr4
global isr5
global isr6
global isr7
global isr8
global isr9
global isr10
global isr11
global isr12
global isr13
global isr14
global isr15
global isr16
global isr17
global isr18
global isr19
global isr20
global isr21
global isr22
global isr23
global isr24
global isr25
global isr26
global isr27
global isr28
global isr29
global isr30
global isr31

isr_common:
    push rax          ; Save registers
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
    extern interrupt_handler
    mov rdi, rsp
    call interrupt_handler ; Call the common interrupt handler
    pop r15           ; Restore registers
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

isr_common_err:
    push rax          ; Save registers
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
    extern interrupt_handler
    mov rdi, rsp
    call interrupt_handler ; Call the common interrupt handler
    pop r15           ; Restore registers
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
    add rsp, 8
    iretq

isr0: ; Division by zero
    push 0
    push 0
    jmp isr_common

isr1: ; Debug
    push 0
    push 1
    jmp isr_common

isr2: ; Non-maskable interrupt
    push 0
    push 2
    jmp isr_common

isr3: ; Breakpoint
    push 0
    push 3
    jmp isr_common

isr4: ; Overflow
    push 0
    push 4
    jmp isr_common

isr5: ; Bound range exceeded
    push 0
    push 5
    jmp isr_common

isr6: ; Invalid opcode
    push 0
    push 6
    jmp isr_common

isr7: ; Device not available
    push 0
    push 7
    jmp isr_common

isr8: ; Double fault (Error code pushed by CPU)
    push 8
    jmp isr_common_err

isr9: ; Coprocessor segment overrun
    push 0
    push 9
    jmp isr_common

isr10: ; Invalid TSS (Error code pushed by CPU)
    push 10
    jmp isr_common_err

isr11: ; Segment not present (Error code pushed by CPU)
    push 11
    jmp isr_common_err

isr12: ; Stack segment fault (Error code pushed by CPU)
    push 12
    jmp isr_common_err

isr13: ; General protection fault (Error code pushed by CPU)
    push 13
    jmp isr_common_err

isr14: ; Page fault (Error code pushed by CPU)
    push 14
    jmp isr_common_err

isr15: ; Reserved
    push 15
    jmp isr_common

isr16: ; x87 FPU floating-point error
    push 0
    push 16
    jmp isr_common

isr17: ; Alignment check (Error code pushed by CPU)
    push 17
    jmp isr_common_err

isr18: ; Machine check
    push 0
    push 18
    jmp isr_common

isr19: ; SIMD floating-point exception
    push 0
    push 19
    jmp isr_common

isr20: ; Virtualization exception
    push 0
    push 20
    jmp isr_common

isr21: ; Control protection exception (Error code pushed by CPU)
    push 21
    jmp isr_common_err

isr22: ; Reserved
    push 0
    push 22
    jmp isr_common

isr23: ; Reserved
    push 0
    push 23
    jmp isr_common

isr24: ; Reserved
    push 0
    push 24
    jmp isr_common

isr25: ; Reserved
    push 0
    push 25
    jmp isr_common

isr26: ; Reserved
    push 0
    push 26
    jmp isr_common

isr27: ; Reserved
    push 0
    push 27
    jmp isr_common

isr28: ; Hypervisor injection exception
    push 0
    push 28
    jmp isr_common

isr29: ; VMM communication exception (Error code pushed by CPU)
    push 29
    jmp isr_common_err

isr30: ; Security exception (Error code pushed by CPU)
    push 30
    jmp isr_common_err

isr31: ; Reserved
    push 0
    push 31
    jmp isr_common
