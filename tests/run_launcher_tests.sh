#!/bin/bash
# Compila e roda os testes de launcher/status.c e launcher/whatsnew.c no PC, com e sem sanitizers.
#   bash tests/run_launcher_tests.sh [iterações do fuzz]
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
CC=${CC:-cc}
ITERS=${1:-50000}
OUT=$(mktemp -d)
cd "$OUT"
FLAGS=(-std=c99 -Wall -Wextra -Werror -I"$ROOT/launcher")
SRC=("$ROOT/launcher/status.c" "$ROOT/launcher/whatsnew.c" "$ROOT/tests/launcher_test.c")

"$CC" "${FLAGS[@]}" -O2 "${SRC[@]}" -o launcher_test
./launcher_test 3000

if "$CC" -fsanitize=address,undefined -x c /dev/null -c -o /dev/null 2>/dev/null; then
    "$CC" "${FLAGS[@]}" -g -O1 -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer \
        "${SRC[@]}" -o launcher_test_san
    ./launcher_test_san "$ITERS"
fi
echo "testes do launcher ok"
