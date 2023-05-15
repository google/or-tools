#!/usr/bin/env bash
set -euo pipefail

#./tools/cross_compile.sh --help

export PROJECT=or-tools
#export PROJECT=glop

#export TARGET=x86_64
export TARGET=aarch64
#export TARGET=mips64
#export TARGET=ppc64

./tools/cross_compile.sh toolchain
./tools/cross_compile.sh build
./tools/cross_compile.sh qemu
./tools/cross_compile.sh test
