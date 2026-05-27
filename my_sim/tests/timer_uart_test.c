/**
 * MMIO polling test (no IRQ): periodic MTIME read + UART TX only.
 * Build: ./compile.sh c timer_uart_test
 * For timer IRQ use interrupt_timer_test.
 */

#include <stdint.h>
#include <interrupt/common.h>

#define TIMER_MTIME_LO (*(volatile uint32_t*)(TIMER_BASE + 0x00u))

#define PRINT_ROUNDS 4
#define WAIT_TICKS_PER_ROUND 80u

static void uart_put_hex32(uint32_t v) {
    static const char hex[] = "0123456789ABCDEF";
    UART_PUTLIT("0x");
    for (int i = 28; i >= 0; i -= 4) {
        uart_putc_u32((uint8_t)hex[(v >> i) & 0xFu]);
    }
}

static void wait_ticks(uint32_t ticks) {
    for (uint32_t i = 0; i < ticks; ++i) {
        (void)TIMER_MTIME_LO;
    }
}

int main(void) {
    uart_enable_tx();

    UART_PUTLIT("=== Timer + UART MMIO poll test (no IRQ) ===");
    UART_NL();
    UART_PUTLIT("No input required; print MTIME periodically.");
    UART_NL();

    for (uint32_t i = 0; i < PRINT_ROUNDS; ++i) {
        uint32_t t = TIMER_MTIME_LO;
        UART_PUTLIT("[tick] MTIME=");
        uart_put_hex32(t);
        UART_NL();
        wait_ticks(WAIT_TICKS_PER_ROUND);
    }

    UART_PUTLIT("Done.");
    UART_NL();
    sim_exit(0);
}
