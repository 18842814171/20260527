#ifndef INTERRUPT_MMIO_UART_H
#define INTERRUPT_MMIO_UART_H

#include <stdint.h>

/* UART MMIO for bare-metal guest programs (matches device/UART.hpp layout). */
#define TEST_UART_BASE 0x10000000u
#define TEST_UART_TXDATA (*(volatile uint32_t*)(TEST_UART_BASE + 0x00u))
#define TEST_UART_RXDATA (*(volatile uint32_t*)(TEST_UART_BASE + 0x04u))
#define TEST_UART_TXCTRL (*(volatile uint32_t*)(TEST_UART_BASE + 0x08u))
#define TEST_UART_RXCTRL (*(volatile uint32_t*)(TEST_UART_BASE + 0x0Cu))
#define TEST_UART_IE (*(volatile uint32_t*)(TEST_UART_BASE + 0x10u))
#define TEST_UART_IP     (*(volatile uint32_t*)(TEST_UART_BASE + 0x14u))
#define TEST_UART_IE_RX  (1u << 1)

#define UART_PUTC(ch) (TEST_UART_TXDATA = (uint32_t)(uint8_t)(ch))
#define UART_NL() UART_PUTC('\n')

static inline void uart_enable_tx(void) {
    TEST_UART_TXCTRL = 1u;
}

static inline void uart_enable_rx(void) {
    TEST_UART_RXCTRL = 1u;
}

static inline void uart_disable_irq(void) {
    TEST_UART_IE = 0u;
}

/** W1C: clear UART RX pending after polling drain (avoids spurious MEI). */
static inline void uart_clear_rx_pending(void) {
    TEST_UART_IP = TEST_UART_IE_RX;
}

static inline void uart_putc_u32(uint32_t c) {
    TEST_UART_TXDATA = (uint32_t)(c & 0xFFu);
}

static inline void uart_write(const char* buf, uint32_t len) {
    for (uint32_t i = 0; i < len; ++i) {
        uart_putc_u32((uint8_t)buf[i]);
    }
}

static inline void uart_puts(const char* s) {
    while (*s) {
        uart_putc_u32((uint8_t)*s++);
    }
}

static inline void uart_puts_n(const char* s, uint32_t max_len) {
    uint32_t i = 0;
    while (i < max_len && s[i] != '\0') {
        uart_putc_u32((uint8_t)s[i]);
        i++;
    }
}

#define UART_PUTLIT(lit) uart_write((lit), (uint32_t)(sizeof(lit) - 1u))

static inline void uart_println(const char* s) {
    uart_puts(s);
    UART_NL();
}

static inline void uart_put_repeat(char c, uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) {
        uart_putc_u32((uint8_t)c);
    }
}

static inline int uart_try_getc_u32(uint32_t* out_c) {
    uint32_t v = TEST_UART_RXDATA;
    if ((v & 0x80000000u) || v == 0x00000000u) {
        return -1;
    }
    *out_c = v & 0xFFu;
    return 0;
}

static inline int uart_getc_u32_timeout(uint32_t* out_c, uint32_t max_spins) {
    for (uint32_t i = 0; i < max_spins; ++i) {
        if (uart_try_getc_u32(out_c) == 0) {
            return 0;
        }
    }
    return -1;
}

#define UART_PUT3(c1, c2, c3) \
    do {                      \
        uart_putc_u32((uint8_t)(c1)); \
        uart_putc_u32((uint8_t)(c2)); \
        uart_putc_u32((uint8_t)(c3)); \
    } while (0)

#endif
