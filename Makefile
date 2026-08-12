TOOLCHAIN = llvm
TARGET = x86_64-elf
HOSTCC = cc
AS = nasm
ASFLAGS =
override ASFLAGS += -f elf64
CFLAGS =
override CFLAGS += -nostdlib -ffreestanding -fno-stack-protector -fno-stack-check -fno-pic -fno-omit-frame-pointer -ffunction-sections -fdata-sections -m64 -march=x86-64 -MMD -MP
KERNEL_CFLAGS =
LIBC_CFLAGS =
BIN_CFLAGS =
override KERNEL_CFLAGS += -mno-80387 -mno-mmx -mno-sse -mno-sse2 -mno-red-zone -mcmodel=kernel -Iglitchfs/libglfs/include
LDFLAGS =
KERNEL_LDFLAGS =
BIN_LDFLAGS =
override KERNEL_LDFLAGS += -T kernel/linker.ld
LDLIBS =
ARFLAGS =

ifeq ($(TOOLCHAIN), llvm)
	CC = clang
	LD = ld.lld
	AR = llvm-ar
	override CFLAGS += --target=$(TARGET)
else ifeq ($(TOOLCHAIN), gcc)
	CC = $(TARGET)-gcc
	LD = $(TARGET)-ld
	AR = $(TARGET)-ar
else ifeq ($(TOOLCHAIN), custom)
else
	$(error Invalid TOOLCHAIN specified: $(TOOLCHAIN))
endif

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

.DELETE_ON_ERROR:
.SECONDARY:
.PHONY: all clean kernel libc binaries disk-image glitchfs

all: kernel libc binaries disk-image

-include $(KERNEL_OBJECTS:.o=.d)
-include $(LIBC_OBJECTS:.o=.d)
-include $(BINARIES_OBJECTS:.o=.d)

kernel: build/kernel

build/kernel: $(KERNEL_OBJECTS) glitchfs/build/target-libglfs.a
	$(LD) $(LDFLAGS) $(KERNEL_LDFLAGS) $^ $(LDLIBS) -o $@

glitchfs/build/target-libglfs.a: glitchfs
glitchfs:
	$(MAKE) -C glitchfs HOSTCC="$(HOSTCC)" CC="$(CC)" AR="$(AR)" CFLAGS="$(CFLAGS) $(KERNEL_CFLAGS)" libglfs-target tools

build/obj/kernel/%.o: kernel/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(KERNEL_CFLAGS) -c $< -o $@

build/obj/kernel/%.o: kernel/%.asm
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

libc: build/libc.a build/crt0.o

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

build/binaries/%: build/obj/binaries/%.o build/crt0.o build/libc.a | build/binaries
	$(LD) $(LDFLAGS) $(BIN_LDFLAGS) build/crt0.o $< build/libc.a $(LDLIBS) -o $@

build/obj/binaries/%.o: binaries/%.c | build/obj/binaries
	$(CC) $(CFLAGS) $(BIN_CFLAGS) -c -Ilibc/ $< -o $@

build/obj/binaries:
	mkdir -p build/obj/binaries

build/binaries:
	mkdir -p build/binaries

disk-image: build/disk.img

build/disk.img: build/kernel $(BINARY_TARGETS) limine.conf glitchfs
	$(MAKE) -C thirdparty/limine CC=$(HOSTCC)
	dd if=/dev/zero of=build/disk.img.incomplete bs=1M count=128
	dd if=/dev/zero of=build/root.img.tmp bs=1M count=111

	parted build/disk.img.incomplete --script -- \
	mklabel gpt \
	mkpart primary 1MiB 4MiB \
	mkpart ESP fat32 4MiB 16MiB \
	mkpart primary 16MiB -1MiB \
	set 1 bios_grub on \
	set 2 esp on

	mformat -i build/disk.img.incomplete@@4194304 -F ::

	mmd -i build/disk.img.incomplete@@4194304 ::/EFI ::/EFI/BOOT
	mmd -i build/disk.img.incomplete@@4194304 ::/limine

	mcopy -i build/disk.img.incomplete@@4194304 thirdparty/limine/BOOTX64.EFI ::/EFI/BOOT/BOOTX64.EFI
	mcopy -i build/disk.img.incomplete@@4194304 thirdparty/limine/limine-bios.sys ::/limine/
	mcopy -i build/disk.img.incomplete@@4194304 limine.conf ::/limine
	mcopy -i build/disk.img.incomplete@@4194304 build/kernel ::/kernel
	mcopy -i build/disk.img.incomplete@@4194304 thirdparty/limine/LICENSE ::/limine

	mkdir -p build/rootfs/bin
	mkdir -p build/rootfs/boot
	mkdir -p build/rootfs/dev
	mkdir -p build/rootfs/tmp

	cp build/binaries/* build/rootfs/bin
	glitchfs/build/tools/glfs-pack build/rootfs build/root.img.tmp
	dd if=build/root.img.tmp of=build/disk.img.incomplete bs=1M seek=16 count=111 conv=notrunc
	rm -f build/root.img.tmp

	thirdparty/limine/limine bios-install build/disk.img.incomplete 1

	mv build/disk.img.incomplete build/disk.img

clean:
	rm -rf build

run: disk-image
	qemu-system-x86_64 -m 4G -monitor stdio -drive file=build/disk.img,format=raw $(QEMUFLAGS)
