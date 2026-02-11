section .text
global kernel_entry
extern kernel_main
kernel_entry:
    xor rbp, rbp ; Clean RBP to ensure stack trace unwinding stops here
    jmp kernel_main
