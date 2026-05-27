/**
 * UART RX interrupt test (interactive or injected).
 * Build: ./compile.sh c interrupt_uart_test
 * Run (keyboard):  ./build/test out/interrupt_uart_test 10000
 * Run (CI/inject): env SIM_INJECT_UART=U ./build/test out/interrupt_uart_test 500000 --debug=0
 *
 * Type any key after start; trap stores the byte in g_uart_rx_char. Passes if exactly
 * one external IRQ delivered a non-empty RX byte. SIM_INJECT_UART disables host stdin.
 */
#include <interrupt/common.h>

#define WAIT_LIMIT 500000u

int main(void) {
    uart_irq_enable();

    UART_PUTLIT("UART IRQ test: type one key (interactive) or use SIM_INJECT_UART\n");

    for (uint32_t i = 0; i < WAIT_LIMIT; ++i) {
        if (g_trap_count >= 1u) {
            break;
        }
        __asm__ volatile("wfi");
    }

    if (g_trap_count != 1u) {
        UART_PUTLIT("FAIL uart: no IRQ (type a key or set SIM_INJECT_UART=?)");
        UART_NL();
        sim_exit(1);
    }

    if (g_last_mcause != (MCAUSE_INTR_MASK | MCAUSE_EXTERNAL)) {
        sim_exit(2);
    }

    const uint32_t rx = g_uart_rx_char & 0xFFu;
    if (rx == 0u) {
        UART_PUTLIT("FAIL uart: IRQ but empty RXDATA");
        UART_NL();
        sim_exit(3);
    }

    UART_PUTLIT("PASS uart interrupt test, rx='");
    uart_putc_u32(rx);
    UART_PUTLIT("' hex=");
    uart_put_hex_byte(rx);
    UART_NL();
    sim_exit(0);
}
