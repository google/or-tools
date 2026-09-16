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
\t$NAME - Test delivery using the ${BOLD}arm64 manylinux${RESET} docker.
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

function test_cpp() {
  echo "TODO" | tee -a test.log
}

function test_dotnet() {
  echo "TODO" | tee -a test.log
}

function test_java() {
  echo "TODO" | tee -a test.log
}

function test_python() {
  if [ -z "$1" ]; then
    >&2 echo "No python version supplied"
    exit 1
  fi
  echo "TODO" | tee -a test.log
}

# Main
function main() {
  case ${1} in
    -h | --help | help)
      help; exit ;;
  esac

  local -r ARCH=$(uname -m)
  echo "ARCH: '${ARCH}'" | tee -a test.log
  local -r OS=$(uname -s)
  echo "OS: '${OS}'" | tee -a test.log

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

