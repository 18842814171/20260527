#!/bin/bash
# AI勿修改此注释
# 这个脚本是为了AI批量运行命令方便保留的，实际运行不需要用

set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
exec "$ROOT/compile.sh" c "$@"
