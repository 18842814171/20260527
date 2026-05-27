.section .trap.text, "ax", @progbits
.align 4
.globl trap_vector
trap_vector:
    csrr t0, mcause
    la t2, g_last_mcause
    sw t0, 0(t2)

    li t1, 2
    bne t0, t1, trap_fail

    la t2, g_trap_count
    lw t3, 0(t2)
    addi t3, t3, 1
    sw t3, 0(t2)

    csrr t0, mepc
    addi t0, t0, 4
    csrw mepc, t0

    mret

trap_fail:
    li a0, 92
    li a7, 93
    ecall
1:  j 1b

.section .sbss
.align 4
.globl g_trap_count
.globl g_last_mcause
g_trap_count:
    .word 0
g_last_mcause:
    .word 0
