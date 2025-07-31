CC = gcc
AS = nasm
ASFLAGS =
override ASFLAGS += -f elf64
CFLAGS =
override CFLAGS += -nostdlib -ffreestanding -fno-stack-protector -fno-stack-check -fno-PIC -ffunction-sections -fdata-sections -m64 -march=x86-64 -mno-80387 -mno-mmx -mno-sse -mno-sse2 -mno-red-zone -mcmodel=kernel
LDFLAGS =
override LDFLAGS += -nostdlib -ffreestanding -fno-stack-protector -fno-stack-check -fno-PIC -ffunction-sections -fdata-sections -m64 -march=x86-64 -mno-80387 -mno-mmx -mno-sse -mno-sse2 -mno-red-zone -mcmodel=kernel -lgcc -T kernel/linker.ld

all: build/kernel

build/kernel: build/main.o build/isr_handlers.o build/idt.o build/console.o build/8259pic.o build/kbd.o build/ps2_keyboard.o build/timer.o build/mman.o build/paging.o build/ata.o build/block.o
	$(CC) $(LDFLAGS) -o $@ $^

build/main.o: kernel/main.c kernel/panic.h | build
	$(CC) $(CFLAGS) -c $< -o $@

build/isr_handlers.o: kernel/isr_handlers.asm | build
	$(AS) $(ASFLAGS) $< -o $@

build/idt.o: kernel/idt.c kernel/panic.h | build
	$(CC) $(CFLAGS) -c $< -o $@

build/console.o: kernel/console.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build/8259pic.o: kernel/io/8259pic.c kernel/io/ports.h | build
	$(CC) $(CFLAGS) -c $< -o $@

build/kbd.o: kernel/drivers/kbd.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build/ps2_keyboard.o: kernel/drivers/ps2_keyboard.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build/timer.o: kernel/drivers/timer.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build/mman.o: kernel/memory/mman.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build/paging.o: kernel/memory/paging.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build/ata.o: kernel/drivers/block/ata.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build/block.o: kernel/drivers/block.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build:
	mkdir -p build

.PHONY: clean
clean:
	rm -rf build
