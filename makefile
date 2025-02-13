# CC = gcc
CC = /opt/homebrew/Cellar/i686-elf-gcc/14.2.0/bin/i686-elf-gcc
AS = /opt/homebrew/Cellar/i686-elf-binutils/2.43.1/bin/i686-elf-as
LD = /opt/homebrew/Cellar/i686-elf-binutils/2.43.1/bin/i686-elf-ld
QEMU = /opt/homebrew/bin/qemu-system-i386

SRC_ASM = bootsect.asm
OUT_BOOTSECT = bootsect.o
OUT_BIN = bootsect.bin

SRC_KERNEL = kernel.cpp
OUT_KERNEL = kernel.o
KERNEL_BIN = kernel.bin

build:
	$(AS) $(SRC_ASM) -o $(OUT_BOOTSECT)
	$(CC) -I/Library/Developer/CommandLineTools/SDKs/MacOSX14.4.sdk/usr/include -ffreestanding -m32 -o $(OUT_KERNEL) -c $(SRC_KERNEL) -v

link: 
	$(LD) -Ttext 0x7c00 --oformat binary -m elf_i386 $(OUT_BOOTSECT) -o $(OUT_BIN)
	$(LD) --oformat binary -Ttext 0x10000 -o $(KERNEL_BIN) --entry=kmain -m elf_i386 $(OUT_KERNEL)

all: build link

run:
	$(QEMU) -fda $(OUT_BIN) -fdb $(KERNEL_BIN)

