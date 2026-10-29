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
\t$NAME - Test delivery using the ${BOLD}local host system${RESET}.
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

function assert_defined(){
  if [[ -z "${!1}" ]]; then
    >&2 echo "Variable '${1}' must be defined"
    exit 1
  fi
}

function log() {
  echo "$@" | tee -a "${ROOT_DIR}/test.log"
}

# shellcheck disable=SC2329
function test_cpp() {
  command -v cmake | xargs echo "cmake: " | tee -a test.log

  log "TODO"
  # untar artifact
  # Try to build and run cmake/samples/cpp
}

# shellcheck disable=SC2329
function test_dotnet() {
  command -v dotnet | xargs echo "dotnet: " | tee -a test.log
  dotnet --version | xargs echo "dotnet version: " | tee -a test.log

  log "Clear dotnet local package cache"
  dotnet nuget locals all --clear

  log "Copy exported nupkg to sample"
  cd "${ROOT_DIR}" || exit 2
  cp export/*.nupkg cmake/samples/dotnet

  log "Build .Net sample"
  cd cmake/samples/dotnet
  dotnet build --nologo -c Release

  log "Pack .Net sample"
  dotnet pack --nologo -c Release

  log "Run sample"
  dotnet run --no-build -c Release
}

# shellcheck disable=SC2329
function test_java() {
  command -v mvn | xargs echo "mvn: " | tee -a test.log
  mvn --version -q | xargs echo "mvn version: " | tee -a test.log

  log "Clear maven local package cache"
  rm -rf ~/.m2/repository/com/google/ortools

  log "Install jar packages"
  cd "${ROOT_DIR}/export" || exit 2
  rm -f ./*-sources.jar ./*-javadoc.jar
  for f in ortools-*.jar; do
    mvn install:install-file -Dfile="$f"
  done

  log "Compile Java sample"
  cd "${ROOT_DIR}/cmake/samples/java"
  mvn compile -B

  log "Package Java sample"
  mvn package -B

  log "Run sample"
  mvn exec:java

  log "Run tests sample"
  mvn test
}

# shellcheck disable=SC2329
function test_python() {
  if [ -z "$1" ]; then
    >&2 echo "No python version supplied"
    exit 1
  fi
  local -r PY_VERSION="3.$1"

  # Check Python env
  echo "check python${PY_VERSION}..."
  command -v "python${PY_VERSION}" | xargs echo "python${PY_VERSION}: " | tee -a test.log
  "python${PY_VERSION}" --version | grep "${PY_VERSION}"

  log "Creating Python${PY_VERSION} venv..."
  VENV_DIR=venv
  "python${PY_VERSION}" -m pip install --user virtualenv
  "python${PY_VERSION}" -m virtualenv "${VENV_DIR}"
  log "Creating Python${PY_VERSION} venv...DONE"

  log "Installing ortools Python${PY_VERSION} venv..."
  "${VENV_DIR}/bin/python" -m pip install export/ortools*.whl
  log "Installing ortools Python${PY_VERSION} venv...DONE"

  set +e
  log "Testing ortools Python${PY_VERSION}..."
  "${VENV_DIR}/bin/python" "cmake/samples/python/sample.py" 2>&1 | tee -a test.log
  log "Testing ortools Python${PY_VERSION}...DONE"
  set -e
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

