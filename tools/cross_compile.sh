#!/usr/bin/env bash

set -eo pipefail

function extract() {
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
    local -r ARCHIVE_NAME=$(basename "${URL}")
    test -f "${ARCHIVE_NAME}" || wget "${URL}"
    extract "${ARCHIVE_NAME}"
    rm -f "${ARCHIVE_NAME}"
  fi
}

function install_qemu() {
  local -r QEMU_VERSION=${QEMU_VERSION:=5.2.0}
  local -r QEMU_TARGET=${QEMU_ARCH}-linux-user

  if echo "${QEMU_VERSION} ${QEMU_TARGET}" | cmp --silent "${QEMU_INSTALL}/.build" -; then
    echo "qemu ${QEMU_VERSION} up to date!"
    return 0
  fi

  echo "QEMU_VERSION: ${QEMU_VERSION}"
  echo "QEMU_TARGET: ${QEMU_TARGET}"

  rm -rf "${QEMU_INSTALL}"

  # Checking for a tarball before downloading makes testing easier :-)
  local -r QEMU_URL="http://wiki.qemu-project.org/download/qemu-${QEMU_VERSION}.tar.xz"
  local -r QEMU_DIR="qemu-${QEMU_VERSION}"
  unpack ${QEMU_URL} ${QEMU_DIR}
  cd ${QEMU_DIR} || exit 2

  # Qemu (meson based build) depends on: pkgconf, libglib2.0, python3, ninja
  ./configure \
    --prefix=${QEMU_INSTALL} \
    --target-list=${QEMU_TARGET} \
    --audio-drv-list= \
    --disable-docs \
    --disable-sdl \
    --disable-gtk \
    --disable-vte \
    --disable-brlapi \
    --disable-opengl \
    --disable-virglrenderer \
    --disable-gnutls \
    --disable-gcrypt \
    --disable-nettle \
    --disable-curses \
    --enable-modules

  # --static Not supported on Archlinux
  # so we use --enable-modules

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

function expand_linaro_config() {
  #ref: https://releases.linaro.org/components/toolchain/binaries/
  #local -r LINARO_VERSION=7.2-2017.11
  local -r LINARO_VERSION=7.5-2019.12
  local -r LINARO_ROOT_URL=https://releases.linaro.org/components/toolchain/binaries/${LINARO_VERSION}

  #local -r GCC_VERSION=7.2.1-2017.11
  local -r GCC_VERSION=7.5.0-2019.12
  local -r GCC_URL=${LINARO_ROOT_URL}/${TARGET}/gcc-linaro-${GCC_VERSION}-x86_64_${TARGET}.tar.xz
  local -r GCC_RELATIVE_DIR="gcc-linaro-${GCC_VERSION}-x86_64_${TARGET}"
  unpack "${GCC_URL}" "${GCC_RELATIVE_DIR}"

  #local -r SYSROOT_VERSION=2.25-2017.11
  local -r SYSROOT_VERSION=2.25-2019.12
  local -r SYSROOT_URL=${LINARO_ROOT_URL}/${TARGET}/sysroot-glibc-linaro-${SYSROOT_VERSION}-${TARGET}.tar.xz
  local -r SYSROOT_RELATIVE_DIR=sysroot-glibc-linaro-${SYSROOT_VERSION}-${TARGET}
  unpack "${SYSROOT_URL}" "${SYSROOT_RELATIVE_DIR}"

  local -r SYSROOT_DIR=${ARCHIVE_DIR}/${SYSROOT_RELATIVE_DIR}
  local -r STAGING_DIR=${ARCHIVE_DIR}/${SYSROOT_RELATIVE_DIR}-stage
  local -r GCC_DIR=${ARCHIVE_DIR}/${GCC_RELATIVE_DIR}

  # Write a Toolchain file
  # note: This is manadatory to use a file in order to have the CMake variable
  # 'CMAKE_CROSSCOMPILING' set to TRUE.
  # ref: https://cmake.org/cmake/help/latest/manual/cmake-toolchains.7.html#cross-compiling-for-linux
  cat >"$TOOLCHAIN_FILE" <<EOL
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR ${TARGET})

set(CMAKE_SYSROOT ${SYSROOT_DIR})
set(CMAKE_STAGING_PREFIX ${STAGING_DIR})

set(tools ${GCC_DIR})
set(CMAKE_C_COMPILER \${tools}/bin/${TARGET}-gcc)
set(CMAKE_CXX_COMPILER \${tools}/bin/${TARGET}-g++)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
EOL
CMAKE_ADDITIONAL_ARGS+=( -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN_FILE}" )
QEMU_ARGS+=( -L "${SYSROOT_DIR}" )
QEMU_ARGS+=( -E LD_LIBRARY_PATH=/lib )
}

function expand_codescape_config() {
  local -r DATE=2020.06-01
  local -r CODESCAPE_URL=https://codescape.mips.com/components/toolchain/${DATE}/Codescape.GNU.Tools.Package.${DATE}.for.MIPS.MTI.Linux.CentOS-6.x86_64.tar.gz
  local -r GCC_URL=${CODESCAPE_URL}
  local -r GCC_RELATIVE_DIR="mips-mti-linux-gnu/${DATE}"
  unpack "${GCC_URL}" "${GCC_RELATIVE_DIR}"

  local -r GCC_DIR=${ARCHIVE_DIR}/${GCC_RELATIVE_DIR}
  local MIPS_FLAGS=""
  local FLAVOUR=""
  local LIBC_DIR_SUFFIX=""
  case "${TARGET}" in
    "mips64")    MIPS_FLAGS="-EB -mabi=64"; FLAVOUR="mips-r2-hard"; LIBC_DIR_SUFFIX="lib64" ;;
    "mips64el")  MIPS_FLAGS="-EL -mabi=64"; FLAVOUR="mipsel-r2-hard"; LIBC_DIR_SUFFIX="lib64" ;;
    *)
      >&2 echo 'unknown mips platform'
      exit 1 ;;
  esac

  CMAKE_ADDITIONAL_ARGS+=( -DCMAKE_FIND_ROOT_PATH="${GCC_DIR}" )
  CMAKE_ADDITIONAL_ARGS+=( -DCMAKE_SYSTEM_NAME=Linux )
  CMAKE_ADDITIONAL_ARGS+=( -DCMAKE_SYSTEM_PROCESSOR="${TARGET}" )
  CMAKE_ADDITIONAL_ARGS+=( -DCMAKE_C_COMPILER=mips-mti-linux-gnu-gcc )
  CMAKE_ADDITIONAL_ARGS+=( -DCMAKE_CXX_COMPILER=mips-mti-linux-gnu-g++ )
  CMAKE_ADDITIONAL_ARGS+=( -DCMAKE_C_COMPILER_ARG1="${MIPS_FLAGS}" )
  CMAKE_ADDITIONAL_ARGS+=( -DCMAKE_CXX_COMPILER_ARG1="${MIPS_FLAGS}" )

  local -r SYSROOT_DIR=${GCC_DIR}/sysroot/${FLAVOUR}

  # Keeping only the sysroot of interest to save on travis cache.
  if [[ "${CONTINUOUS_INTEGRATION}" = "true" ]]; then
    for folder in "${GCC_DIR}"/sysroot/*; do
      if [[ "${folder}" != "${SYSROOT_DIR}" ]]; then
        rm -rf "${folder}"
      fi
    done
  fi

  local -r LIBC_DIR=${GCC_DIR}/mips-mti-linux-gnu/lib/${FLAVOUR}/${LIBC_DIR_SUFFIX}
  QEMU_ARGS+=( -L "${SYSROOT_DIR}" )
  QEMU_ARGS+=( -E LD_PRELOAD="${LIBC_DIR}/libstdc++.so.6:${LIBC_DIR}/libgcc_s.so.1" )
}

function build() {
  cd "${PROJECT_DIR}" || exit 2

  # CMake Configuration
  set -x
  cmake -S. -B"${BUILD_DIR}" "${CMAKE_DEFAULT_ARGS[@]}" "${CMAKE_ADDITIONAL_ARGS[@]}"

  # CMake Build
  #cmake --build "${BUILD_DIR}" --target host_tools -v
  cmake --build "${BUILD_DIR}" --target all -j8 -v
  set +x
}

function run_test() {
  assert_defined QEMU_ARCH
  if [[ "${QEMU_ARCH}" == "DISABLED" ]]; then
    return
  fi
  install_qemu
  RUN_CMD="${QEMU_INSTALL}/bin/qemu-${QEMU_ARCH} ${QEMU_ARGS[*]}"
  for test_binary in "${BUILD_DIR}"/test/*_test; do
    ${RUN_CMD} "${test_binary}"
  done
}

function usage() {
  local -r NAME=$(basename "$0")
  echo -e "$NAME - Build using a cross toolchain.

SYNOPSIS
\t$NAME [-h|--help]

DESCRIPTION
\tCross compile Google OR-Tools (or Glop) using a cross toolchain.

\tYou MUST define the following variables before running this script:
\t* PROJECT: glop or-tools
\t* TARGET: x86_64 aarch64-linux-gnu aarch64_be-linux-gnu mips64 mips64el

OPTIONS
\t-h --help: show this help text

EXAMPLE
export PROJECT=glop
export TARGET=aarch64-linux-gnu
$0"
}

# Main
function main() {
  if [[ -n ${1-} ]]; then
    case $1 in
      -h | --help)
        usage; exit ;;
    esac
  fi

  assert_defined PROJECT
  assert_defined TARGET

  declare -r PROJECT_DIR="$(cd -P -- "$(dirname -- "$0")/.." && pwd -P)"
  declare -r ARCHIVE_DIR="${PROJECT_DIR}/build_cross/archives"
  declare -r BUILD_DIR="${PROJECT_DIR}/build_cross/${TARGET}"
  declare -r TOOLCHAIN_FILE=${BUILD_DIR}/toolchain.cmake

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

  case ${TARGET} in
    x86_64)
      clean_build
      declare -r QEMU_ARCH=x86_64 ;;
    aarch64-linux-gnu)
      clean_build
      expand_linaro_config
      declare -r QEMU_ARCH=aarch64 ;;
    aarch64_be-linux-gnu)
      clean_build
      expand_linaro_config
      declare -r QEMU_ARCH=DISABLED ;;
    mips64)
      clean_build
      expand_codescape_config
      declare -r QEMU_ARCH=mips64 ;;
    mips64el)
      clean_build
      expand_codescape_config
      declare -r QEMU_ARCH=mips64el ;;
    *)
      >&2 echo "Unknown TARGET '${TARGET}'..."
      exit 1 ;;
  esac
  declare -r QEMU_INSTALL=${ARCHIVE_DIR}/qemu
  declare -a QEMU_ARGS=()

  if [[ -n ${1-} ]]; then
    case $1 in
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
  fi
}

main "$1"
