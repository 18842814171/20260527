#include <interrupt/common.h>

int main(void) {
    uart_irq_enable();

    UART_PUTLIT("abc");
    UART_NL();
    sim_exit(0);
}
