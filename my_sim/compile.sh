#!/bin/bash
# compile.sh - RISC-V 编译脚本
#
# 用法:
#   ./compile.sh c <tests/foo.c>     # 裸机 C（crt0 + rv32.ld；按源文件自动链接 trap_*.S）
#   ./compile.sh asm <file.s>        # 汇编 -> .o
#   ./compile.sh dump <file>         # 反汇编 ELF

set -e

CMD=${1:-}
SRC=${2:-}
OUT_DIR="out"
ASM_DIR="asm"
ROOT="$(cd "$(dirname "$0")" && pwd)"
CC="${RV32_CC:-riscv64-unknown-elf-gcc}"
AS="${RV32_AS:-riscv64-unknown-elf-as}"
OBJDUMP="${RV32_OBJDUMP:-riscv64-unknown-elf-objdump}"

mkdir -p "$OUT_DIR"

INC_ROOT="-I$ROOT/include"
ASM_INT="$ROOT/asm/interrupt"
TOOLS_INT="$ROOT/tools/interrupt"
GUEST_CFLAGS="-march=rv32im_zicsr -mabi=ilp32 -nostdlib -static $INC_ROOT"
GUEST_LDFLAGS="-T $TOOLS_INT/rv32.ld -Wl,--gc-sections"
CRT0="$ASM_INT/crt0.S"

resolve_src() {
    local f="$1"
    if [[ -z "$f" ]]; then
        echo "缺少源文件参数" >&2
        exit 1
    fi
    if [[ -f "$f" ]]; then
        echo "$f"
        return
    fi
    if [[ -f "$ROOT/tests/$f" ]]; then
        echo "$ROOT/tests/$f"
        return
    fi
    if [[ -f "$ROOT/tests/${f}.c" ]]; then
        echo "$ROOT/tests/${f}.c"
        return
    fi
    echo "找不到源文件: $f" >&2
    exit 1
}

# Return trap handler path for this guest, or empty if none.
guest_trap_for_src() {
    case "$(basename "$1")" in
        interrupt_timer_test.c|interrupt_csr_test.c)
            echo "$ASM_INT/trap_timer.S"
            ;;
        interrupt_uart_test.c)
            echo "$ASM_INT/trap_uart.S"
            ;;
        interrupt_timer_uart_test.c)
            echo "$ASM_INT/trap_timer_uart.S"
            ;;
        interrupt_exception_test.c)
            echo "$ASM_INT/trap_exception.S"
            ;;
        simple_uart_test.c)
            echo ""
            ;;
        io.c|io_line.c|char.c|hex_str.c|circle.c|abc.c|2.c)
            echo "$ASM_INT/trap_uart.S"
            ;;
        *)
            echo ""
            ;;
    esac
}

UART_IRQ_C="$ROOT/tests/uart_irq.c"

build_guest_c() {
    local src="$1"
    local basename
    basename=$(basename "$src" .c)
    local objs=("$CRT0")
    local trap
    trap=$(guest_trap_for_src "$src")
    if [[ -n "$trap" ]]; then
        objs+=("$trap")
        case "$(basename "$trap")" in
            trap_uart.S|trap_timer_uart.S)
                objs+=("$UART_IRQ_C")
                ;;
        esac
    fi
    echo "Building: $basename${trap:+ + $(basename "$trap")}"
    "$CC" $GUEST_CFLAGS $GUEST_LDFLAGS "${objs[@]}" "$src" -o "$OUT_DIR/${basename}"
}

build_standard_irq_tests() {
    build_guest_c "$ROOT/tests/interrupt_timer_test.c"
    build_guest_c "$ROOT/tests/interrupt_uart_test.c"
    build_guest_c "$ROOT/tests/interrupt_exception_test.c"
    build_guest_c "$ROOT/tests/interrupt_csr_test.c"
    build_guest_c "$ROOT/tests/interrupt_timer_uart_test.c"
    echo "OK: $OUT_DIR/interrupt_{timer,uart,exception,csr,timer_uart}_test"
}

case "$CMD" in
    asm)
        BASENAME=$(basename "$SRC" .s)
        "$AS" -march=rv32im "$SRC" -o "$OUT_DIR/${BASENAME}.o"
        echo "OK: $OUT_DIR/${BASENAME}.o"
        ;;

    c)
        if [[ -z "$SRC" ]]; then
            build_standard_irq_tests
        else
            SRC=$(resolve_src "$SRC")
            build_guest_c "$SRC"
            echo "OK: $OUT_DIR/$(basename "$SRC" .c)"
        fi
        ;;

    dump)
        BASENAME=$(basename "$SRC")
        BASENAME="${BASENAME%.*}"
        OUTPUT_FILE="$ASM_DIR/${BASENAME}.s"
        "$OBJDUMP" -d "$SRC" > "$OUTPUT_FILE"
        echo "Saved to: $OUTPUT_FILE"
        ;;

    *)
        echo "用法: $0 <command> [file]"
        echo ""
        echo "命令:"
        echo "  c          [tests/foo.c]  裸机 ELF（crt0 + rv32.ld；自动链接 trap_*.S）"
        echo "                           无参数时编译 interrupt_{timer,uart,exception,csr,timer_uart}_test"
        echo "  asm        <file.s>       汇编 -> .o"
        echo "  dump       <file>         反汇编"
        exit 1
        ;;
esac
