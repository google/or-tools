#!/usr/bin/env bash
# Copyright 2010-2025 Google LLC
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

function help() {
  local -r NAME=$(basename "$0")
  local -r BOLD="\e[1m"
  local -r RESET="\e[0m"
  local -r help=$(cat << EOF
${BOLD}NAME${RESET}
\t$NAME - Build delivery using a ${BOLD}Manylinux 2.28 aarch64 docker image${RESET}.
${BOLD}SYNOPSIS${RESET}
\t$NAME [-h|--help|help] [env|devel|cpp|dotnet|java|julia|python X.Y|reset]
${BOLD}DESCRIPTION${RESET}
\tBuild Google OR-Tools deliveries.
\tYou ${BOLD}MUST${RESET} define the following variables before running this script:
\t* ORTOOLS_TOKEN: secret use to decrypt keys to sign .Net and Java packages.

${BOLD}OPTIONS${RESET}
\t-h --help: display this help text (default)
\tenv: build env stage
\tdevel: build devel stage
\tcpp: build C++ (CMake based) prebuilt archive
\tdotnet: build all .Net packages
\tjava: build all Java packages
\tjulia: build Julia package
\tpython <X.Y>: build Pyhon X.Y package

${BOLD}EXAMPLES${RESET}
Using export to define the ${BOLD}ORTOOLS_TOKEN${RESET} env and only building the Java packages:
export ORTOOLS_TOKEN=SECRET
$0 java

note: the 'export ...' should be placed in your bashrc to avoid any leak
of the secret in your bash history
EOF
)
  echo -e "$help"
}

function assert_defined(){
  if [[ -z "${!1}" ]]; then
    >&2 echo "Variable '${1}' must be defined"
    exit 1
  fi
}

function log() {
  echo "$@" | tee -a "${ROOT_DIR}/build.log"
}

# shellcheck disable=SC2329
function build_env() {
  assert_defined ARCH
  assert_defined ORTOOLS_IMG
  assert_defined ORTOOLS_BRANCH
  assert_defined ORTOOLS_SHA1
  assert_defined DOCKERFILE

  # Enable docker over QEMU support
  if [[ ${ARCH} != "arm64" && ${ARCH} != "aarch64" ]]; then
    docker run --rm --privileged multiarch/qemu-user-static --reset -p yes
  fi
  cd "${RELEASE_DIR}" || exit 2

  log -n "Remove previous docker image..."
  docker image rm -f "${ORTOOLS_IMG}":env 2>/dev/null
  log "DONE"

  # Build env
  # https://github.com/containerd/containerd/blob/v1.4.3/platforms/platforms.go#L63
  log -n "Build ${ORTOOLS_IMG}:env..."
  docker buildx build --platform linux/arm64 \
    --tag "${ORTOOLS_IMG}":env \
    --build-arg ORTOOLS_GIT_BRANCH="${ORTOOLS_BRANCH}" \
    --build-arg ORTOOLS_GIT_SHA1="${ORTOOLS_SHA1}" \
    --target=env \
    -f "${DOCKERFILE}" .
  log "DONE"
}

# shellcheck disable=SC2329
function build_devel() {
  build_env

  cd "${RELEASE_DIR}" || exit 2

  log -n "Remove previous docker images..."
  docker image rm -f "${ORTOOLS_IMG}":devel 2>/dev/null
  log "DONE"

  # Build devel
  log -n "Build ${ORTOOLS_IMG}:devel..."
  docker buildx build --platform linux/arm64 \
    --tag "${ORTOOLS_IMG}":devel \
    --build-arg ORTOOLS_GIT_BRANCH="${ORTOOLS_BRANCH}" \
    --build-arg ORTOOLS_GIT_SHA1="${ORTOOLS_SHA1}" \
    --target=devel \
    -f "${DOCKERFILE}" .
  log "DONE"
}

# shellcheck disable=SC2329
function build_delivery() {
  build_env
  build_devel

  assert_defined ORTOOLS_TOKEN
  assert_defined ORTOOLS_DELIVERY

  cd "${RELEASE_DIR}" || exit 2

  log -n "Remove previous docker images..."
  docker image rm -f "${ORTOOLS_IMG}":"${ORTOOLS_DELIVERY}" 2>/dev/null
  log "DONE"

  # Build delivery
  log -n "Build ${ORTOOLS_IMG}:${ORTOOLS_DELIVERY}..."
  docker buildx build --platform linux/arm64 \
    --tag "${ORTOOLS_IMG}":"${ORTOOLS_DELIVERY}" \
    --build-arg ORTOOLS_GIT_BRANCH="${ORTOOLS_BRANCH}" \
    --build-arg ORTOOLS_GIT_SHA1="${ORTOOLS_SHA1}" \
    --build-arg ORTOOLS_TOKEN="${ORTOOLS_TOKEN}" \
    --build-arg ORTOOLS_DELIVERY="${ORTOOLS_DELIVERY}" \
    --target=delivery \
    -f "${DOCKERFILE}" .
  log "DONE"
}

# C++ build
# shellcheck disable=SC2329
function build_cpp() {
  if echo "${ORTOOLS_BRANCH} ${ORTOOLS_SHA1}" | cmp --silent "${ROOT_DIR}/export/arm64_cpp_build" -; then
    log "build C++ up to date!"
    return 0
  fi

  assert_defined ORTOOLS_IMG
  local -r ORTOOLS_DELIVERY=cpp
  build_delivery

  # copy tar.gz to export
  docker run --rm --init \
  -w /root/or-tools \
  -v "${ROOT_DIR}/export":/export \
  -u "$(id -u "${USER}")":"$(id -g "${USER}")" \
  -t "${ORTOOLS_IMG}":"${ORTOOLS_DELIVERY}" "cp export/*tar.gz /export/"
  echo "${ORTOOLS_BRANCH} ${ORTOOLS_SHA1}" > "${ROOT_DIR}/export/arm64_cpp_build"
}

# .Net build
# shellcheck disable=SC2329
function build_dotnet() {
  if echo "${ORTOOLS_BRANCH} ${ORTOOLS_SHA1}" | cmp --silent "${ROOT_DIR}/export/arm64_dotnet_build" -; then
    log "build .Net up to date!"
    return 0
  fi

  assert_defined ORTOOLS_IMG
  local -r ORTOOLS_DELIVERY=dotnet
  build_delivery

  # copy nupkg to export
  docker run --rm --init \
  -w /root/or-tools \
  -v "${ROOT_DIR}/export":/export \
  -u "$(id -u "${USER}")":"$(id -g "${USER}")" \
  -t "${ORTOOLS_IMG}":"${ORTOOLS_DELIVERY}" "cp export/*nupkg /export/"
  echo "${ORTOOLS_BRANCH} ${ORTOOLS_SHA1}" > "${ROOT_DIR}/export/arm64_dotnet_build"
}

# Java build
# shellcheck disable=SC2329
function build_java() {
  if echo "${ORTOOLS_BRANCH} ${ORTOOLS_SHA1}" | cmp --silent "${ROOT_DIR}/export/arm64_java_build" -; then
    log "build Java up to date!"
    return 0
  fi

  assert_defined ORTOOLS_IMG
  local -r ORTOOLS_DELIVERY=java
  build_delivery

  # copy .jar to export
  docker run --rm --init \
  -w /root/or-tools \
  -v "${ROOT_DIR}/export":/export \
  -u "$(id -u "${USER}")":"$(id -g "${USER}")" \
  -t "${ORTOOLS_IMG}":"${ORTOOLS_DELIVERY}" "cp export/*.jar* /export/"
  echo "${ORTOOLS_BRANCH} ${ORTOOLS_SHA1}" > "${ROOT_DIR}/export/arm64_java_build"
}

# Python build
# shellcheck disable=SC2329
function build_python() {
  if [ -z "$1" ]; then
    >&2 echo "No python version supplied"
    exit 1
  fi
  local -r PY_VERSION="3.$1"

  if echo "${ORTOOLS_BRANCH} ${ORTOOLS_SHA1}" | cmp --silent "${ROOT_DIR}/export/arm64_python_build" -; then
    log "build python up to date!"
    return 0
  fi

  local -r ORTOOLS_DELIVERY=python
  build_delivery

  # copy .whl to export
  docker run --rm --init \
  -w /root/or-tools \
  -v "${ROOT_DIR}/export":/export \
  -u "$(id -u "${USER}")":"$(id -g "${USER}")" \
  -t "${ORTOOLS_IMG}":"${ORTOOLS_DELIVERY}" "cp export/*.whl /export/"
  echo "${ORTOOLS_BRANCH} ${ORTOOLS_SHA1}" > "${ROOT_DIR}/export/arm64_python_build"
}

# Cleaning everything
function reset() {
  echo "Cleaning docker images..."
  assert_defined ORTOOLS_IMG
  docker image rm -f "${ORTOOLS_IMG}":cpp 2>/dev/null
  docker image rm -f "${ORTOOLS_IMG}":dotnet 2>/dev/null
  docker image rm -f "${ORTOOLS_IMG}":java 2>/dev/null
  docker image rm -f "${ORTOOLS_IMG}":python 2>/dev/null

  docker image rm -f "${ORTOOLS_IMG}":devel 2>/dev/null
  docker image rm -f "${ORTOOLS_IMG}":env 2>/dev/null
  echo "DONE"

  echo "Cleaning everything..."
  cd "${ROOT_DIR}" || exit 2
  rm -rf export
  rm -f ./*.log
  echo "DONE"
}

# Main
function main() {
  case ${1} in
    -h | --help | help)
      help; exit ;;
  esac

  local -r ROOT_DIR="$(cd -P -- "$(dirname -- "$0")/../.." && pwd -P)"
  log "ROOT_DIR: '${ROOT_DIR}'"

  local -r ARCH=$(uname -m)
  log "ARCH: '${ARCH}'"
  local -r OS=$(uname -s)
  log "OS: '${OS}'"

  # shellcheck source=/dev/null
  source "${ROOT_DIR}/Version.txt"
  assert_defined OR_TOOLS_MAJOR
  assert_defined OR_TOOLS_MINOR
  log "ORTOOLS_VERSION: '${OR_TOOLS_MAJOR}.${OR_TOOLS_MINOR}'"

  local -r ORTOOLS_BRANCH=$(git rev-parse --abbrev-ref HEAD)
  log "ORTOOLS_BRANCH: '${ORTOOLS_BRANCH}'"
  local -r ORTOOLS_SHA1=$(git rev-parse --verify HEAD)
  log "ORTOOLS_SHA1: '${ORTOOLS_SHA1}'"

  assert_defined ORTOOLS_TOKEN
  log "ORTOOLS_TOKEN: FOUND"

  local -r RELEASE_DIR="$(cd -P -- "$(dirname -- "$0")" && pwd -P)"
  log "RELEASE_DIR: '${RELEASE_DIR}'"

  local -r DOCKERFILE="arm64.Dockerfile"
  log "DOCKERFILE: '${DOCKERFILE}'"
  local -r ORTOOLS_IMG="ortools/manylinux_delivery_arm64"
  log "ORTOOLS_IMG: '${ORTOOLS_IMG}'"

  mkdir -p "${ROOT_DIR}/export"

  case ${1} in
    env|devel|cpp|dotnet|java|julia)
      "build_$1"
      exit ;;
    python)
      "build_$1" "$2"
      exit ;;
    reset)
      reset
      exit ;;
    *)
      >&2 echo "Target '${1}' unknown"
      exit 1
  esac
}

main "${1:-help}" "$2"

