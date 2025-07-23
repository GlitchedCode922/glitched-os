CC = gcc
AS = nasm
ASFLAGS =
ASFLAGS += -f elf64
CFLAGS =
CFLAGS += -nostdlib -ffreestanding -fno-stack-protector -fno-stack-check -fno-PIC -ffunction-sections -fdata-sections -m64 -march=x86-64 -mno-80387 -mno-mmx -mno-sse -mno-sse2 -mno-red-zone -mcmodel=kernel
LDFLAGS =
LDFLAGS += -nostdlib -ffreestanding -fno-stack-protector -fno-stack-check -fno-PIC -ffunction-sections -fdata-sections -m64 -march=x86-64 -mno-80387 -mno-mmx -mno-sse -mno-sse2 -mno-red-zone -mcmodel=kernel -lgcc -T kernel/linker.ld

all: build/kernel

build/kernel: build/main.o build/isr_handlers.o build/idt.o build/console.o
	$(CC) $(LDFLAGS) -o $@ $^

build/main.o: kernel/main.c kernel/panic.h | build
	$(CC) $(CFLAGS) -c $< -o $@

build/isr_handlers.o: kernel/isr_handlers.asm | build
	$(AS) $(ASFLAGS) $< -o $@

build/idt.o: kernel/idt.c kernel/panic.h | build
	$(CC) $(CFLAGS) -c $< -o $@

build/console.o: kernel/console.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build:
	mkdir -p build

.PHONY: clean
clean:
	rm -rf build
