.section .text.init
.globl _start
_start:
    la sp, __stack_top
    call main
    li a0, 0
    li a7, 93
    ecall
1:  j 1b

.section .bss
.align 4
__stack:
    .space 4096
__stack_top:
