#!/bin/bash
# build.sh — builds the EnergyPass plugin (macOS)
set -e

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

echo "Building EnergyPass..."
cd "$REPO"
cmake --preset mac-arm64
cmake --build --preset mac-arm64
echo ""
echo "Done: $REPO/pass-build/mac-arm64/lib/EnergyPass.dylib"
