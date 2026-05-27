# RISC-V 教学模拟器（RV32IM，M-mode 裸机）

RV32IM、1MB RAM、自研 Timer/UART/Display MMIO、**无 MMU / 无 PLIC / 无 VirtIO**。  
适合 ISA 与 M-mode 中断教学；**不能**直接启动 Linux（见 [`task.md`](task.md) 阶段 2 需求）。

更多实现细节与步数参考：[`notes.md`](notes.md)、[`tools/interrupt/README.md`](tools/interrupt/README.md)。

---

## 环境要求

- 主机：`g++`（C++17）、`make`、`pthread`
- Guest 交叉工具链：`riscv64-unknown-elf-gcc` / `as` / `objdump`（可通过 `RV32_CC`、`RV32_AS`、`RV32_OBJDUMP` 覆盖）

---

## 编译

### 模拟器（主机）

```bash
make              # 生成 ./build/test
make clean        # 清理 build/
```

### Guest 程序（裸机 ELF）

`./compile.sh c` 使用 **nostdlib + `asm/interrupt/crt0.S` + `tools/interrupt/rv32.ld`（入口 `0x10000`）**，按源文件名自动链接 `asm/interrupt/trap_*.S` 与（部分程序）`tests/uart_irq.c`。

```bash
./compile.sh c                                    # 五个标准中断测试 → out/interrupt_*_test
./compile.sh c interrupt_timer_test               # 单个 .c（可写 tests/foo 或 foo）
./compile.sh c tests/io_line.c
./compile.sh c timer_uart_test                    # 无 trap，仅 MMIO 轮询
./compile.sh asm asm/simple.s                     # 汇编 → out/simple.o（仅 .o，需自行链接）
```
以上命令输出目录：`out/<basename>`（无扩展名 ELF）。
```
./compile.sh dump out/io_line                     # 反汇编 → asm/io_line.s，输出目录asm/
```



### Trap 与源文件对应关系

| Guest 源文件 | 链接的 trap |
|--------------|-------------|
| `interrupt_timer_test.c`, `interrupt_csr_test.c` | `trap_timer.S` |
| `interrupt_uart_test.c`, `io_line.c`, `abc.c`, `circle.c`, `hex_str.c` | `trap_uart.S` + `uart_irq.c` |
| `interrupt_timer_uart_test.c` | `trap_timer_uart.S` + `uart_irq.c` |
| `interrupt_exception_test.c` | `trap_exception.S` |
| `timer_uart_test.c` | （无 trap） |

---

## 运行模拟器

入口：**`./build/test`**（`test.cpp` → `simulator()`）。

### 命令行

```bash
./build/test <elf> [max_steps] [选项...]
```

| 参数 / 选项 | 说明 |
|-------------|------|
| `<elf>` | ELF 路径；可只写名字，自动查找 **当前目录 → `out/` → `tests/`**（如 `io_line` → `out/io_line`） |
| `max_steps` | 正整数，最大模拟周期数；**省略或与其他标志混排时**，`0` 会被忽略，改用默认上限 |
| `--debug=0` | 安静：不写 `log/`、少刷屏；stderr 仍保留告警/错误 |
| `--debug=1` | 默认：写 `log/`、打印 `[sim]` 横幅与统计 |
| `--trap-uart=1` | 将 `[Interrupt] TRAP: ...` 镜像到 **stdout**（与 Guest UART 同路） |
| `--trap_uart=1` | 同上（下划线别名，会提示推荐连字符写法） |
| `--trap-uart` / `--trap_uart` | 等价于 `--trap-uart=1` |
| `--log-steps=<n>` | 仅限制写入日志的步数，**不**改变运行停止条件 |
| `-h`, `--help` | 帮助 |

**步数上限（`core/CPU.hpp`）**

- 未指定有效 `max_steps`：默认 `CPU::kDefaultRunStepLimit` = **10,000,000**
- 显式指定：不超过 `CPU::kHardAbsoluteRunStepLimit` = **100,000,000**

### 环境变量

| 变量 | 作用 |
|------|------|
| `SIM_INJECT_UART=<byte>` | 启动前向 UART RX FIFO 注入**一字节**；**关闭**主机 stdin 线程（CI 确定性测试）。例：`SIM_INJECT_UART=U` |
| `SIM_TRAP_UART=1` | 等同默认开启 `--trap-uart=1` |

### 运行示例

```bash
# 阶段 1 全套（推荐）
make && ./tools/interrupt/run_tests.sh

# 单测 + 步数上限 + 安静
./build/test interrupt_timer_test 500000 --debug=0

# CI：UART 中断（注入，不用键盘）
env SIM_INJECT_UART=U ./build/test interrupt_uart_test 500000 --debug=0

# Timer + UART 联合（Phase B 必须注入 T）
env SIM_INJECT_UART=T ./build/test interrupt_timer_uart_test 500000 --debug=0

# 终端上看 TRAP 流水（连字符推荐）
./build/test interrupt_timer_test --trap-uart=1

# 交互式行输入（真实 TTY，不要设 SIM_INJECT_UART）
./compile.sh c io_line
./build/test io_line
```

正常结束 Guest 会 `sim_exit(code)`（ECALL）；模拟器 stderr 会打印 `exit_code`、`steps`、`halted`。Guest 文本经 UART 出现在 stdout。

---

## 测试程序说明

### 阶段 1 标准中断测试（`./compile.sh c` 无参数会全部编译）

| ELF | Trap | 验证内容 | 推荐运行 |
|-----|------|----------|----------|
| `interrupt_timer_test` | timer | 2 次 MTIP → handler → `mret` | `./build/test interrupt_timer_test 500000 --debug=0` |
| `interrupt_uart_test` | uart | 1 次 MEIP，RX 字节进 `g_uart_rx_char` | `env SIM_INJECT_UART=U ./build/test interrupt_uart_test 500000 --debug=0` |
| `interrupt_exception_test` | exception | 非法指令同步陷阱，`mepc+4` | `./build/test interrupt_exception_test 500000 --debug=0` |
| `interrupt_csr_test` | timer | `csrc`/`csrs mie` 屏蔽与恢复；SLTU | `./build/test interrupt_csr_test 500000 --debug=0` |
| `interrupt_timer_uart_test` | timer+uart | Phase A：仅 timer×2；Phase B：`irq_log` 先 EXTERNAL 后 TIMER | `env SIM_INJECT_UART=T ./build/test interrupt_timer_uart_test 500000 --debug=0` |

`interrupt_timer_uart_test` 无注入时 Phase B 会失败，常见 **`exit_code=3`**（无 UART trap）或 **`4`**（无后续 timer）；**`6`/`7`** 为 `g_irq_log` 顺序错误。

### 其他 Guest 程序（需单独 `./compile.sh c <name>`）

| 源文件 | Trap | 用途 | 运行要点 |
|--------|------|------|----------|
| `timer_uart_test.c` | 无 | 轮询 MTIME + UART 打印，**无 IRQ** | `./build/test timer_uart_test 100000 --debug=0` |
| `io_line.c` | uart | 行编辑：输入一行回显，`q`+Enter 退出 | **真实终端**；`./build/test io_line`；勿用 `SIM_INJECT_UART` |
| `hex_str.c` | uart | 半交互：有键回显，超时心跳退出 | 可键盘或注入；见文件头注释 |
| `abc.c` | uart | 打印 `abc` 后退出 | `./build/test abc --debug=0` |
| `circle.c` | uart | ASCII 圆 | `./build/test circle --debug=0` |
| `uart_irq.c` | — | 库：阻塞 `getc`（供 trap_uart 程序链接） | 不单独运行 |
| `program.c` | 无 | 说明用例（需 newlib 时用主机 gcc，非本脚本默认路径） | — |

### 汇编片段（`asm/`）

`./compile.sh asm <file.s>` 只生成 **`out/*.o`**，不生成完整 ELF。完整裸机程序请用 `tests/*.c` + `./compile.sh c`。  
`asm/` 下另有 `mul_test.s`、`display_stage1.s` 等，供手工链接或历史调试（见 `tests/debug.sh`）。

---

## 设备 MMIO

| 设备 | 基址 | 说明 |
|------|------|------|
| Timer | `0x02004000` | `mtime` / `mtimecmp`，MTIP |
| UART | `0x10000000` | TX/RX、中断；主机 stdin → RX（注入模式除外） |
| Display | `0x10001000` | 简单帧缓冲类设备 |

Guest 头文件：`include/interrupt/common.h`、`include/interrupt/mmio_uart.h`。  
主机中断：`include/Interrupt.hpp`、`core/Interrupt.cpp`。

---

## 目录速览

| 路径 | 说明 |
|------|------|
| `core/` | CPU 流水线、模拟器、中断投递 |
| `device/` | Timer、UART、Display、总线 |
| `asm/interrupt/` | `crt0.S`、`trap_*.S` |
| `tests/` | Guest C 测试 |
| `tools/interrupt/` | `rv32.ld`、`run_tests.sh` |
| `log/` | `--debug=1` 时运行日志（gitignore） |

---

