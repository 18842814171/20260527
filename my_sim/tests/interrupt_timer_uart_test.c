/**
 * Timer + UART interrupt correlation test (single mtvec, dual dispatch).
 *
 * Phase A — timer only (MEIE off): 2x MTIP traps.
 * Phase B — MEIE first (injected RX), then MTIP: irq_log[0]=EXTERNAL, [1]=TIMER.
 *
 * Build: ./compile.sh c interrupt_timer_uart_test
 * Run:   env SIM_INJECT_UART=T ./build/test interrupt_timer_uart_test 500000
 *        (without SIM_INJECT_UART the UART phase fails with exit_code=3/4)
 */
#include <interrupt/common.h>

#define EXPECTED_TIMER_TRAPS 2u
#define SPIN_LIMIT           50000u
#define IRQ_LOG_CAP          8u

extern volatile uint32_t g_timer_irq_count;
extern volatile uint32_t g_uart_irq_count;
extern volatile uint32_t g_irq_log_idx;
extern volatile uint32_t g_irq_log[IRQ_LOG_CAP];

static void irq_log_reset(void) {
    g_irq_log_idx = 0;
    for (uint32_t i = 0; i < IRQ_LOG_CAP; ++i) {
        g_irq_log[i] = 0;
    }
}

static void trap_stats_reset(void) {
    g_trap_count = 0;
    g_timer_irq_count = 0;
    g_uart_irq_count = 0;
    g_last_mcause = 0;
    g_uart_rx_char = 0;
    irq_log_reset();
}

static void irq_delivery_off(void) {
    disable_global_interrupts();
    clear_mie_bits(MIE_MTIE | MIE_MEIE);
}

static int wait_timer_traps(uint32_t want) {
    for (uint32_t i = 0; i < SPIN_LIMIT; ++i) {
        if (g_timer_irq_count >= want) {
            return 0;
        }
        __asm__ volatile("nop");
    }
    return -1;
}

static int wait_uart_traps(uint32_t want) {
    for (uint32_t i = 0; i < SPIN_LIMIT; ++i) {
        if (g_uart_irq_count >= want) {
            return 0;
        }
        __asm__ volatile("nop");
    }
    return -1;
}

int main(void) {
    uart_enable_tx();
    uart_trap_stack_init(); /* trap_timer_uart.S uses mscratch before Phase A timer IRQs */
    UART_PUTLIT("timer+uart IRQ test: need SIM_INJECT_UART=T for phase B\n");
    trap_stats_reset();

    /* --- Phase A: timer only --- */
    uart_disable_irq();
    TEST_UART_RXCTRL = 0;
    uart_clear_rx_pending();
    timer_irq_enable(20ull);

    if (wait_timer_traps(EXPECTED_TIMER_TRAPS) != 0) {
        sim_exit(1);
    }
    irq_delivery_off();

    if (g_last_mcause != (MCAUSE_INTR_MASK | MCAUSE_TIMER)) {
        sim_exit(2);
    }
    if (g_uart_irq_count != 0u) {
        sim_exit(12);
    }

    /* --- Phase B: UART then timer (preserves SIM_INJECT byte for MEIE) --- */
    trap_stats_reset();
    write_mtvec((uint32_t)(uintptr_t)trap_vector);
    uart_enable_tx();
    uart_enable_rx();
    TEST_UART_IE = TEST_UART_IE_RX;
    write_mie(MIE_MEIE);
    enable_global_interrupts();

    if (wait_uart_traps(1u) != 0) {
        irq_delivery_off();
        sim_exit(3);
    }

    set_mie_bits(MIE_MTIE);
    timer_arm_relative(30u);

    const uint32_t timers_before = g_timer_irq_count;
    if (wait_timer_traps(timers_before + 1u) != 0) {
        irq_delivery_off();
        sim_exit(4);
    }
    irq_delivery_off();

    if (g_irq_log_idx < 2u) {
        sim_exit(5);
    }
    if (g_irq_log[0] != (MCAUSE_INTR_MASK | MCAUSE_EXTERNAL)) {
        sim_exit(6);
    }
    if (g_irq_log[1] != (MCAUSE_INTR_MASK | MCAUSE_TIMER)) {
        sim_exit(7);
    }

    const uint32_t rx = g_uart_rx_char & 0xFFu;
    if (rx == 0u) {
        sim_exit(8);
    }

    UART_PUTLIT("PASS timer+uart: timer=");
    uart_put_u32_dec(g_timer_irq_count);
    UART_PUTLIT(" uart=");
    uart_put_u32_dec(g_uart_irq_count);
    UART_PUTLIT(" rx=");
    uart_putc_u32(rx);
    UART_PUTLIT(" log0=");
    uart_put_hex_u32(g_irq_log[0]);
    UART_PUTLIT(" log1=");
    uart_put_hex_u32(g_irq_log[1]);
    UART_NL();
    sim_exit(0);
}
