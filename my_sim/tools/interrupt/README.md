# Bare-metal interrupt support (global)

Guest-side trap/CSR helpers for **any** program under `tests/`. Simulator logic is in `core/Interrupt.cpp` + `include/Interrupt.hpp`.

| Location | Contents |
|----------|----------|
| `include/interrupt/mmio_uart.h` | UART MMIO (all bare-metal tests) |
| `include/interrupt/common.h` | CSR helpers, `sim_exit`, timer setup |
| `asm/interrupt/crt0.S` | `_start`, stack |
| `asm/interrupt/trap_*.S` | Trap vectors (link one per ELF) |
| `tools/interrupt/rv32.ld` | Link script (`mtvec` region at `0x10000`) |

## Build

```bash
# Polling / MMIO tests (no trap handler linked)
./compile.sh c tests/timer_uart_test.c
./compile.sh c timer_uart_test          # also finds tests/timer_uart_test.c

# IRQ tests: ./compile.sh c picks trap_*.S from the .c basename
./compile.sh c
./compile.sh c interrupt_timer_test

make && ./tools/interrupt/run_tests.sh
```

Runs: `interrupt_timer_test`, `interrupt_uart_test`, `interrupt_exception_test`, `interrupt_csr_test`, `interrupt_timer_uart_test` (`SIM_INJECT_UART=T`).

**UART IRQ test** needs a deterministic RX byte (host stdin thread is off while this env is set):

```bash
env SIM_INJECT_UART=U ./build/test out/interrupt_uart_test 500000
```

UART guest programs (`io.c`, `abc.c`, `circle.c`, …) are built with `./compile.sh c`, which links `trap_uart.S` and uses RX interrupts (`uart_irq_enable` in `include/interrupt/common.h`).

In your `tests/*.c`:

```c
#include <interrupt/mmio_uart.h>   /* polling / print */
#include <interrupt/common.h>    /* mtvec, MIE, timer_set_mtimecmp, sim_exit */
```
