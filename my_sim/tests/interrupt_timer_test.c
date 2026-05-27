/**
 * Timer interrupt test: MTIP -> mtvec handler -> mret -> main continues.
 * Build: ./compile.sh c interrupt_timer_test
 */
#include <interrupt/common.h>

#define EXPECTED_TRAPS 2u
#define WAIT_LIMIT     500000u

int main(void) {
    uart_enable_tx();
    timer_irq_enable(20ull);

    for (uint32_t i = 0; i < WAIT_LIMIT; ++i) {
        if (g_trap_count >= EXPECTED_TRAPS) {
            disable_global_interrupts();
            clear_mie_bits(MIE_MTIE);
            break;
        }
        __asm__ volatile("nop");
    }

    if (g_trap_count < EXPECTED_TRAPS) {
        UART_PUTLIT("FAIL timer: trap_count=");
        uart_put_u32_dec(g_trap_count);
        UART_NL();
        sim_exit(1);
    }

    if (g_last_mcause != (MCAUSE_INTR_MASK | MCAUSE_TIMER)) {
        UART_PUTLIT("FAIL timer: bad mcause=");
        uart_put_hex_u32(g_last_mcause);
        UART_NL();
        sim_exit(2);
    }

    UART_PUTLIT("PASS timer interrupt test\n");
    sim_exit(0);
}
