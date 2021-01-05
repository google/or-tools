#!/usr/bin/env bash

set -euo pipefail

function extract() {
  case $1 in
    *.tar.bz2)   tar xjf "$1"    ;;
    *.tar.xz)    tar xJf "$1"    ;;
    *.tar.gz)    tar xzf "$1"    ;;
    *)
      echo "don't know how to extract '$1'..."
      exit 1
  esac
}

function unpackifnotexists() {
  mkdir -p "${ARCHIVE_DIR}"
  cd "${ARCHIVE_DIR}" || exit
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

function installqemuifneeded() {
  local VERSION=${QEMU_VERSION:=5.2.0}
  local ARCHES=${QEMU_ARCHES:=arm aarch64 x86_64 mips64 mips64el}
  local TARGETS=${QEMU_TARGETS:=$(echo "$ARCHES" | sed 's#$# #;s#\([^ ]*\) #\1-linux-user #g')}

  if echo "${VERSION} ${TARGETS}" | cmp --silent ${QEMU_INSTALL}/.build -; then
    echo "qemu ${VERSION} up to date!"
    return 0
  fi

  echo "VERSION: ${VERSION}"
  echo "TARGETS: ${TARGETS}"

  rm -rf ${QEMU_INSTALL}

  # Checking for a tarball before downloading makes testing easier :-)
  local QEMU_URL="http://wiki.qemu-project.org/download/qemu-${VERSION}.tar.xz"
  local QEMU_DIR="qemu-${VERSION}"
  unpackifnotexists ${QEMU_URL} ${QEMU_DIR}
  cd ${QEMU_DIR} || exit

  ./configure \
    --prefix=${QEMU_INSTALL} \
    --target-list=${TARGETS} \
    --disable-docs \
    --disable-sdl \
    --disable-gtk \
    --disable-gnutls \
    --disable-gcrypt \
    --disable-nettle \
    --disable-curses \
    --static

  make -j4
  make install

  echo "$VERSION $TARGETS" > ${QEMU_INSTALL}/.build
}

function assert_defined(){
  if [[ -z "${!1}" ]]; then
    echo "Variable '${1}' must be defined"
    exit 1
  fi
}

function integrate() {
  cd "${PROJECT_DIR}"

  # CMake Configuration
  set -x
  cmake -S. -B"${BUILD_DIR}" "${DEFAULT_CMAKE_ARGS[@]}" "${CMAKE_ADDITIONAL_ARGS[@]}"

  # CMake Build
  #cmake --build "${BUILD_DIR}" --target host_tools -v
  cmake --build "${BUILD_DIR}" --target all -j8 -v
  set +x
  # Running tests if needed
  if [[ "${QEMU_ARCH}" == "DISABLED" ]]; then
    return
  fi
  RUN_CMD=""
  if [[ -n "${QEMU_ARCH}" ]]; then
    installqemuifneeded
    RUN_CMD="${QEMU_INSTALL}/bin/qemu-${QEMU_ARCH} ${QEMU_ARGS[*]}"
  fi
  CMAKE_TEST_FILES="${BUILD_DIR}/test/*_test"
  for test_binary in ${CMAKE_TEST_FILES}; do
    ${RUN_CMD} "${test_binary}"
  done
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
  unpackifnotexists "${GCC_URL}" "${GCC_RELATIVE_DIR}"

  #local -r SYSROOT_VERSION=2.25-2017.11
  local -r SYSROOT_VERSION=2.25-2019.12
  local -r SYSROOT_URL=${LINARO_ROOT_URL}/${TARGET}/sysroot-glibc-linaro-${SYSROOT_VERSION}-${TARGET}.tar.xz
  local -r SYSROOT_RELATIVE_DIR=sysroot-glibc-linaro-${SYSROOT_VERSION}-${TARGET}
  unpackifnotexists "${SYSROOT_URL}" "${SYSROOT_RELATIVE_DIR}"

  local -r SYSROOT_DIR=${ARCHIVE_DIR}/${SYSROOT_RELATIVE_DIR}
  local -r STAGING_DIR=${ARCHIVE_DIR}/${SYSROOT_RELATIVE_DIR}-stage
  local -r GCC_DIR=${ARCHIVE_DIR}/${GCC_RELATIVE_DIR}

  # Write Toolchain
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
  unpackifnotexists "${GCC_URL}" "${GCC_RELATIVE_DIR}"

  local -r GCC_DIR=${ARCHIVE_DIR}/${GCC_RELATIVE_DIR}
  local MIPS_FLAGS=""
  local FLAVOUR=""
  local LIBC_DIR_SUFFIX=""
  case "${TARGET}" in
    "mips64")    MIPS_FLAGS="-EB -mabi=64"; FLAVOUR="mips-r2-hard"; LIBC_DIR_SUFFIX="lib64" ;;
    "mips64el")  MIPS_FLAGS="-EL -mabi=64"; FLAVOUR="mipsel-r2-hard"; LIBC_DIR_SUFFIX="lib64" ;;
    *)           echo 'unknown mips platform'; exit 1;;
  esac

  CMAKE_ADDITIONAL_ARGS+=( -DCMAKE_FIND_ROOT_PATH="${GCC_DIR}" )
  CMAKE_ADDITIONAL_ARGS+=( -DCMAKE_SYSTEM_NAME=Linux )
  CMAKE_ADDITIONAL_ARGS+=( -DCMAKE_SYSTEM_PROCESSOR="${TARGET}" )
  CMAKE_ADDITIONAL_ARGS+=( -DCMAKE_C_COMPILER=mips-mti-linux-gnu-gcc )
  CMAKE_ADDITIONAL_ARGS+=( -DCMAKE_CXX_COMPILER=mips-mti-linux-gnu-g++ )
  CMAKE_ADDITIONAL_ARGS+=( -DCMAKE_C_COMPILER_ARG1="${MIPS_FLAGS}" )
  CMAKE_ADDITIONAL_ARGS+=( -DCMAKE_CXX_COMPILER_ARG1="${MIPS_FLAGS}" )

  local SYSROOT_DIR=${GCC_DIR}/sysroot/${FLAVOUR}

  # Keeping only the sysroot of interest to save on travis cache.
  if [[ "${CONTINUOUS_INTEGRATION}" = "true" ]]; then
    for folder in "${GCC_DIR}/sysroot/*"; do
      if [[ "${folder}" != "${SYSROOT_DIR}" ]]; then
        rm -rf "${folder}"
      fi
    done
  fi

  local LIBC_DIR=${GCC_DIR}/mips-mti-linux-gnu/lib/${FLAVOUR}/${LIBC_DIR_SUFFIX}
  QEMU_ARGS+=( -L "${SYSROOT_DIR}" )
  QEMU_ARGS+=( -E LD_PRELOAD="${LIBC_DIR}/libstdc++.so.6:${LIBC_DIR}/libgcc_s.so.1" )
}

# Main
declare -r usage="$(basename "$0") [-h] -- Build glop using a cross toolchain (aarch64).

where:
\t-h --help: show this help text

You must define the following variables before running this script:
\tTOOLCHAIN: LINARO CODESPACE NATIVE
\tTARGET: depending of the TOOLCHAIN chosen,
\t  LINARO: aarch64-linux-gnu aarch64_be-linux-gnu
\t  CODESPACE: mips64 mips64el
\t  NATIVE: x86_64

Optional variables:
\tQEMU_ARCH: DISABLED x86_64 aarch64 mips64 mips64el

Example:
export TOOLCHAIN=LINARO
export TARGET=aarch64-linux-gnu
export QEMU_ARCH=aarch64
$0
"

if [[ -n ${1-} ]]; then
  case $1 in
    -h | --help)
      echo -e "$usage"
      exit
      ;;
  esac
fi

assert_defined TOOLCHAIN
assert_defined TARGET

declare -r PROJECT_DIR="$(cd -P -- "$(dirname -- "$0")/.." && pwd -P)"
declare -r ARCHIVE_DIR="${PROJECT_DIR}/build_cross/archives"
declare -r BUILD_DIR="${PROJECT_DIR}/build_cross/${TARGET}"
declare -r TOOLCHAIN_FILE=${BUILD_DIR}/toolchain.cmake
declare -a DEFAULT_CMAKE_ARGS=( -DBUILD_CXX=OFF -DBUILD_GLOP=ON -DBUILD_DEPS=ON -G ${CMAKE_GENERATOR:-"Unix Makefiles"} )
declare -r QEMU_INSTALL=${ARCHIVE_DIR}/qemu
declare -r QEMU_ARCHES=${QEMU_ARCH}

echo "Project: '${PROJECT_DIR}'"
echo "Target: '${TARGET}'"
echo "Toolchain: '${TOOLCHAIN}'"

echo "Build dir: '${BUILD_DIR}'"
echo "toolchain file: '${TOOLCHAIN_FILE}'"

# Cleanup previous build
rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}"

declare -a QEMU_ARGS=()
declare -a CMAKE_ADDITIONAL_ARGS=()

case ${TOOLCHAIN} in
  LINARO)    expand_linaro_config     ;;
  CODESCAPE) expand_codescape_config  ;;
  NATIVE)    QEMU_ARCH=""             ;;
  *)         echo "Unknown toolchain '${TOOLCHAIN}'..."; exit 1;;
esac

integrate
