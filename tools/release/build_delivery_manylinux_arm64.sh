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

function help() {
  local -r NAME=$(basename "$0")
  local -r BOLD="\e[1m"
  local -r RESET="\e[0m"
  local -r help=$(cat << EOF
${BOLD}NAME${RESET}
\t$NAME - Build delivery using an ${BOLD}Centos 7 docker image${RESET}.
${BOLD}SYNOPSIS${RESET}
\t$NAME [-h|--help|help] [examples|dotnet|java|python|all|reset]
${BOLD}DESCRIPTION${RESET}
\tBuild Google OR-Tools deliveries.
\tYou ${BOLD}MUST${RESET} define the following variables before running this script:
\t* ORTOOLS_TOKEN: secret use to decrypt keys to sign .Net and Java packages.

${BOLD}OPTIONS${RESET}
\t-h --help: display this help text
\tdotnet: build all .Net packages
\tjava: build all Java packages
\tpython: build all Pyhon packages
\texamples: build examples archives
\tall: build everything (default)

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

function build_delivery() {
  assert_defined ORTOOLS_BRANCH
  assert_defined ORTOOLS_SHA1
  assert_defined ORTOOLS_TOKEN
  assert_defined ORTOOLS_DELIVERY
  assert_defined DOCKERFILE
  assert_defined ORTOOLS_IMG

  # Enable docker over QEMU support
  docker run --rm --privileged multiarch/qemu-user-static --reset -p yes

  # Clean
  echo -n "Remove previous docker images..." | tee -a "${ROOT_DIR}/build.log"
  docker image rm -f "${ORTOOLS_IMG}":"${ORTOOLS_DELIVERY}" 2>/dev/null
  docker image rm -f "${ORTOOLS_IMG}":devel 2>/dev/null
  docker image rm -f "${ORTOOLS_IMG}":env 2>/dev/null
  echo "DONE" | tee -a "${ROOT_DIR}/build.log"

  cd "${RELEASE_DIR}" || exit 2

  # Build env
  echo -n "Build ${ORTOOLS_IMG}:env..." | tee -a "${ROOT_DIR}/build.log"
  docker build --platform linux/arm64 \
    --tag "${ORTOOLS_IMG}":env \
    --build-arg ORTOOLS_GIT_BRANCH="${ORTOOLS_BRANCH}" \
    --build-arg ORTOOLS_GIT_SHA1="${ORTOOLS_SHA1}" \
    --target=env \
    -f "${DOCKERFILE}" .
  echo "DONE" | tee -a "${ROOT_DIR}/build.log"

  # Build devel
  echo -n "Build ${ORTOOLS_IMG}:devel..." | tee -a "${ROOT_DIR}/build.log"
  docker build --platform linux/arm64 \
    --tag "${ORTOOLS_IMG}":devel \
    --build-arg ORTOOLS_GIT_BRANCH="${ORTOOLS_BRANCH}" \
    --build-arg ORTOOLS_GIT_SHA1="${ORTOOLS_SHA1}" \
    --target=devel \
    -f "${DOCKERFILE}" .
  echo "DONE" | tee -a "${ROOT_DIR}/build.log"

  # Build delivery
  echo -n "Build ${ORTOOLS_IMG}:${ORTOOLS_DELIVERY}..." | tee -a "${ROOT_DIR}/build.log"
  docker build --platform linux/arm64 \
    --tag "${ORTOOLS_IMG}":"${ORTOOLS_DELIVERY}" \
    --build-arg ORTOOLS_GIT_BRANCH="${ORTOOLS_BRANCH}" \
    --build-arg ORTOOLS_GIT_SHA1="${ORTOOLS_SHA1}" \
    --build-arg ORTOOLS_TOKEN="${ORTOOLS_TOKEN}" \
    --build-arg ORTOOLS_DELIVERY="${ORTOOLS_DELIVERY}" \
    --target=delivery \
    -f "${DOCKERFILE}" .
  echo "DONE" | tee -a "${ROOT_DIR}/build.log"
}

# .Net build
function build_dotnet() {
  if echo "${ORTOOLS_BRANCH} ${ORTOOLS_SHA1}" | cmp --silent "${ROOT_DIR}/export/arm64_dotnet_build" -; then
    echo "build .Net up to date!" | tee -a build.log
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
function build_java() {
  if echo "${ORTOOLS_BRANCH} ${ORTOOLS_SHA1}" | cmp --silent "${ROOT_DIR}/export/arm64_java_build" -; then
    echo "build Java up to date!" | tee -a build.log
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
function build_python() {
  if echo "${ORTOOLS_BRANCH} ${ORTOOLS_SHA1}" | cmp --silent "${ROOT_DIR}/export/arm64_python_build" -; then
    echo "build python up to date!" | tee -a build.log
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

# Create Archive
function build_archive() {
  if echo "${ORTOOLS_BRANCH} ${ORTOOLS_SHA1}" | cmp --silent "${ROOT_DIR}/export/arm64_archive_build" -; then
    echo "build archive up to date!" | tee -a build.log
    return 0
  fi

  local -r ORTOOLS_DELIVERY=archive
  build_delivery

  # copy archive to export
  docker run --rm --init \
  -w /root/or-tools \
  -v "${ROOT_DIR}/export":/export \
  -u "$(id -u "${USER}")":"$(id -g "${USER}")" \
  -t "${ORTOOLS_IMG}":"${ORTOOLS_DELIVERY}" "cp export/*.tar.gz /export/"
 echo "${ORTOOLS_BRANCH} ${ORTOOLS_SHA1}" > "${ROOT_DIR}/export/arm64_archive_build"
}

# Build Examples
function build_examples() {
  if echo "${ORTOOLS_BRANCH} ${ORTOOLS_SHA1}" | cmp --silent "${ROOT_DIR}/export/amd64_examples_build" -; then
    echo "build examples up to date!" | tee -a build.log
    return 0
  fi

  local -r ORTOOLS_DELIVERY=examples
  build_delivery

  echo "${ORTOOLS_BRANCH} ${ORTOOLS_SHA1}" > "${ROOT_DIR}/export/arm64_examples_build"
}

# Main
function main() {
  case ${1} in
    -h | --help | help)
      help; exit ;;
  esac

  assert_defined ORTOOLS_TOKEN
  echo "ORTOOLS_TOKEN: FOUND" | tee -a build.log

  local -r ROOT_DIR="$(cd -P -- "$(dirname -- "$0")/../.." && pwd -P)"
  echo "ROOT_DIR: '${ROOT_DIR}'" | tee -a build.log

  local -r RELEASE_DIR="$(cd -P -- "$(dirname -- "$0")" && pwd -P)"
  echo "RELEASE_DIR: '${RELEASE_DIR}'" | tee -a build.log

  (cd "${ROOT_DIR}" && make print-OR_TOOLS_VERSION | tee -a build.log)

  local -r ORTOOLS_BRANCH=$(git rev-parse --abbrev-ref HEAD)
  local -r ORTOOLS_SHA1=$(git rev-parse --verify HEAD)
  local -r DOCKERFILE="arm64.Dockerfile"
  local -r ORTOOLS_IMG="ortools/manylinux_delivery_arm64"

  mkdir -p export

  case ${1} in
    dotnet|java|python|archive|examples)
      "build_$1"
      exit ;;
    all)
      build_dotnet
      build_java
      #build_python
      #build_archive
      #build_examples
      exit ;;
    *)
      >&2 echo "Target '${1}' unknown"
      exit 1
  esac
  exit 0
}

main "${1:-all}"

