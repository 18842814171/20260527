#include <stdint.h>
#include <interrupt/common.h>

int uart_irq_try_getc(uint32_t* out_ch) {
    uint32_t tail = g_uart_rx_tail;
    while (tail != g_uart_rx_head) {
        const uint32_t ch = (uint32_t)g_uart_rx_ring[tail & UART_RX_RING_MASK];
        tail += 1u;
        g_uart_rx_tail = tail;
        if (ch != 0u) {
            *out_ch = ch;
            return 0;
        }
    }
    return -1;
}

int uart_irq_getc_blocking(uint32_t* out_ch) {
    /* nop (not wfi): one full CPU step per spin so IRQ is taken at step boundary. */
    while (uart_irq_try_getc(out_ch) != 0) {
        __asm__ volatile("nop");
    }
    return 0;
}

int uart_irq_getc_timeout(uint32_t* out_ch, uint32_t max_wfi_spins) {
    for (uint32_t i = 0; i < max_wfi_spins; ++i) {
        if (uart_irq_try_getc(out_ch) == 0) {
            return 0;
        }
        __asm__ volatile("nop");
    }
    return -1;
}
