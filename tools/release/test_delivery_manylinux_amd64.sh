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
\t$NAME - Test delivery using the ${BOLD}amd64 manylinux${RESET} docker.
${BOLD}SYNOPSIS${RESET}
\t$NAME [-h|--help|help] [cpp|dotnet|java|python X.Y]
${BOLD}DESCRIPTION${RESET}
\tTest Google OR-Tools deliveries.
\t* artifact must be in the current working directory.

${BOLD}OPTIONS${RESET}
\t-h --help: display this help text (default)
\tcpp: test C++ (CMake based) prebuilt archive
\tdotnet: test all .Net packages
\tjava: test all Java packages
\tpython <X.Y>: test Pyhon X.Y package

${BOLD}EXAMPLES${RESET}
$0 python 3.12
EOF
)
  echo -e "$help"
}

function log() {
  echo "$@" | tee -a "${ROOT_DIR}/test.log"
}

# shellcheck disable=SC2329
function test_cpp() {
  log "TODO"
}

# shellcheck disable=SC2329
function test_dotnet() {
  log "TODO"
}

# shellcheck disable=SC2329
function test_java() {
  log "TODO"
}

# shellcheck disable=SC2329
function test_python() {
  if [ -z "$1" ]; then
    >&2 echo "No python version supplied"
    exit 1
  fi
  log "TODO"
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

  case ${1} in
    cpp|dotnet|java)
      "test_$1"
      exit ;;
    python)
      "test_$1" "$2"
      exit ;;
    *)
      >&2 echo "Target '${1}' unknown"
      exit 1
  esac
}

main "${1:-help}" "$2"

