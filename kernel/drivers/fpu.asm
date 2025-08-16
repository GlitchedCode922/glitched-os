section .data
align 16
fpu_cw dw 0x037F       ; Default FPU control word

section .text
global init_fpu
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
    ; ECX bits: 
    ; 25 = SSE, 26 = SSE2, 28 = AVX
    test ecx, 1 << 25
    jz no_sse
    test ecx, 1 << 28
    jz no_avx

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
no_avx:
    test ecx, 1 << 28
    jz done_avx

    mov rax, cr4
    or rax, 1 << 18      ; OSXSAVE
    mov cr4, rax

    xor ecx, ecx
    xgetbv               ; Read XCR0
    or eax, 1 << 1       ; Enable XMM
    or eax, 1 << 2       ; Enable YMM
    xsetbv               ; Write XCR0

done_avx:
    ret
