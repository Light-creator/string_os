.code16

  .section .text
  .global _start

# https://en.wikipedia.org/wiki/BIOS_interrupt_call (BIOS interrupts)
_start:
  cli
	xorw  %ax, %ax
	movw  %ax, %ss
	movw  %ax, %ds
	movw  %ax, %es
  
	movw  $_start, %sp
  sti

  call clear_screen
  call set_cursor
  
  call read_input

setup:
  # read the first sector of the disk
  movb $0x01, %dl # num drive 
  movb $0x00, %dh # num head
  movb $0x00, %ch # num track
  movb $0x01, %cl # num sector
  movb $0x30, %al # count sectors
  movw $0x1000, %bx
  movw %bx, %es
  xorw %bx, %bx
  movb $0x02, %ah
  int $0x13

  cli 
  lgdt gdt_info

  inb $0x92, %al
  orb $2, %al
  outb %al, $0x92
  movl %cr0, %eax
  
  orb $1, %al
  movl %eax, %cr0
  ljmp $0x8, $protected_mode 
  
.code32
protected_mode:
  movw $0x10, %ax
  movw %ax, %es
  movw %ax, %ds
  movw %ax, %ss
  call 0x10000

inf_loop:
  jmp inf_loop

clear_screen:
	movb $0x07, %ah # scroll the screen function
	movb $0x00, %al # scroll the whole screen
	movb $0x07, %bh # colors
	movb $0x00, %ch
	movb $0x18, %dh # end row
	movb $0x4f, %dl

	int $0x10

  ret 

set_cursor:
  mov $0x02, %ah
  mov $0x00, %bh
  mov $0x00, %dh
  mov $0x00, %dl

  int $0x10

  ret

out_char:
  movb $0x0e, %ah
  int $0x10
  ret

# b = 98 | m = 109 | s = 115 | t = 116 | d = 100
read_input:
  xorw %ax, %ax
  movb $0x00, %ah
  int $0x16

check_1:  
  # if compination == "b"
  movb $98, %bl
  cmpb %al, %bl
  jne cmp_std

  movb $0x00, %ah
  int $0x16

  # if combination == "bm"
  movb $109, %bl
  cmpb %al, %bl
  jne check_1
  
  # save flag 0 to 0x100
  movw $0x100, %bx 
  movw $0, 0(%bx) 

  jmp end_read_input

cmp_std:
  # if combination == "s"
  movb $115, %bl
  cmpb %al, %bl
  jne read_input

  movb $0x00, %ah
  int $0x16
  
  # if combination == "st"
  movb $116, %bl
  cmpb %al, %bl
  jne check_1

  movb $0x00, %ah
  int $0x16

  # if combination == "std"
  movb $100, %bl
  cmpb %al, %bl
  jne check_1
  
  # save flag 1 to 0x100
  movw $0x100, %bx 
  movw $1, 0(%bx)

  jmp end_read_input

end_read_input:
  ret

gdt:
  .byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  .byte 0xff, 0xff, 0x00, 0x00, 0x00, 0x9A, 0xCF, 0x00
  .byte 0xff, 0xff, 0x00, 0x00, 0x00, 0x92, 0xCF, 0x00

gdt_info:
  .word gdt_info - gdt
  .word gdt, 0

. = _start + 510
  .byte 0x55
  .byte 0xaa
