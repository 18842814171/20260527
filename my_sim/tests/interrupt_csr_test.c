/**
 * CSR delivery test: timer IRQ fires, csrc mie.MTIE blocks further traps, csrs restores.
 * Build: ./compile.sh c interrupt_csr_test
 */
#include <interrupt/common.h>

#define SPIN_WAIT_TRAP  50000u
#define SPIN_NO_TRAP    2000u

static uint32_t sltu_insn(uint32_t a, uint32_t b) {
    uint32_t r = 0;
    __asm__ volatile("sltu %0, %1, %2" : "=r"(r) : "r"(a), "r"(b));
    return r;
}

static int verify_sltu(void) {
    if (sltu_insn(1u, 2u) != 1u) {
        return 20;
    }
    if (sltu_insn(2u, 1u) != 0u) {
        return 21;
    }
    if (sltu_insn(0u, 0xFFFFFFFFu) != 1u) {
        return 22;
    }
    return 0;
}

int main(void) {
    const int sltu_err = verify_sltu();
    if (sltu_err != 0) {
        UART_PUTLIT("FAIL csr: SLTU insn check");
        UART_NL();
        sim_exit(sltu_err);
    }
    /* Avoid EXTERNAL (UART) beating TIMER in IRQ priority during this test. */
    uart_disable_irq();
    TEST_UART_RXCTRL = 0;
    uart_clear_rx_pending();
    uart_enable_tx();
    timer_irq_enable(20ull);

    uint32_t base_count = 0;
    for (uint32_t i = 0; i < SPIN_WAIT_TRAP; ++i) {
        if (g_trap_count >= 1u) {
            base_count = g_trap_count;
            clear_mie_bits(MIE_MTIE);
            disable_global_interrupts();
            break;
        }
        __asm__ volatile("nop");
    }
    if (base_count < 1u) {
        UART_PUTLIT("FAIL csr: no first trap");
        UART_NL();
        sim_exit(1);
    }

    timer_arm_relative(80u); /* common.h */
    const uint32_t frozen = g_trap_count;
    for (uint32_t i = 0; i < SPIN_NO_TRAP; ++i) {
        if (g_trap_count != frozen) {
            UART_PUTLIT("FAIL csr: trap while mie/mstatus masked");
            UART_NL();
            sim_exit(2);
        }
        /* nop only: WFI would spin in host while MIE is off but MTIP may be set */
        __asm__ volatile("nop");
    }

    enable_global_interrupts();
    set_mie_bits(MIE_MTIE);
    timer_arm_relative(40u);

    for (uint32_t i = 0; i < SPIN_WAIT_TRAP; ++i) {
        if (g_trap_count > frozen) {
            UART_PUTLIT("PASS csr interrupt delivery test\n");
            sim_exit(0);
        }
        __asm__ volatile("nop");
    }

    UART_PUTLIT("FAIL csr: no trap after mie/mstatus restore");
    UART_NL();
    sim_exit(3);
}
