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

all: build/kernel build/libc.a binaries

build/kernel: build/kernelobj/main.o build/kernelobj/isr_handlers.o build/kernelobj/idt.o build/kernelobj/console.o build/kernelobj/8259pic.o build/kernelobj/kbd.o build/kernelobj/ps2_keyboard.o build/kernelobj/timer.o build/kernelobj/mman.o build/kernelobj/paging.o build/kernelobj/ata.o build/kernelobj/block.o build/kernelobj/mbr.o build/kernelobj/fat32.o build/kernelobj/mount.o build/kernelobj/syscalls.o build/kernelobj/elf.o build/kernelobj/exec.o build/kernelobj/power.o build/kernelobj/fpu.o build/kernelobj/pci.o build/kernelobj/rtl8139.o build/kernelobj/ethernet.o build/kernelobj/arp.o build/kernelobj/ip.o build/kernelobj/icmp.o build/kernelobj/udp.o build/kernelobj/dhcp.o build/kernelobj/net.o
	$(CC) $(LDFLAGS) $(KERNEL_LDFLAGS) -o $@ $^

build/kernelobj/main.o: kernel/main.c kernel/panic.h | build/kernelobj
	$(CC) $(CFLAGS) $(KERNEL_CFLAGS) -c $< -o $@

build/kernelobj/isr_handlers.o: kernel/isr_handlers.asm | build/kernelobj
	$(AS) $(ASFLAGS) $< -o $@

build/kernelobj/idt.o: kernel/idt.c kernel/panic.h | build/kernelobj
	$(CC) $(CFLAGS) $(KERNEL_CFLAGS) -c $< -o $@

build/kernelobj/console.o: kernel/console.c | build/kernelobj
	$(CC) $(CFLAGS) $(KERNEL_CFLAGS) -c $< -o $@

build/kernelobj/8259pic.o: kernel/io/8259pic.c kernel/io/ports.h | build/kernelobj
	$(CC) $(CFLAGS) $(KERNEL_CFLAGS) -c $< -o $@

build/kernelobj/kbd.o: kernel/drivers/kbd.c | build/kernelobj
	$(CC) $(CFLAGS) $(KERNEL_CFLAGS) -c $< -o $@

build/kernelobj/ps2_keyboard.o: kernel/drivers/ps2_keyboard.c | build/kernelobj
	$(CC) $(CFLAGS) $(KERNEL_CFLAGS) -c $< -o $@

build/kernelobj/timer.o: kernel/drivers/timer.c | build/kernelobj
	$(CC) $(CFLAGS) $(KERNEL_CFLAGS) -c $< -o $@

build/kernelobj/mman.o: kernel/memory/mman.c | build/kernelobj
	$(CC) $(CFLAGS) $(KERNEL_CFLAGS) -c $< -o $@

build/kernelobj/paging.o: kernel/memory/paging.c | build/kernelobj
	$(CC) $(CFLAGS) $(KERNEL_CFLAGS) -c $< -o $@

build/kernelobj/ata.o: kernel/drivers/block/ata.c | build/kernelobj
	$(CC) $(CFLAGS) $(KERNEL_CFLAGS) -c $< -o $@

build/kernelobj/block.o: kernel/drivers/block.c | build/kernelobj
	$(CC) $(CFLAGS) $(KERNEL_CFLAGS) -c $< -o $@

build/kernelobj/mbr.o: kernel/drivers/partitions/mbr.c | build/kernelobj
	$(CC) $(CFLAGS) $(KERNEL_CFLAGS) -c $< -o $@

build/kernelobj/fat32.o: kernel/fs/fat32.c | build/kernelobj
	$(CC) $(CFLAGS) $(KERNEL_CFLAGS) -c $< -o $@

build/kernelobj/mount.o: kernel/mount.c | build/kernelobj
	$(CC) $(CFLAGS) $(KERNEL_CFLAGS) -c $< -o $@

build/kernelobj/syscalls.o: kernel/usermode/syscalls.c | build/kernelobj
	$(CC) $(CFLAGS) $(KERNEL_CFLAGS) -c $< -o $@

build/kernelobj/elf.o: kernel/usermode/elf.c | build/kernelobj
	$(CC) $(CFLAGS) $(KERNEL_CFLAGS) -c $< -o $@

build/kernelobj/exec.o: kernel/usermode/exec.c | build/kernelobj
	$(CC) $(CFLAGS) $(KERNEL_CFLAGS) -c $< -o $@

build/kernelobj/power.o: kernel/power.c | build/kernelobj
	$(CC) $(CFLAGS) $(KERNEL_CFLAGS) -c $< -o $@

build/kernelobj/fpu.o: kernel/drivers/fpu.asm | build/kernelobj
	$(AS) $(ASFLAGS) $< -o $@

build/kernelobj/pci.o: kernel/io/pci.c | build/kernelobj
	$(CC) $(CFLAGS) $(KERNEL_CFLAGS) -c $< -o $@

build/kernelobj/rtl8139.o: kernel/drivers/net/rtl8139.c | build/kernelobj
	$(CC) $(CFLAGS) $(KERNEL_CFLAGS) -c $< -o $@

build/kernelobj/ethernet.o: kernel/net/ethernet.c | build/kernelobj
	$(CC) $(CFLAGS) $(KERNEL_CFLAGS) -c $< -o $@

build/kernelobj/arp.o: kernel/net/arp.c | build/kernelobj
	$(CC) $(CFLAGS) $(KERNEL_CFLAGS) -c $< -o $@

build/kernelobj/ip.o: kernel/net/ip.c | build/kernelobj
	$(CC) $(CFLAGS) $(KERNEL_CFLAGS) -c $< -o $@

build/kernelobj/icmp.o: kernel/net/icmp.c | build/kernelobj
	$(CC) $(CFLAGS) $(KERNEL_CFLAGS) -c $< -o $@

build/kernelobj/udp.o: kernel/net/udp.c | build/kernelobj
	$(CC) $(CFLAGS) $(KERNEL_CFLAGS) -c $< -o $@

build/kernelobj/dhcp.o: kernel/net/dhcp.c | build/kernelobj
	$(CC) $(CFLAGS) $(KERNEL_CFLAGS) -c $< -o $@

build/kernelobj/net.o: kernel/drivers/net.c | build/kernelobj
	$(CC) $(CFLAGS) $(KERNEL_CFLAGS) -c $< -o $@

build/kernelobj: | build
	mkdir -p build/kernelobj

build/libc.a: build/libcobj/console_io.o build/libcobj/file_io.o build/libcobj/syscall.o build/libcobj/string.o build/libcobj/exec.o build/libcobj/math.o
	$(AR) $(ARFLAGS) rcs $@ $^

build/libcobj/console_io.o: libc/console_io.c | build/libcobj
	$(CC) $(CFLAGS) $(LIBC_CFLAGS) -c $< -o $@

build/libcobj/file_io.o: libc/file_io.c | build/libcobj
	$(CC) $(CFLAGS) $(LIBC_CFLAGS) -c $< -o $@

build/libcobj/syscall.o: libc/syscall.c | build/libcobj
	$(CC) $(CFLAGS) $(LIBC_CFLAGS) -c $< -o $@

build/libcobj/string.o: libc/string.c | build/libcobj
	$(CC) $(CFLAGS) $(LIBC_CFLAGS) -c $< -o $@

build/libcobj/exec.o: libc/exec.c | build/libcobj
	$(CC) $(CFLAGS) $(LIBC_CFLAGS) -c $< -o $@

build/libcobj/math.o: libc/math.c | build/libcobj
	$(CC) $(CFLAGS) $(LIBC_CFLAGS) -c $< -o $@

build/libcobj: | build
	mkdir -p build/libcobj

binaries: build/sh build/ls build/mkdir build/touch build/rm build/pwd build/cat build/echo build/reboot build/cp build/mv build/fwrite

build/sh: binaries/sh.c build/libc.a
	$(CC) $(CFLAGS) $(BIN_CFLAGS) -Ilibc/ $< build/libc.a -o $@

build/ls: binaries/ls.c build/libc.a
	$(CC) $(CFLAGS) $(BIN_CFLAGS) -Ilibc/ $< build/libc.a -o $@

build/mkdir: binaries/mkdir.c build/libc.a
	$(CC) $(CFLAGS) $(BIN_CFLAGS) -Ilibc/ $< build/libc.a -o $@

build/touch: binaries/touch.c build/libc.a
	$(CC) $(CFLAGS) $(BIN_CFLAGS) -Ilibc/ $< build/libc.a -o $@

build/rm: binaries/rm.c build/libc.a
	$(CC) $(CFLAGS) $(BIN_CFLAGS) -Ilibc/ $< build/libc.a -o $@

build/pwd: binaries/pwd.c build/libc.a
	$(CC) $(CFLAGS) $(BIN_CFLAGS) -Ilibc/ $< build/libc.a -o $@

build/cat: binaries/cat.c build/libc.a
	$(CC) $(CFLAGS) $(BIN_CFLAGS) -Ilibc/ $< build/libc.a -o $@

build/echo: binaries/echo.c build/libc.a
	$(CC) $(CFLAGS) $(BIN_CFLAGS) -Ilibc/ $< build/libc.a -o $@

build/reboot: binaries/reboot.c build/libc.a
	$(CC) $(CFLAGS) $(BIN_CFLAGS) -Ilibc/ $< build/libc.a -o $@

build/cp: binaries/cp.c build/libc.a
	$(CC) $(CFLAGS) $(BIN_CFLAGS) -Ilibc/ $< build/libc.a -o $@

build/mv: binaries/mv.c build/libc.a
	$(CC) $(CFLAGS) $(BIN_CFLAGS) -Ilibc/ $< build/libc.a -o $@

build/fwrite: binaries/fwrite.c build/libc.a
	$(CC) $(CFLAGS) $(BIN_CFLAGS) -Ilibc/ $< build/libc.a -o $@

build:
	mkdir -p build

.PHONY: clean
clean:
	rm -rf build
