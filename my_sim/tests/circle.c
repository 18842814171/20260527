#include <stdint.h>
#include <interrupt/common.h>

int main(void) {
    uart_irq_enable();

    UART_PUTLIT("Drawing a circle...\n");
    for (int y = -10; y <= 10; y++) {
        for (int x = -10; x <= 10; x++) {
            if (x * x + y * y <= 100) {
                uart_putc_u32('*');
            } else {
                uart_putc_u32(' ');
            }
        }
        UART_NL();
    }
    UART_PUTLIT("Done.\n");
    sim_exit(0);
}
