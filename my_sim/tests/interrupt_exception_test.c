/**
 * Synchronous exception test: illegal instruction -> mtvec -> mret.
 * Build: ./compile.sh c interrupt_exception_test
 */
#include <interrupt/common.h>

static void trigger_illegal_instruction(void) {
    __asm__ volatile(".word 0xffffffff");
}

int main(void) {
    g_trap_count = 0;
    g_last_mcause = 0;
    write_mtvec((uint32_t)(uintptr_t)trap_vector);
    uart_enable_tx();

    trigger_illegal_instruction();

    if (g_trap_count != 1u) {
        UART_PUTLIT("FAIL exception: no trap taken");
        UART_NL();
        sim_exit(1);
    }

    if (g_last_mcause != MCAUSE_ILLEGAL) {
        sim_exit(2);
    }

    UART_PUTLIT("PASS exception trap test");
    UART_NL();
    sim_exit(0);
}
