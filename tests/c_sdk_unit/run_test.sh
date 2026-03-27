#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TEST_SRC="$ROOT/test_lcd_mcu_st7735s.c"
LCD_SRC="$ROOT/../../components/drivers/lcd/src/lcd_mcu.c"
STUB_INC="$ROOT/stubs"
BUILD_DIR="$ROOT/build"
OUT="$BUILD_DIR/test_lcd_mcu_st7735s"

mkdir -p "$BUILD_DIR"

if command -v gcc >/dev/null 2>&1; then
  CC=gcc
elif command -v clang >/dev/null 2>&1; then
  CC=clang
else
  echo "gcc/clang not found. Install a host C compiler." >&2
  exit 1
fi

"$CC" -std=c99 -Wall -Wextra -I"$STUB_INC" "$TEST_SRC" "$LCD_SRC" -o "$OUT"
"$OUT"
echo "Done: $OUT"
