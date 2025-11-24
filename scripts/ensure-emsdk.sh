#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
EMSDK_DIR="${ROOT_DIR}/emsdk"
EMCC_BIN="${EMSDK_DIR}/upstream/emscripten/emcc"

if [[ -x "${EMCC_BIN}" ]]; then
  exit 0
fi

echo "Emscripten toolchain not found. Installing latest release via emsdk..."
"${EMSDK_DIR}/emsdk" install latest
"${EMSDK_DIR}/emsdk" activate latest
