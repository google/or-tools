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
\t$NAME - Build delivery using the ${BOLD}local host system${RESET}.
${BOLD}SYNOPSIS${RESET}
\t$NAME [-h|--help|help] [examples|dotnet|java|python|all|reset]
${BOLD}DESCRIPTION${RESET}
\tBuild Google OR-Tools deliveries.
\tYou ${BOLD}MUST${RESET} define the following variables before running this script:
\t* ORTOOLS_TOKEN: secret use to decrypt keys to sign .Net and Java packages.

${BOLD}OPTIONS${RESET}
\t-h --help: display this help text
\tarchive: build all (C++, .Net, Java) archives
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

# .Net build
function build_dotnet() {
  if echo "${ORTOOLS_BRANCH} ${ORTOOLS_SHA1}" | cmp --silent "${ROOT_DIR}/export/dotnet_build" -; then
    echo "build .Net up to date!" | tee -a build.log
    return 0
  fi

  cd "${ROOT_DIR}" || exit 2
  echo "check swig..."
  command -v swig
  command -v swig | xargs echo "swig: " | tee -a build.log
  echo "check dotnet..."
  command -v dotnet
  command -v dotnet | xargs echo "dotnet: " | tee -a build.log

  # Install .Net SNK
  echo -n "Install .Net SNK..." | tee -a build.log
  local OPENSSL_PRG=openssl
  if [[ -x $(command -v openssl11) ]]; then
    OPENSSL_PRG=openssl11
  fi
  echo "check ${OPENSSL_PRG}..."
  command -v ${OPENSSL_PRG} | xargs echo "openssl: " | tee -a build.log

  $OPENSSL_PRG aes-256-cbc -iter 42 -pass pass:"$ORTOOLS_TOKEN" \
    -in "${RELEASE_DIR}/or-tools.snk.enc" \
    -out "${ROOT_DIR}/export/or-tools.snk" -d
  DOTNET_SNK=export/or-tools.snk
  echo "DONE" | tee -a build.log

  # Clean dotnet
  echo -n "Clean .Net..." | tee -a build.log
  cd "${ROOT_DIR}" || exit 2
  rm -rf "${ROOT_DIR}/temp_dotnet"
  echo "DONE" | tee -a build.log

  echo -n "Build .Net..." | tee -a build.log
  cmake -S. -Btemp_dotnet -DBUILD_SAMPLES=OFF -DBUILD_EXAMPLES=OFF -DBUILD_DOTNET=ON
  cmake --build temp_dotnet -j8 -v
  echo "DONE" | tee -a build.log
  #cmake --build temp_dotnet --target test
  #echo "cmake test: DONE" | tee -a build.log

  # copy nupkg to export
  cp temp_dotnet/dotnet/packages/*nupkg export/
  echo "${ORTOOLS_BRANCH} ${ORTOOLS_SHA1}" > "${ROOT_DIR}/export/dotnet_build"
}

# Java build
function build_java() {
  if echo "${ORTOOLS_BRANCH} ${ORTOOLS_SHA1}" | cmp --silent "${ROOT_DIR}/export/java_build" -; then
    echo "build Java up to date!" | tee -a build.log
    return 0
  fi

  cd "${ROOT_DIR}" || exit 2
  echo "check swig..."
  command -v swig
  command -v swig | xargs echo "swig: " | tee -a build.log
  # maven require JAVA_HOME
  if [[ -z "${JAVA_HOME}" ]]; then
    echo "JAVA_HOME: not found !" | tee -a build.log
    exit 1
  else
    echo "JAVA_HOME: ${JAVA_HOME}" | tee -a build.log
    echo "check java..."
    command -v java | xargs echo "java: " | tee -a build.log
    echo "check javac..."
    command -v javac | xargs echo "javac: " | tee -a build.log
    echo "check jar..."
    command -v jar | xargs echo "jar: " | tee -a build.log
    echo "check mvn..."
    command -v mvn | xargs echo "mvn: " | tee -a build.log
    echo "Check java version..."
    java -version 2>&1 | head -n 1 | xargs echo "java version: " | tee -a build.log
  fi
  # Maven central need gpg sign and we store the release key encoded using openssl
  local OPENSSL_PRG=openssl
  if [[ -x $(command -v openssl11) ]]; then
    OPENSSL_PRG=openssl11
  fi
  echo "check ${OPENSSL_PRG}..."
  command -v ${OPENSSL_PRG} | xargs echo "openssl: " | tee -a build.log
  echo "check gpg..."
  command -v gpg
  command -v gpg | xargs echo "gpg: " | tee -a build.log

  # Install Java GPG
  echo -n "Install Java GPG..." | tee -a build.log
  $OPENSSL_PRG aes-256-cbc -iter 42 -pass pass:"$ORTOOLS_TOKEN" \
  -in tools/release/private-key.gpg.enc \
  -out private-key.gpg -d
  gpg --batch --import private-key.gpg
  # Don't need to trust the key
  #expect -c "spawn gpg --edit-key "corentinl@google.com" trust quit; send \"5\ry\r\"; expect eof"

  # Install the maven settings.xml having the GPG passphrase
  mkdir -p ~/.m2
  $OPENSSL_PRG aes-256-cbc -iter 42 -pass pass:"$ORTOOLS_TOKEN" \
  -in tools/release/settings.xml.enc \
  -out ~/.m2/settings.xml -d
  echo "DONE" | tee -a build.log

  # Clean java
  echo -n "Clean Java..." | tee -a build.log
  cd "${ROOT_DIR}" || exit 2
  rm -rf "${ROOT_DIR}/temp_java"
  echo "DONE" | tee -a build.log

  echo -n "Build Java..." | tee -a build.log

  if [[ ! -v GPG_ARGS ]]; then
    GPG_EXTRA=""
  else
    GPG_EXTRA="-DGPG_ARGS=${GPG_ARGS}"
  fi

  # shellcheck disable=SC2086 # cmake fail to parse empty string ""
  cmake -S. -Btemp_java -DBUILD_SAMPLES=OFF -DBUILD_EXAMPLES=OFF \
 -DBUILD_JAVA=ON -DSKIP_GPG=OFF ${GPG_EXTRA}
  cmake --build temp_java -j8 -v
  echo "DONE" | tee -a build.log
  #cmake --build temp_java --target test
  #echo "cmake test: DONE" | tee -a build.log

  # copy jar to export
  if [[ ${PLATFORM} == "aarch64" ]]; then
    cp temp_java/java/ortools-linux-aarch64/target/*.jar* export/
  else
    cp temp_java/java/ortools-linux-x86-64/target/*.jar* export/
  fi
  cp temp_java/java/ortools-java/target/*.jar* export/
  echo "${ORTOOLS_BRANCH} ${ORTOOLS_SHA1}" > "${ROOT_DIR}/export/java_build"
}

# Python 3
# TODO(user) Use `make --directory tools/docker python` instead
function build_python() {
  if echo "${ORTOOLS_BRANCH} ${ORTOOLS_SHA1}" | cmp --silent "${ROOT_DIR}/export/python_build" -; then
    echo "build python up to date!" | tee -a build.log
    return 0
  fi

  cd "${ROOT_DIR}" || exit 2
  echo "check swig..."
  command -v swig
  command -v swig | xargs echo "swig: " | tee -a build.log

  # Check Python env
  echo "check python3..."
  command -v python3 | xargs echo "python3: " | tee -a build.log
  python3 -c "import platform as p; print(p.platform())" | tee -a build.log
  python3 -m pip install --upgrade --user --break-system-package pip
  python3 -m pip install --upgrade --user --break-system-package wheel absl-py mypy mypy-protobuf virtualenv "typing-extensions>=4.12"
  echo "check protoc-gen-mypy..."
  command -v protoc-gen-mypy | xargs echo "protoc-gen-mypy: " | tee -a build.log
  protoc-gen-mypy --version | xargs echo "protoc-gen-mypy version: " | tee -a build.log
  protoc-gen-mypy --version | grep "3\.6\.0"

  # Clean and build
  echo -n "Cleaning Python 3..." | tee -a build.log
  rm -rf "temp_python"
  echo "DONE" | tee -a build.log
  echo -n "Build Python 3..." | tee -a build.log
  cmake -S. -Btemp_python -DBUILD_SAMPLES=OFF -DBUILD_EXAMPLES=OFF -DBUILD_PYTHON=ON
  cmake --build temp_python -j8 -v
  echo "DONE" | tee -a build.log
  #cmake --build test_python --target test
  #echo "cmake test_python: DONE" | tee -a build.log

  cp temp_python/python/dist/*.whl export/

  echo "${ORTOOLS_BRANCH} ${ORTOOLS_SHA1}" > "${ROOT_DIR}/export/python_build"
}

# Create Archive
function build_archive() {
  if echo "${ORTOOLS_BRANCH} ${ORTOOLS_SHA1}" | cmp --silent "${ROOT_DIR}/export/archive_build" -; then
    echo "build archive up to date!" | tee -a build.log
    return 0
  fi

  # Clean archive
  cd "${ROOT_DIR}" || exit 2
  make clean_archive

  echo -n "Make cpp archive..." | tee -a build.log
  make archive_cpp
  echo "DONE" | tee -a build.log

  echo -n "Make dotnet archive..." | tee -a build.log
  make archive_dotnet
  echo "DONE" | tee -a build.log

  echo -n "Make java archive..." | tee -a build.log
  make archive_java
  echo "DONE" | tee -a build.log

  # move archive to export
  mv or-tools_*.tar.gz export/
  echo "${ORTOOLS_BRANCH} ${ORTOOLS_SHA1}" > "${ROOT_DIR}/export/archive_build"
}

# Build Examples
function build_examples() {
  if echo "${ORTOOLS_BRANCH} ${ORTOOLS_SHA1}" | cmp --silent "${ROOT_DIR}/export/examples_build" -; then
    echo "build examples up to date!" | tee -a build.log
    return 0
  fi

  cd "${ROOT_DIR}" || exit 2

  rm -rf temp ./*.tar.gz
  echo -n "Build examples archives..." | tee -a build.log
  echo -n "  Python examples archive..." | tee -a build.log
  make python_examples_archive UNIX_PYTHON_VER=3
  echo -n "  Java examples archive..." | tee -a build.log
  make java_examples_archive UNIX_PYTHON_VER=3
  echo -n "  .Net examples archive..." | tee -a build.log
  make dotnet_examples_archive UNIX_PYTHON_VER=3
  echo "DONE" | tee -a build.log

  # move example to export/
  mv or-tools_*_examples_*.tar.gz export/
  echo "${ORTOOLS_BRANCH} ${ORTOOLS_SHA1}" > "${ROOT_DIR}/export/examples_build"
}

# Cleaning everything
function reset() {
  echo "Cleaning everything..."

  cd "${ROOT_DIR}" || exit 2

  make clean
  rm -rf temp_dotnet
  rm -rf temp_java
  rm -rf temp_python*
  rm -rf export
  rm -f ./*.gpg
  rm -f ./*.log
  rm -f ./*.whl
  rm -f ./*.tar.gz
  rm -f ortools.snk
  echo "DONE"
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
  local -r PLATFORM=$(uname -m)

  mkdir -p "${ROOT_DIR}/export"

  case ${1} in
    dotnet|java|python|archive|examples)
      "build_$1"
      exit ;;
    reset)
      reset
      exit ;;
    all)
      build_dotnet
      build_java
      #build_python
      build_archive
      #build_examples
      exit ;;
    *)
      >&2 echo "Target '${1}' unknown"
      exit 1
  esac
  exit 0
}

main "${1:-all}"

