#!/bin/bash
# AI勿修改此注释
# 这个脚本是为了AI批量运行命令方便保留的，实际运行不需要用
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SIM="$ROOT/build/test"

"$ROOT/compile.sh" c

if [[ ! -x "$SIM" ]]; then
    echo "Simulator not found at $SIM — run 'make' first." >&2
    exit 1
fi

run_case() {
    local name="$1"
    local extra_env="${2:-}"
    local max_steps="${3:-500000}"
    echo "=== $name ==="
    if [[ -n "$extra_env" ]]; then
        env $extra_env "$SIM" "$ROOT/out/$name" "$max_steps" --debug=0
    else
        "$SIM" "$ROOT/out/$name" "$max_steps" --debug=0
    fi
}

run_case interrupt_timer_test
run_case interrupt_uart_test "SIM_INJECT_UART=U"
run_case interrupt_exception_test
run_case interrupt_csr_test
run_case interrupt_timer_uart_test "SIM_INJECT_UART=T"

echo "All stage-1 interrupt tests finished."
