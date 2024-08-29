# bootblock.s

# .equ symbol, expression
# These directive set the value of the symbol to the expression
  .equ    BOOT_SEGMENT,      0x07c0
  .equ    DISPLAY_SEGMENT,   0xb800
  .equ    KERNEL_SEGMENT,    0x0000
  .equ    KERNEL_OFFSET,     0x1000

# You need to decide where to put the stack
 .equ    STACK_SEGMENT,     0x07e0 #(0x07e0 - 0x7fe0)
 .equ    STACK_POINTER,     0x7fe0

.text                   # Code segment
.globl    _start        # The entry point must be global
.code16                 # Real mode
.org 0x0

#
# The first instruction to execute in a program is called the entry
# point. The linker expects to find the entry point in the "symbol" _start
# (with underscore).
#

_start:
  jmp beyondReservedSpace

kernelSize:
  .word 0               # bootimage will write size of kernel, in sectors


beyondReservedSpace:
  # setup display segment
  movw $DISPLAY_SEGMENT, %bx
  movw %bx, %es
  
  # Clear screen
  movw $0x0a00, %ax     # Fill with black background / green foreground
  movw $2000, %cx       # Number of characters (80x25 screen in text mode = 2000)
  xorw %di, %di         # DI = 0
  rep stosw
  
  # movb $0x4b, %es:(0x0) # Write 'K' in the upper left corner of the screen


setup:
  # setup data segment
  movw  $BOOT_SEGMENT, %ax
  movw  %ax, %ds

  # setup stack
  movw  $STACK_SEGMENT, %ax
  movw  %ax, %ss
  movw  $STACK_POINTER, %sp


INT_13h:
  movb $0x02, %ah             # function number = 02h
  movb kernelSize, %al        # AL = number of sectors to read  
  movb $0x0, %ch              # CH = Cylinder, hardcoced 0 (since we use SSD)
  movb $0x2, %cl              # CL = Sector, hardcoded 2 (since kernel start at sector 2)
  movb $0x0, %dh              # DH = Head, hardcoded 0 (seince we use SSD)
  // movb $0x0, %dl           # DL = Drive, No need to specify drive since BIOS does this job
  # Important to save DL for future?

  # ES:BS
  movw $KERNEL_SEGMENT, %bx
  movw %bx, %es               # ES should be 0
  movw $KERNEL_OFFSET, %bx
  
  int $0x13                   # interrupt call, INT 13h

  jc error                    # INT 13h raises flag if something is wrong

  # setup Kernel data segment
  movw  $KERNEL_SEGMENT, %ax
  movw  %ax, %ds

  ljmp $KERNEL_SEGMENT, $KERNEL_OFFSET # jmp to kernel, and setup Kernel code segment %cs


forever:
  jmp forever

error:
  movb $0x50, %es:((20-1)*80+(30-1))*2 # Character that indicates that something is wrong
  jmp forever