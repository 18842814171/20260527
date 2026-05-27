.section .bss
.align 16
.globl trap_cpu_stack_top
trap_cpu_stack:
    .space 1024
trap_cpu_stack_top:

.section .trap.text, "ax", @progbits
.align 4
.globl trap_vector
trap_vector:
    csrrw sp, mscratch, sp
    addi sp, sp, -128
    sw ra, 0(sp)
    sw t0, 4(sp)
    sw t1, 8(sp)
    sw t2, 12(sp)
    sw t3, 16(sp)
    sw t4, 20(sp)
    sw t5, 24(sp)
    sw t6, 28(sp)
    sw a0, 32(sp)
    sw a1, 36(sp)
    sw a2, 40(sp)
    sw a3, 44(sp)
    sw a4, 48(sp)
    sw a5, 52(sp)
    sw a6, 56(sp)
    sw a7, 60(sp)

    csrr t0, mcause

    /* Record trap order (mcause) for host/guest correlation — max 8 entries. */
    la t2, g_irq_log_idx
    lw t3, 0(t2)
    li t4, 8
    bgeu t3, t4, 1f
    la t5, g_irq_log
    slli t6, t3, 2
    add t5, t5, t6
    sw t0, 0(t5)
    addi t3, t3, 1
    sw t3, 0(t2)
1:
    la t2, g_last_mcause
    sw t0, 0(t2)

    li t1, 0x80000007
    beq t0, t1, handle_timer
    li t1, 0x8000000b
    beq t0, t1, handle_uart
    /* Sync illegal insn: skip faulting instruction (same as trap_exception.S). */
    li t1, 2
    beq t0, t1, handle_sync_illegal
    j trap_fail

handle_sync_illegal:
    csrr t0, mepc
    addi t0, t0, 4
    csrw mepc, t0
    j trap_restore

handle_timer:
    la t2, g_timer_irq_count
    lw t3, 0(t2)
    addi t3, t3, 1
    sw t3, 0(t2)
    la t2, g_trap_count
    lw t3, 0(t2)
    addi t3, t3, 1
    sw t3, 0(t2)

    li t0, 0x02004000
    lw t1, 0(t0)
    li t2, 50
    add t1, t1, t2
    li t0, 0x0200400c
    sw zero, 0(t0)
    li t0, 0x02004008
    sw t1, 0(t0)
    j trap_restore

handle_uart:
    la t2, g_uart_irq_count
    lw t3, 0(t2)
    addi t3, t3, 1
    sw t3, 0(t2)
    la t2, g_trap_count
    lw t3, 0(t2)
    addi t3, t3, 1
    sw t3, 0(t2)

trap_rx_loop:
    li t0, 0x10000004
    lw t0, 0(t0)
    li t1, 0x80000000
    and t2, t0, t1
    bnez t2, trap_restore
    andi t0, t0, 0xff
    beqz t0, trap_rx_loop

    la t2, g_uart_rx_char
    sw t0, 0(t2)

    la t2, g_uart_rx_head
    lw t3, 0(t2)
    la t4, g_uart_rx_tail
    lw t5, 0(t4)
    addi t6, t3, 1
    andi t6, t6, 0x3f
    beq t6, t5, trap_rx_drop
    la t6, g_uart_rx_ring
    andi t3, t3, 0x3f
    add t6, t6, t3
    sb t0, 0(t6)
    lw t3, 0(t2)
    addi t3, t3, 1
    sw t3, 0(t2)
    j trap_rx_loop

trap_rx_drop:
    addi t5, t5, 1
    andi t5, t5, 0x3f
    sw t5, 0(t4)
    la t6, g_uart_rx_ring
    lw t3, 0(t2)
    andi t3, t3, 0x3f
    add t6, t6, t3
    sb t0, 0(t6)
    lw t3, 0(t2)
    addi t3, t3, 1
    sw t3, 0(t2)
    j trap_rx_loop

trap_restore:
    lw ra, 0(sp)
    lw t0, 4(sp)
    lw t1, 8(sp)
    lw t2, 12(sp)
    lw t3, 16(sp)
    lw t4, 20(sp)
    lw t5, 24(sp)
    lw t6, 28(sp)
    lw a0, 32(sp)
    lw a1, 36(sp)
    lw a2, 40(sp)
    lw a3, 44(sp)
    lw a4, 48(sp)
    lw a5, 52(sp)
    lw a6, 56(sp)
    lw a7, 60(sp)
    addi sp, sp, 128
    csrrw sp, mscratch, sp
    mret

trap_fail:
    li a0, 93
    li a7, 93
    ecall
1:  j 1b

.section .sbss
.align 4
.globl g_trap_count
.globl g_last_mcause
.globl g_timer_irq_count
.globl g_uart_irq_count
.globl g_irq_log_idx
.globl g_irq_log
.globl g_uart_rx_char
.globl g_uart_rx_head
.globl g_uart_rx_tail
.globl g_uart_rx_ring
g_trap_count:
    .word 0
g_last_mcause:
    .word 0
g_timer_irq_count:
    .word 0
g_uart_irq_count:
    .word 0
g_irq_log_idx:
    .word 0
g_irq_log:
    .space 32
g_uart_rx_char:
    .word 0
g_uart_rx_head:
    .word 0
g_uart_rx_tail:
    .word 0
g_uart_rx_ring:
    .space 64
