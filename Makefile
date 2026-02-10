CC = gcc
AS = nasm
AR = ar
ASFLAGS =
override ASFLAGS += -f elf64
CFLAGS =
KERNEL_CFLAGS = -nostdlib -ffreestanding -fno-stack-protector -fno-stack-check -fno-PIC -ffunction-sections -fdata-sections -m64 -march=x86-64 -mno-80387 -mno-mmx -mno-sse -mno-sse2 -mno-red-zone -mcmodel=kernel
LIBC_CFLAGS = -nostdlib -ffreestanding -fno-stack-protector -fno-stack-check -fno-PIC -ffunction-sections -fdata-sections -m64 -march=x86-64 -mno-red-zone
BIN_CFLAGS = -nostdlib -ffreestanding -fno-stack-protector -fno-stack-check -fno-PIC -ffunction-sections -fdata-sections -m64 -march=x86-64 -mno-red-zone
LDFLAGS =
KERNEL_LDFLAGS = -nostdlib -lgcc -T kernel/linker.ld
ARFLAGS =

KERNEL_C_FILES = $(shell find kernel -name '*.c')
KERNEL_ASM_FILES = $(shell find kernel -name '*.asm')
KERNEL_SOURCE = $(KERNEL_C_FILES) $(KERNEL_ASM_FILES)

KERNEL_OBJECTS = $(patsubst kernel/%.c, build/obj/kernel/%.o, $(KERNEL_C_FILES)) \
                 $(patsubst kernel/%.asm, build/obj/kernel/%.o, $(KERNEL_ASM_FILES))

LIBC_SOURCE = $(shell find libc -name '*.c')
LIBC_OBJECTS = $(patsubst libc/%.c, build/obj/libc/%.o, $(LIBC_SOURCE))

BINARIES_SOURCE = $(wildcard binaries/*.c)
BINARIES_OBJECTS = $(patsubst binaries/%, build/obj/binaries/%,$(BINARIES:.c=.o))
BINARIES = $(basename $(notdir $(BINARIES_SOURCE)))
BINARY_TARGETS = $(patsubst %, build/binaries/%,$(BINARIES))

all: build/kernel build/libc.a binaries

build/kernel: $(KERNEL_OBJECTS)
	$(CC) $(LDFLAGS) $(KERNEL_LDFLAGS) -o $@ $^

build/obj/kernel/%.o: kernel/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(KERNEL_CFLAGS) -c $< -o $@

build/obj/kernel/%.o: kernel/%.asm
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

build/libc.a: $(LIBC_OBJECTS)
	$(AR) $(ARFLAGS) rcs $@ $^

build/obj/libc/%.o: libc/%.c | build/obj/libc
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(LIBC_CFLAGS) -c $< -o $@

build/obj/libc:
	mkdir -p build/obj/libc

binaries: $(BINARY_TARGETS)

build/crt0.o: libc/crt0.asm
	$(AS) $(ASFLAGS) $< -o $@

build/binaries/%: binaries/%.c build/crt0.o build/libc.a | build/binaries
	$(CC) $(CFLAGS) $(BIN_CFLAGS) -Ilibc/ $< build/crt0.o build/libc.a -o $@

build/binaries:
	mkdir -p build/binaries

.PHONY: clean
clean:
	rm -rf build
