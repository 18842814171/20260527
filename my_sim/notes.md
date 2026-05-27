### Interrupt trace on terminal

With `SIM_TRAP_UART=1` or `--trap-uart=1`, `[Interrupt] TRAP: enter ...` / `mret to epc=...` lines are mirrored to **stdout** via the same path as guest UART TX (alongside guest `PASS` text). Full pipeline logs still go to `log/` when `--debug=1`.

### Layout

| Layer | Path |
|--------|------|
| Host delivery | `core/Interrupt.cpp`, `core/CPU.cpp` |
| Guest traps | `asm/interrupt/trap_{timer,uart,exception}.S` |
| Guest helpers | `include/interrupt/common.h`, `tests/uart_irq.c` |
| Tests | `tests/interrupt_*_test.c` |

New IRQ guest: add basename → `guest_trap_for_src()` in `compile.sh`.
