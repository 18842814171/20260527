#ifndef INTERRUPT_COMMON_H
#define INTERRUPT_COMMON_H

#include <stdint.h>
#include <interrupt/mmio_uart.h>

#define MSTATUS_MIE  (1u << 3)
#define MIE_MTIE     (1u << 7)
#define MIE_MEIE     (1u << 11)

#define MCAUSE_INTR_MASK 0x80000000u
#define MCAUSE_TIMER     7u
#define MCAUSE_EXTERNAL  11u
#define MCAUSE_ILLEGAL   2u

#define TIMER_BASE       0x02004000u
#define TIMER_MTIMECMP_LO (*(volatile uint32_t*)(TIMER_BASE + 0x08u))
#define TIMER_MTIMECMP_HI (*(volatile uint32_t*)(TIMER_BASE + 0x0Cu))

static inline void timer_set_mtimecmp(uint64_t value) {
    TIMER_MTIMECMP_LO = (uint32_t)(value & 0xFFFFFFFFu);
    TIMER_MTIMECMP_HI = (uint32_t)(value >> 32);
}

/** Arm mtimecmp relative to current MTIME (32-bit stores; avoids libgcc helpers). */
static inline void timer_arm_relative(uint32_t delta) {
    const uint32_t t = *(volatile uint32_t*)(TIMER_BASE + 0x00u);
    TIMER_MTIMECMP_HI = 0u;
    TIMER_MTIMECMP_LO = t + delta;
}


#define UART_RX_RING_SIZE 64u
#define UART_RX_RING_MASK (UART_RX_RING_SIZE - 1u)

extern char trap_vector[];
/** Top of dedicated trap stack (set into mscratch before IRQ delivery). */
extern char trap_cpu_stack_top[];

static inline void uart_trap_stack_init(void) {
    const uintptr_t top = (uintptr_t)&trap_cpu_stack_top;
    __asm__ volatile("csrw mscratch, %0" :: "r"(top));
}

extern volatile uint32_t g_trap_count;
extern volatile uint32_t g_last_mcause;
/* trap_timer_uart.S */
extern volatile uint32_t g_timer_irq_count;
extern volatile uint32_t g_uart_irq_count;
extern volatile uint32_t g_irq_log_idx;
extern volatile uint32_t g_irq_log[8];
extern volatile uint32_t g_uart_rx_char;
extern volatile uint32_t g_uart_rx_head;
extern volatile uint32_t g_uart_rx_tail;
extern volatile uint8_t g_uart_rx_ring[UART_RX_RING_SIZE];

/** Drain hardware RX FIFO (polling); use before uart_irq_enable if not using SIM_INJECT. */
static inline void uart_drain_rx_fifo(void) {
    uint32_t ch;
    while (uart_try_getc_u32(&ch) == 0) {
        (void)ch;
    }
}

static inline void write_mtvec(uint32_t value) {
    __asm__ volatile("csrw mtvec, %0" :: "r"(value));
}

static inline void write_mie(uint32_t value) {
    __asm__ volatile("csrw mie, %0" :: "r"(value));
}

static inline void set_mie_bits(uint32_t mask) {
    __asm__ volatile("csrs mie, %0" :: "r"(mask));
}

static inline void clear_mie_bits(uint32_t mask) {
    __asm__ volatile("csrc mie, %0" :: "r"(mask));
}

static inline void enable_global_interrupts(void) {
    const uint32_t mask = MSTATUS_MIE;
    __asm__ volatile("csrs mstatus, %0" :: "r"(mask));
}

static inline void disable_global_interrupts(void) {
    const uint32_t mask = MSTATUS_MIE;
    __asm__ volatile("csrc mstatus, %0" :: "r"(mask));
}

/** Timer IRQ: mtvec + MIE.MTIE + MSTATUS.MIE + first mtimecmp. */
static inline void timer_irq_enable(uint64_t first_mtimecmp) {
    g_trap_count = 0;
    g_last_mcause = 0;
    write_mtvec((uint32_t)(uintptr_t)trap_vector);
    write_mie(MIE_MTIE);
    enable_global_interrupts();
    timer_set_mtimecmp(first_mtimecmp);
}

static inline void uart_irq_reset_buffer(void) {
    g_uart_rx_head = 0;
    g_uart_rx_tail = 0;
    g_trap_count = 0;
    g_last_mcause = 0;
    g_uart_rx_char = 0;
}

/** Drop all pending RX (HW FIFO + software ring). Briefly masks MIE. */
static inline void uart_irq_flush_rx(void) {
    disable_global_interrupts();
    g_uart_rx_head = g_uart_rx_tail;
    uart_drain_rx_fifo();
    uart_clear_rx_pending();
    enable_global_interrupts();
}

/** Install trap_vector, enable UART RX interrupt + M-mode IRQ delivery. */
static inline void uart_irq_enable(void) {
    uart_irq_reset_buffer();
    uart_trap_stack_init();
    write_mtvec((uint32_t)(uintptr_t)trap_vector);
    uart_enable_tx();
    uart_enable_rx();
    TEST_UART_IE = TEST_UART_IE_RX;
    write_mie(MIE_MEIE);
    enable_global_interrupts();
}

/**
 * Interactive stdin: show prompts first, then discard stale HW FIFO bytes (polling),
 * then enable trap + RX IRQ. Do not call uart_drain_rx_fifo() after this.
 */
static inline void uart_irq_enable_interactive(void) {
    uart_disable_irq();
    uart_enable_tx();
    uart_enable_rx();
    uart_drain_rx_fifo();
    uart_clear_rx_pending();
    uart_irq_enable();
}

int uart_irq_try_getc(uint32_t* out_ch);
int uart_irq_getc_blocking(uint32_t* out_ch);
int uart_irq_getc_timeout(uint32_t* out_ch, uint32_t max_wfi_spins);

static inline void uart_put_u32_dec(uint32_t v) {
    if (v == 0u) {
        uart_putc_u32('0');
        return;
    }
    char buf[10];
    int n = 0;
    while (v > 0u && n < 10) {
        buf[n++] = (char)('0' + (v % 10u));
        v /= 10u;
    }
    while (n > 0) {
        uart_putc_u32((uint8_t)buf[--n]);
    }
}

static inline void uart_put_hex_byte(uint32_t v) {
    static const char hex[] = "0123456789abcdef";
    uart_putc_u32((uint8_t)hex[(v >> 4) & 0xFu]);
    uart_putc_u32((uint8_t)hex[v & 0xFu]);
}

static inline void uart_put_hex_u32(uint32_t v) {
    uart_putc_u32('0');
    uart_putc_u32('x');
    for (int shift = 28; shift >= 0; shift -= 4) {
        uint32_t nibble = (v >> shift) & 0xFu;
        uart_putc_u32((uint8_t)(nibble < 10u ? ('0' + nibble) : ('a' + nibble - 10u)));
    }
}

static inline void sim_exit(int code) {
    __asm__ volatile(
        "li a7, 93\n"
        "mv a0, %0\n"
        "ecall\n"
        :: "r"(code)
        : "a0", "a7", "memory");
    for (;;) {
        __asm__ volatile("wfi");
    }
}

#endif
