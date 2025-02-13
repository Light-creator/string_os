.code16

.section .data
  hash:  .zero 512

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
  
  # call print_start_msg
  call read_input

setup_16:
  movb $0x01, %dl
  movb $0x00, %dh
  movb $0x00, %ch
  movb $0x01, %cl
  movb $0x30, %al
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
	mov $0x07, %ah # scroll the screen function
	mov $0x00, %al # scroll the whole screen
	mov $0x07, %bh # colors
	mov $0x00, %ch
	mov $0x18, %dh # end row
	mov $0x4f, %dl

	int $0x10

  ret 

set_cursor:
  mov $0x02, %ah
  mov $0x00, %bh
  mov $0x00, %dh
  mov $0x00, %dl

  int $0x10

  ret

print_start_msg:
  movw $bios_start_msg, %bx
  call print_str
  ret

print_str:
  movb 0(%bx), %al
  test %al, %al

  jz end_print_str

  movb $0x0e, %ah
  int $0x10

  addw $1, %bx
  jmp print_str

end_print_str:
  ret

# b = 98 | m = 109 | s = 115 | t = 116 | d = 100
read_input:
  xorw %ax, %ax
  movb $0x00, %ah
  int $0x16

  movb $0x0e, %ah
  int $0x10
  
  movb $109, %bl
  cmpb %al, %bl
  jne second_cmp

  movw $0x100, %bx 
  movw $0, 0(%bx) 

  jmp end_read_input

  movb hash+98, %cl
  movb $1, %bl
  cmpb %cl, %bl
  jne read_input
  


second_cmp:
  movb $100, %bl
  cmpb %al, %bl
  jne set_hash

  movw $0x100, %bx 
  movw $1, 0(%bx)

  jmp end_read_input

  # movw $115, %si
  # call read_hash
  
  movb hash+115, %cl
  movb $1, %bl
  cmpb %cl, %bl
  jne read_input

  # movw $116, %si
  # call read_hash
  
  movb hash+116, %cl
  movb $1, %bl
  cmpb %cl, %bl
  jne read_input
    


set_hash:
  movb $0x0e, %ah
  int $0x10
  
  movsbw %al, %bx 
  movb $1, hash(%bx)

  jmp read_input

end_read_input:
  ret

insert_hash:
  leaw hash(%si), %bx
  movb $1, 0(%bx) 
  ret

read_hash:
  movb hash(%si), %al
  ret

  
gdt:
  .byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  .byte 0xff, 0xff, 0x00, 0x00, 0x00, 0x9A, 0xCF, 0x00
  .byte 0xff, 0xff, 0x00, 0x00, 0x00, 0x92, 0xCF, 0x00
gdt_info:
  .word gdt_info - gdt
  .word gdt, 0



bios_start_msg:
  .asciz "qweqwe..."


. = _start + 510
  .byte 0x55
  .byte 0xaa
