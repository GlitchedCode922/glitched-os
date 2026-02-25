section .data
align 16
global has_xsave
global fpu_memory_size
fpu_cw dw 0x037F       ; Default FPU control word
has_xsave db 0
fpu_memory_size dd 0

section .text
global init_fpu
global save_fpu
global restore_fpu
init_fpu:

    ; ----------------------
    ; 1. Initialize x87 FPU
    ; ----------------------
    finit
    fldcw [fpu_cw]

    ; ----------------------
    ; 2. Initialize MMX
    ; ----------------------
    movq mm0, [fpu_cw]  ; Example MMX load
    emms                 ; Clear MMX state

    ; ----------------------
    ; 3. CPUID check for SSE & AVX
    ; ----------------------
    xor eax, eax
    cpuid                ; EAX=0: Get max CPUID leaf
    mov eax, 1
    cpuid                ; EAX=1: Feature flags
    ; EDX bits:
    ; 25 = SSE
    ; ECX bits:
    ; 26 = XSAVE
    ; 28 = AVX
    test ecx, 1 << 26
    setnz al
    mov [has_xsave], al
    jz no_xsave

    mov rax, cr4
    or rax, 1 << 18      ; OSXSAVE
    mov cr4, rax

no_xsave:

    test edx, 1 << 25
    jz no_sse

    ; ----------------------
    ; 4. Enable SSE
    ; ----------------------
    mov rax, cr4
    or rax, 1 << 9       ; OSFXSR
    or rax, 1 << 10      ; OSXMMEXCPT
    mov cr4, rax

    mov rax, cr0
    and rax, ~(1 << 2)   ; Clear EM
    or rax, 1 << 1       ; Set MP
    mov cr0, rax

no_sse:

    ; ----------------------
    ; 5. Enable AVX (if supported)
    ; ----------------------
    test ecx, 1 << 28
    jz no_avx

    xor ecx, ecx
    xgetbv               ; Read XCR0
    or eax, 1 << 1       ; Enable XMM
    or eax, 1 << 2       ; Enable YMM
    xsetbv               ; Write XCR0
no_avx:
    cmp byte [has_xsave], 0
    je mem_512
    xor ecx, ecx        ; sub-leaf 0
    mov eax, 0xD
    cpuid               ; EBX = XSAVE memory size
    mov [fpu_memory_size], ebx
    ret
mem_512:
    mov dword [fpu_memory_size], 512
    ret

save_fpu:
    xor rax, rax
    mov al, [has_xsave]
    test al, al
    jz use_fxsave

    xor ecx, ecx        ; sub-leaf 0
    mov eax, 0xD
    cpuid               ; EAX = supported low 32, EDX = supported high 32
    xsave [rdi]
    ret
use_fxsave:
    fxsave [rdi]
    ret

restore_fpu:
    xor rax, rax
    mov al, [has_xsave]
    test al, al
    jz use_fxrstor

    xor ecx, ecx        ; sub-leaf 0
    mov eax, 0xD
    cpuid               ; EAX = supported low 32, EDX = supported high 32
    xrstor [rdi]
    ret
use_fxrstor:
    fxrstor [rdi]
    ret
