Simulator Goal 1 — ecall Support

This is your MOST important next feature.

Implement:

user code
→ ecall
→ trap
→ syscall handler
→ return

This directly supports:

syscall layer
kernel/user interaction

and perfectly matches your OS project.

Simulator Goal 2 — Context Switching Validation

Add:

register dump
task switch trace

Example:

[TIMER IRQ]
save task 1
restore task 2
mepc=...
sp=...

This is AMAZING for demos.

Simulator Goal 3 — Lightweight Privilege Separation

You do NOT need full Linux S-mode.

But you CAN implement:

Feature	Worth it?
minimal S-mode	yes
medeleg/mideleg	yes
sret	yes
full Sv39	probably no

This gives architectural depth without MMU hell.

Simulator Goal 4 — Tiny Kernel Runtime

Try running:

scheduler demo
UART shell
syscall tests

inside your simulator.

NOT full OpenRuyi OS.

A reduced profile.


The MOST Important Thing To Avoid

Do NOT attempt simultaneously:

full Linux support
full OpenRuyi compatibility
full MMU
virtio
PLIC
OpenSBI


----------------
## Foundation already in place (not explicit goals, but required)

- **M-mode trap loop:** IRQ → `mtvec` → handler → `mret` → resume  
- **Devices:** Timer, UART (stdin thread), Display MMIO  
- **Tests:** `interrupt_{timer,uart,exception,csr,timer_uart}_test` + `run_tests.sh`  
- **Guest IRQ apps:** `io_line`, `hex_str`, `uart_irq.c`, dedicated trap stacks + `mscratch`  
- **Host syscalls (non-trap path):** `handle_syscall()` for exit, read, write, brk, etc.

That is **not** listed as a separate goal in `task.md`, but it is the main chunk of work already finished.

---

## Goal 1 — ECALL support (trap → syscall handler → return)

**`task.md` wants:** user `ecall` → **trap** → syscall handler → return.

**Today:**

- `ECALL` is handled **inside the CPU pipeline** (`CPU::execute` → `handle_syscall()`), not via `mtvec`.
- Guest bare-metal programs use `ecall` mainly as **`sim_exit` (a7=93)** to stop the simulator.
- There is **no** guest trap vector for syscalls, no syscall table in guest code, no `mepc+4` return path for syscalls.

**Left to do:**

- Route `ecall` through **synchronous trap** (like illegal-instruction path).
- Guest (or host) **syscall dispatcher** after trap.
- **`mret`** back to caller; tests that call syscalls from guest C/asm.
- Align with your OS course’s syscall ABI (likely a7 + args in a0–a2).

**Estimate:** ~**35%** — plumbing exists on the host; the **architecture `task.md` cares about** is still missing.

---

## Goal 2 — Context switching validation

**`task.md` wants:** register dump, task-switch trace, e.g.  
`[TIMER IRQ] save task 1 / restore task 2 / mepc=… sp=…`.

**Today:**

- `[Interrupt] TRAP: enter / mret` logging (optional `--trap-uart=1`).
- **No** multi-task structures, **no** scheduler, **no** save/restore of full contexts, **no** switch trace.

**Left to do:**

- 2+ guest tasks + stacks.
- Timer-driven **preemptive** or cooperative switch.
- Dump/trace on switch (registers, `sp`, `mepc`).
- Optional: hook into existing interrupt trace.

**Estimate:** ~**5%** (only generic trap logging).

---

## Goal 3 — Lightweight privilege separation

**`task.md` wants (explicitly not full Sv39):**

| Item | Status |
|------|--------|
| Minimal S-mode | ❌ |
| `medeleg` / `mideleg` | ❌ |
| `sret` | ❌ |
| Full Sv39 | ❌ (correctly deferred) |

**Today:** M-mode only for traps; `PrivilegeMode::SUPERVISOR` exists in headers but is unused. Only **`mret`**, not **`sret`**.

**Left to do:** S-mode CSRs, delegation, trap to S-mode or M/S coordination, `sret`, and small tests (e.g. syscall in U/S if you add user mode later).

**Estimate:** ~**0%**.

---

## Goal 4 — Tiny kernel runtime

**`task.md` wants:** scheduler demo, UART shell, syscall tests — **reduced profile**, not full OpenRuyi/Linux.

**Today:**

| Piece | Status |
|-------|--------|
| UART shell | **Partial** — `io_line` (line editor + IRQ RX), not a command shell |
| Scheduler demo | ❌ |
| Syscall tests (guest) | ❌ (host `handle_syscall` only) |
| Multitasking / FS | ❌ |

**Left to do:** minimal kernel or supervisor layer in guest ELF: syscall API, maybe 2 tasks + timer, simple shell commands, CI-style syscall tests.

**Estimate:** ~**20%** (`io_line` + interrupt tests as demos).

---

## Suggested order (if you follow `task.md` literally)

1. **Goal 1** — ecall → trap → handler → `mret` (unblocks OS syscalls).  
2. **Goal 4** — tiny runtime + syscall tests + better shell.  
3. **Goal 2** — context-switch trace (great for Demo B).  
4. **Goal 3** — minimal S-mode + delegation (depth without Sv39).

