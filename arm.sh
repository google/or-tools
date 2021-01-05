#!/usr/bin/env bash

set -euo pipefail

#export TOOLCHAIN=NATIVE
#export TARGET=x86_64
#export QEMU_ARCH=DISABLED

export TOOLCHAIN=LINARO
export TARGET=aarch64-linux-gnu
export QEMU_ARCH=aarch64

./tools/cross_compile_glop.sh
