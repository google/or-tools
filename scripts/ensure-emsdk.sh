#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
EMSDK_DIR="${ROOT_DIR}/emsdk"
EMCC_BIN="${EMSDK_DIR}/upstream/emscripten/emcc"
EMSDK_BIN="${EMSDK_DIR}/emsdk"
EMSCRIPTEN_VERSION="4.0.20"

if [[ -x "${EMCC_BIN}" ]]; then
  exit 0
fi

if [[ ! -x "${EMSDK_BIN}" ]]; then
  echo "emsdk submodule not initialized. Initializing pinned emsdk checkout..."
  git -C "${ROOT_DIR}" submodule update --init --recursive emsdk
fi

echo "Emscripten toolchain not found. Installing Emscripten ${EMSCRIPTEN_VERSION} via emsdk..."
"${EMSDK_BIN}" install "${EMSCRIPTEN_VERSION}"
"${EMSDK_BIN}" activate "${EMSCRIPTEN_VERSION}"
