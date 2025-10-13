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
\t$NAME - Publish delivery using an ${BOLD}Ubuntu 18.04 LTS docker image${RESET}.
${BOLD}SYNOPSIS${RESET}
\t$NAME [-h|--help] [java|python]
${BOLD}DESCRIPTION${RESET}
\tPublish Google OR-Tools deliveries.
\tYou ${BOLD}MUST${RESET} define the following variables before running this script:
\t* ORTOOLS_TOKEN: secret use to decrypt key to sign Java package.

${BOLD}OPTIONS${RESET}
\t-h --help: display this help text
\tjava: publish the Java runtime packages
\tpython: publish all Pyhon packages
\tall: publish everything (default)

${BOLD}EXAMPLES${RESET}
Using export to define the ${BOLD}ORTOOLS_TOKEN${RESET} env and only publishing the Java packages:
export ORTOOLS_TOKEN=SECRET
$0 java

note: the 'export ORTOOLS_TOKEN=...' should be placed in your bashrc to avoid any leak
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

function publish_delivery() {
  assert_defined ORTOOLS_BRANCH
  assert_defined ORTOOLS_SHA1
  assert_defined ORTOOLS_TOKEN
  assert_defined ORTOOLS_DELIVERY
  assert_defined DOCKERFILE
  assert_defined ORTOOLS_IMG

  # Clean
  echo -n "Remove previous docker images..." | tee -a "${ROOT_DIR}/publish.log"
  docker image rm -f "${ORTOOLS_IMG}":"publish_${ORTOOLS_DELIVERY}" 2>/dev/null
  echo "DONE" | tee -a "${ROOT_DIR}/publish.log"

  cd "${RELEASE_DIR}" || exit 2

  # Publish delivery
  echo -n "Build ${ORTOOLS_IMG}:publish_${ORTOOLS_DELIVERY}..." | tee -a "${ROOT_DIR}/publish.log"
  docker buildx build --platform linux/arm64 \
    --tag "${ORTOOLS_IMG}":"publish_${ORTOOLS_DELIVERY}" \
    --build-arg ORTOOLS_GIT_BRANCH="${ORTOOLS_BRANCH}" \
    --build-arg ORTOOLS_GIT_SHA1="${ORTOOLS_SHA1}" \
    --build-arg ORTOOLS_TOKEN="${ORTOOLS_TOKEN}" \
    --build-arg ORTOOLS_DELIVERY="${ORTOOLS_DELIVERY}" \
    --target=publish \
    -f "${DOCKERFILE}" .
  echo "DONE" | tee -a "${ROOT_DIR}/publish.log"
}

# Java publish
function publish_java() {
  local -r ORTOOLS_DELIVERY=java
  publish_delivery
}

# Python publish
function publish_python() {
  local -r ORTOOLS_DELIVERY=python
  publish_delivery
}

# Main
function main() {
  case ${1} in
    -h | --help)
      help; exit ;;
  esac

  assert_defined ORTOOLS_TOKEN
  echo "ORTOOLS_TOKEN: FOUND" | tee publish.log

  local -r ROOT_DIR="$(cd -P -- "$(dirname -- "$0")/../.." && pwd -P)"
  echo "ROOT_DIR: '${ROOT_DIR}'" | tee -a publish.log

  local -r RELEASE_DIR="$(cd -P -- "$(dirname -- "$0")" && pwd -P)"
  echo "RELEASE_DIR: '${RELEASE_DIR}'" | tee -a publish.log

  (cd "${ROOT_DIR}" && make print-OR_TOOLS_VERSION | tee -a publish.log)

  local -r ORTOOLS_BRANCH=$(git rev-parse --abbrev-ref HEAD)
  local -r ORTOOLS_SHA1=$(git rev-parse --verify HEAD)
  local -r DOCKERFILE="arm64.Dockerfile"
  local -r ORTOOLS_IMG="ortools/manylinux_delivery_arm64"
  local -r PLATFORM=$(uname -m)

  mkdir -p "${ROOT_DIR}/export"

  case ${1} in
    java|python)
      "publish_$1"
      exit ;;
    all)
      #publish_dotnet
      publish_java
      #publish_python
      exit ;;
    *)
      >&2 echo "Target '${1}' unknown"
      exit 1
  esac
  exit 0
}

main "${1:-all}"

