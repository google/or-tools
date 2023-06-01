#!/usr/bin/env bash
# Copyright 2010-2022 Google LLC
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set -eo pipefail

function extract() {
  echo "Extracting ${1}..."
  case $1 in
    *.tar.bz2)   tar xjf "$1"    ;;
    *.tar.xz)    tar xJf "$1"    ;;
    *.tar.gz)    tar xzf "$1"    ;;
    *)
      >&2 echo "don't know how to extract '$1'..."
      exit 1
  esac
}

function unpack() {
  mkdir -p "${ARCHIVE_DIR}"
  cd "${ARCHIVE_DIR}" || exit 2
  local -r URL=$1
  local -r RELATIVE_DIR=$2
  local -r DESTINATION="${ARCHIVE_DIR}/${RELATIVE_DIR}"
  if [[  ! -d "${DESTINATION}" ]] ; then
    echo "Downloading ${URL}..."
    local -r ARCHIVE_NAME=$(basename "${URL}")
    [[ -f "${ARCHIVE_NAME}" ]] || wget --no-verbose "${URL}"
    extract "${ARCHIVE_NAME}"
    rm -f "${ARCHIVE_NAME}"
  fi
}

function install_qemu() {
  if [[ "${QEMU_ARCH}" == "DISABLED" ]]; then
    >&2 echo 'QEMU is disabled !'
    return 0
  fi
  local -r QEMU_VERSION=${QEMU_VERSION:=8.0.0}
  local -r QEMU_TARGET=${QEMU_ARCH}-linux-user

  if echo "${QEMU_VERSION} ${QEMU_TARGET}" | cmp --silent "${QEMU_INSTALL}/.build" -; then
    echo "qemu ${QEMU_VERSION} up to date!"
    return 0
  fi

  echo "QEMU_VERSION: ${QEMU_VERSION}"
  echo "QEMU_TARGET: ${QEMU_TARGET}"

  rm -rf "${QEMU_INSTALL}"

  # Checking for a tarball before downloading makes testing easier :-)
  local -r QEMU_URL="https://download.qemu.org/qemu-${QEMU_VERSION}.tar.xz"
  local -r QEMU_DIR="qemu-${QEMU_VERSION}"
  unpack ${QEMU_URL} ${QEMU_DIR}
  cd ${QEMU_DIR} || exit 2

  ./configure \
    --prefix="${QEMU_INSTALL}" \
    --target-list="${QEMU_TARGET}" \
    --audio-drv-list= \
    --disable-brlapi \
    --disable-curl \
    --disable-curses \
    --disable-docs \
    --disable-gcrypt \
    --disable-gnutls \
    --disable-gtk \
    --disable-libnfs \
    --disable-libssh \
    --disable-nettle \
    --disable-opengl \
    --disable-sdl \
    --disable-virglrenderer \
    --disable-vte

  # wrapper on ninja
  make -j8
  make install

  echo "$QEMU_VERSION $QEMU_TARGET" > "${QEMU_INSTALL}/.build"
}

function assert_defined(){
  if [[ -z "${!1}" ]]; then
    >&2 echo "Variable '${1}' must be defined"
    exit 1
  fi
}

function clean_build() {
  # Cleanup previous build
  rm -rf "${BUILD_DIR}"
  mkdir -p "${BUILD_DIR}"
}

function expand_codescape_config() {
  # https://www.mips.com/develop/tools/codescape-mips-sdk/mips-toolchain-configurations/
  # mips-mti: MIPS32R6 and MIPS64R6
  # mips-img: MIPS32R2 and MIPS64R2

  # ref: https://codescape.mips.com/components/toolchain/2020.06-01/downloads.html
  local -r DATE=2020.06-01
  local -r CODESCAPE_URL=https://codescape.mips.com/components/toolchain/${DATE}/Codescape.GNU.Tools.Package.${DATE}.for.MIPS.MTI.Linux.CentOS-6.x86_64.tar.gz
  local -r GCC_RELATIVE_DIR="mips-mti-linux-gnu/${DATE}"

  # ref: https://codescape.mips.com/components/toolchain/2019.02-04/downloads.html
  #local -r DATE=2019.02-04
  #local -r CODESCAPE_URL=https://codescape.mips.com/components/toolchain/${DATE}/Codescape.GNU.Tools.Package.${DATE}.for.MIPS.IMG.Linux.CentOS-6.x86_64.tar.gz
  #local -r GCC_RELATIVE_DIR="mips-img-linux-gnu/${DATE}"

  local -r GCC_URL=${CODESCAPE_URL}
  unpack "${GCC_URL}" "${GCC_RELATIVE_DIR}"

  local -r GCC_DIR=${ARCHIVE_DIR}/${GCC_RELATIVE_DIR}
  local MIPS_FLAGS=""
  local LIBC_DIR_SUFFIX=""
  local FLAVOUR=""
  case "${TARGET}" in
    "mips64")
      MIPS_FLAGS="-EB -mips64r6 -mabi=64"
      #MIPS_FLAGS="-EB -mips64r2 -mabi=64"
      FLAVOUR="mips-r6-hard"
      #FLAVOUR="mips-r2-hard"
      LIBC_DIR_SUFFIX="lib64"
      ;;
    "mips64el")
      MIPS_FLAGS="-EL -mips64r6 -mabi=64"
      #MIPS_FLAGS="-EL -mips64r2 -mabi=64"
      FLAVOUR="mipsel-r6-hard"
      #FLAVOUR="mipsel-r2-hard"
      LIBC_DIR_SUFFIX="lib64"
      ;;
    *)
      >&2 echo 'unknown mips platform'
      exit 1 ;;
  esac
  local -r SYSROOT_DIR=${GCC_DIR}/sysroot
  local -r STAGING_DIR=${SYSROOT_DIR}-stage

  # Write a Toolchain file
  # note: This is manadatory to use a file in order to have the CMake variable
  # 'CMAKE_CROSSCOMPILING' set to TRUE.
  # ref: https://cmake.org/cmake/help/latest/manual/cmake-toolchains.7.html#cross-compiling-for-linux
  cat >"${TOOLCHAIN_FILE}" <<EOL
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR ${TARGET})

set(CMAKE_SYSROOT ${SYSROOT_DIR})
set(CMAKE_STAGING_PREFIX ${STAGING_DIR})

set(tools ${GCC_DIR})

# R6
set(CMAKE_C_COMPILER \${tools}/bin/mips-mti-linux-gnu-gcc)
set(CMAKE_C_FLAGS "${MIPS_FLAGS}")
set(CMAKE_CXX_COMPILER \${tools}/bin/mips-mti-linux-gnu-g++)
set(CMAKE_CXX_FLAGS "${MIPS_FLAGS} -L${SYSROOT_DIR}/usr/lib64")

# R2
#set(CMAKE_C_COMPILER \${tools}/bin/mips-img-linux-gnu-gcc)
#set(CMAKE_C_FLAGS "${MIPS_FLAGS}")
#set(CMAKE_CXX_COMPILER \${tools}/bin/mips-img-linux-gnu-g++)
#set(CMAKE_CXX_FLAGS "${MIPS_FLAGS}")

set(CMAKE_FIND_ROOT_PATH ${GCC_DIR})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
EOL

CMAKE_ADDITIONAL_ARGS+=( -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN_FILE}" )
QEMU_ARGS+=( -L "${SYSROOT_DIR}/${FLAVOUR}" )
local -r LIBC_DIR=${GCC_DIR}/mips-mti-linux-gnu/lib/${FLAVOUR}/${LIBC_DIR_SUFFIX}
#local -r LIBC_DIR=${GCC_DIR}/mips-img-linux-gnu/lib/${FLAVOUR}/${LIBC_DIR_SUFFIX}
QEMU_ARGS+=( -E LD_PRELOAD="${LIBC_DIR}/libstdc++.so.6:${LIBC_DIR}/libgcc_s.so.1" )
}

function expand_bootlin_config() {
  # ref: https://toolchains.bootlin.com/
  local -r GCC_DIR=${ARCHIVE_DIR}/${GCC_RELATIVE_DIR}

  case "${TARGET}" in
    "aarch64")
      local -r POWER_URL="https://toolchains.bootlin.com/downloads/releases/toolchains/aarch64/tarballs/aarch64--glibc--stable-2021.11-1.tar.bz2"
      local -r GCC_PREFIX="aarch64"
      ;;
    "aarch64be")
      local -r POWER_URL="https://toolchains.bootlin.com/downloads/releases/toolchains/aarch64be/tarballs/aarch64be--glibc--stable-2021.11-1.tar.bz2"
      local -r GCC_PREFIX="aarch64_be"
      ;;
    "ppc64le")
      local -r POWER_URL="https://toolchains.bootlin.com/downloads/releases/toolchains/powerpc64le-power8/tarballs/powerpc64le-power8--glibc--stable-2021.11-1.tar.bz2"
      local -r GCC_PREFIX="powerpc64le"
      ;;
    "ppc64")
      local -r POWER_URL="https://toolchains.bootlin.com/downloads/releases/toolchains/powerpc64-power8/tarballs/powerpc64-power8--glibc--stable-2021.11-1.tar.bz2"
      local -r GCC_PREFIX="powerpc64"
      ;;
    "ppc")
      #local -r POWER_URL="https://toolchains.bootlin.com/downloads/releases/toolchains/powerpc-e500mc/tarballs/powerpc-e500mc--glibc--stable-2021.11-1.tar.bz2"
      local -r POWER_URL="https://toolchains.bootlin.com/downloads/releases/toolchains/powerpc-440fp/tarballs/powerpc-440fp--glibc--stable-2021.11-1.tar.bz2"
      local -r GCC_PREFIX="powerpc"
      ;;
    *)
      >&2 echo 'unknown power platform'
      exit 1 ;;
  esac

  local -r POWER_RELATIVE_DIR="${TARGET}"
  unpack "${POWER_URL}" "${POWER_RELATIVE_DIR}"
  local -r EXTRACT_DIR="${ARCHIVE_DIR}/$(basename ${POWER_URL%.tar.bz2})"

  local -r POWER_DIR=${ARCHIVE_DIR}/${POWER_RELATIVE_DIR}
  if [[ -d "${EXTRACT_DIR}" ]]; then
    mv "${EXTRACT_DIR}" "${POWER_DIR}"
  fi

  local -r SYSROOT_DIR="${POWER_DIR}/${GCC_PREFIX}-buildroot-linux-gnu/sysroot"
  #local -r STAGING_DIR=${SYSROOT_DIR}-stage

  # Write a Toolchain file
  # note: This is manadatory to use a file in order to have the CMake variable
  # 'CMAKE_CROSSCOMPILING' set to TRUE.
  # ref: https://cmake.org/cmake/help/latest/manual/cmake-toolchains.7.html#cross-compiling-for-linux
  cat >"${TOOLCHAIN_FILE}" <<EOL
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR ${GCC_PREFIX})

set(CMAKE_SYSROOT ${SYSROOT_DIR})
#set(CMAKE_STAGING_PREFIX ${STAGING_DIR})

set(tools ${POWER_DIR})

set(CMAKE_C_COMPILER \${tools}/bin/${GCC_PREFIX}-linux-gcc)
set(CMAKE_C_FLAGS "${POWER_FLAGS}")
set(CMAKE_CXX_COMPILER \${tools}/bin/${GCC_PREFIX}-linux-g++)
set(CMAKE_CXX_FLAGS "${POWER_FLAGS} -L${SYSROOT_DIR}/lib")

set(CMAKE_FIND_ROOT_PATH ${POWER_DIR})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
EOL

CMAKE_ADDITIONAL_ARGS+=( -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN_FILE}" )
QEMU_ARGS+=( -L "${SYSROOT_DIR}" )
QEMU_ARGS+=( -E LD_PRELOAD="${SYSROOT_DIR}/usr/lib/libstdc++.so.6:${SYSROOT_DIR}/lib/libgcc_s.so.1" )
}

function build() {
  cd "${PROJECT_DIR}" || exit 2
  set -x
  clean_build
  cmake -S. -B"${BUILD_DIR}" "${CMAKE_DEFAULT_ARGS[@]}" "${CMAKE_ADDITIONAL_ARGS[@]}"
  cmake --build "${BUILD_DIR}" --target all -j8 -v
  set +x
}

function run_test() {
  assert_defined QEMU_ARCH
  if [[ "${QEMU_ARCH}" == "DISABLED" ]]; then
    >&2 echo "QEMU is disabled for ${TARGET}"
    return
  fi
  install_qemu
  RUN_CMD="${QEMU_INSTALL}/bin/qemu-${QEMU_ARCH} ${QEMU_ARGS[*]}"

  cd "${BUILD_DIR}" || exit 2
  set -x
  case ${PROJECT} in
    glop)
      # shellcheck disable=SC2086
      ${RUN_CMD} bin/simple_glop_program ;;
    or-tools)
      for test_binary in \
        "${BUILD_DIR}"/bin/simple_* \
        "${BUILD_DIR}"/bin/*tsp* \
        "${BUILD_DIR}"/bin/*vrp*; do
        # shellcheck disable=SC2086
        ${RUN_CMD} "${test_binary}"
      done
      ;;
    *)
      >&2 echo "Unknown PROJECT '${PROJECT}'..."
      exit 1 ;;
  esac
  set +x
}

function usage() {
  local -r NAME=$(basename "$0")
  echo -e "$NAME - Build using a cross toolchain.

SYNOPSIS
\t$NAME [-h|--help] [toolchain|build|qemu|test|all]

DESCRIPTION
\tCross compile Google OR-Tools (or Glop) using a cross toolchain.

\tYou MUST define the following variables before running this script:
\t* PROJECT: glop or-tools
\t* TARGET:
\t\tx86_64
\t\taarch64 aarch64be (bootlin)
\t\tmips64 mips64el (codespace)
\t\tppc (bootlin)
\t\tppc64 ppc64le (bootlin)

OPTIONS
\t-h --help: show this help text
\ttoolchain: download, unpack toolchain and generate CMake toolchain file
\tbuild: toolchain + build the project using the toolchain file (note: remove previous build dir)
\tqemu: download, unpack and build qemu
\ttest: qemu + run all executable using qemu (note: don't build !)
\tall: build + test (default)

EXAMPLES
* Using export:
export PROJECT=glop
export TARGET=aarch64-linux-gnu
$0

* One-liner:
PROJECT=or-tools TARGET=aarch64 $0"
}

# Main
function main() {
  case ${1} in
    -h | --help)
      usage; exit ;;
  esac

  assert_defined PROJECT
  assert_defined TARGET

  # shellcheck disable=SC2155
  declare -r PROJECT_DIR="$(cd -P -- "$(dirname -- "$0")/.." && pwd -P)"
  declare -r ARCHIVE_DIR="${PROJECT_DIR}/build_cross/archives"
  declare -r BUILD_DIR="${PROJECT_DIR}/build_cross/${TARGET}"
  declare -r TOOLCHAIN_FILE=${ARCHIVE_DIR}/toolchain_${TARGET}.cmake

  echo "Project: '${PROJECT}'"
  echo "Target: '${TARGET}'"

  echo "Project dir: '${PROJECT_DIR}'"
  echo "Archive dir: '${ARCHIVE_DIR}'"
  echo "Build dir: '${BUILD_DIR}'"
  echo "toolchain file: '${TOOLCHAIN_FILE}'"

  declare -a CMAKE_DEFAULT_ARGS=( -G ${CMAKE_GENERATOR:-"Unix Makefiles"} -DBUILD_DEPS=ON )
  case ${PROJECT} in
    glop)
      CMAKE_DEFAULT_ARGS+=( -DBUILD_CXX=OFF -DBUILD_GLOP=ON ) ;;
    or-tools)
      CMAKE_DEFAULT_ARGS+=( -DBUILD_CXX=ON ) ;;
    *)
      >&2 echo "Unknown PROJECT '${PROJECT}'..."
      exit 1 ;;
  esac
  declare -a CMAKE_ADDITIONAL_ARGS=()

  declare -a QEMU_ARGS=()
  case ${TARGET} in
    x86_64)
      declare -r QEMU_ARCH=x86_64 ;;
    aarch64)
      expand_bootlin_config
      declare -r QEMU_ARCH=aarch64 ;;
    aarch64be)
      expand_bootlin_config
      declare -r QEMU_ARCH=aarch64_be ;;
    mips64)
      expand_codescape_config
      declare -r QEMU_ARCH=mips64 ;;
    mips64el)
      expand_codescape_config
      declare -r QEMU_ARCH=mips64el ;;
    ppc64le)
      expand_bootlin_config
      declare -r QEMU_ARCH=ppc64le ;;
    ppc64)
      expand_bootlin_config
      declare -r QEMU_ARCH=ppc64 ;;
    ppc)
      expand_bootlin_config
      declare -r QEMU_ARCH=ppc ;;
    *)
      >&2 echo "Unknown TARGET '${TARGET}'..."
      exit 1 ;;
  esac
  declare -r QEMU_INSTALL=${ARCHIVE_DIR}/qemu-${QEMU_ARCH}

  case ${1} in
    toolchain)
      exit ;;
    build)
      build ;;
    qemu)
      install_qemu ;;
    test)
      run_test ;;
    *)
      build
      run_test ;;
  esac
}

main "${1:-all}"
