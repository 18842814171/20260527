.section .trap.text, "ax", @progbits
.align 4
.globl trap_vector
trap_vector:
    csrr t0, mcause
    li t1, 0x80000007
    bne t0, t1, trap_fail

    la t2, g_trap_count
    lw t3, 0(t2)
    addi t3, t3, 1
    sw t3, 0(t2)

    la t2, g_last_mcause
    sw t0, 0(t2)

    /* Re-arm timer: mtimecmp = mtime_lo + 50 (clear high half first). */
    li t0, 0x02004000
    lw t1, 0(t0)
    li t2, 50
    add t1, t1, t2
    li t0, 0x0200400c
    sw zero, 0(t0)
    li t0, 0x02004008
    sw t1, 0(t0)

    mret

trap_fail:
    li a0, 90
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
