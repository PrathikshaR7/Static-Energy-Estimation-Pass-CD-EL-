#!/bin/bash
# run_tests.sh — macOS/Linux
set -e

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$REPO/build"
PASS="$REPO/pass-build/mac-arm64/lib/EnergyPass.dylib"
SYSROOT="$(xcrun --show-sdk-path)"
export ENERGY_MODEL_PATH="$REPO/data/energy_model.json"

echo "========================================"
echo " EnergyPass Test Suite"
echo "========================================"

for src in "$REPO"/test/0*.c; do
  name="$(basename "${src%.c}")"
  echo ""
  echo "========================================"
  echo " $name"
  echo "========================================"
  bc="$(mktemp /tmp/${name}.bc.XXXX)"
  yml="$(getconf DARWIN_USER_TEMP_DIR 2>/dev/null || echo /tmp)/${name}_remarks.yml"

  "$BUILD/bin/clang" -O1 -g -emit-llvm \
    -isysroot "$SYSROOT" "$src" -c -o "$bc"

  "$BUILD/bin/opt" \
    -load-pass-plugin="$PASS" \
    -passes="energy-pass" \
    -pass-remarks-analysis=energy \
    -pass-remarks-output="$yml" \
    -disable-output "$bc"
done

echo ""
echo "========================================"
echo " All tests complete."
echo "========================================"
