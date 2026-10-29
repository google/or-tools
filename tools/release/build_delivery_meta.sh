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
\t$NAME - Build Java and .Net meta package delivery using the ${BOLD}local host system${RESET}.
${BOLD}SYNOPSIS${RESET}
\t$NAME [-h|--help|help] [dotnet|java|reset]
${BOLD}DESCRIPTION${RESET}
\tBuild Google OR-Tools meta-package deliveries.
\tYou ${BOLD}MUST${RESET} define the following variables before running this script:
\t* ORTOOLS_TOKEN: secret use to decrypt keys to sign .Net and Java packages.

${BOLD}OPTIONS${RESET}
\t-h --help: display this help text
\tdotnet: build the meta .Net package
\tjava: build the meta Java package

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

# .Net build
# shellcheck disable=SC2329
function build_dotnet() {
  if echo "${ORTOOLS_BRANCH} ${ORTOOLS_SHA1}" | cmp --silent "${ROOT_DIR}/export_meta/meta_dotnet_build" -; then
    log "build .Net up to date!"
    return 0
  fi

  cd "${ROOT_DIR}" || exit 2
  echo -n "check swig..."
  command -v swig | xargs echo "swig: " | tee -a build.log
  log "DONE"

  echo -n "check dotnet..."
  command -v dotnet | xargs echo "dotnet: " | tee -a build.log
  log "DONE"

  # Install .Net SNK
  log -n "Install .Net SNK..."
  local OPENSSL_PRG=openssl
  if [[ -x $(command -v openssl11) ]]; then
    OPENSSL_PRG=openssl11
  fi
  log "DONE"
  echo -n "check ${OPENSSL_PRG}..."
  command -v "${OPENSSL_PRG}" | xargs echo "openssl: " | tee -a build.log

  $OPENSSL_PRG aes-256-cbc -iter 42 -pass pass:"$ORTOOLS_TOKEN" \
    -in "${RELEASE_DIR}/or-tools.snk.enc" \
    -out "${ROOT_DIR}/export_meta/or-tools.snk" -d
  export DOTNET_SNK=export_meta/or-tools.snk
  log "DONE"

  log "Clear dotnet local package cache..."
  dotnet nuget locals all --clear

  # Clean dotnet
  log -n "Clean .Net..."
  cd "${ROOT_DIR}" || exit 2
  rm -rf "${ROOT_DIR}/temp_meta_dotnet"
  log "DONE"

  log -n "Build .Net..."
  cmake -S. -Btemp_meta_dotnet -DBUILD_SAMPLES=OFF -DBUILD_EXAMPLES=OFF \
  -DBUILD_DOTNET=ON -DUNIVERSAL_DOTNET_PACKAGE=ON
  cp "${ROOT_DIR}"/export/Google.OrTools.runtime.*.nupkg "${ROOT_DIR}/temp_meta_dotnet/dotnet/packages/"
  cmake --build temp_meta_dotnet
  log "DONE"
  #cmake --build temp_meta_dotnet --target test
  #echo "cmake test: DONE" | tee -a build.log

  # copy nupkg to export
  cp "${ROOT_DIR}"/temp_meta_dotnet/dotnet/packages/Google.OrTools."${OR_TOOLS_MAJOR}"."${OR_TOOLS_MINOR}".*nupkg "${ROOT_DIR}/export_meta/"
  echo "${ORTOOLS_BRANCH} ${ORTOOLS_SHA1}" > "${ROOT_DIR}/export_meta/meta_dotnet_build"
}

# Java build
# shellcheck disable=SC2329
function build_java() {
  if echo "${ORTOOLS_BRANCH} ${ORTOOLS_SHA1}" | cmp --silent "${ROOT_DIR}/export_meta/meta_java_build" -; then
    log "build Java up to date!"
    return 0
  fi

  cd "${ROOT_DIR}" || exit 2
  echo -n "check swig..."
  command -v swig | xargs echo "swig: " | tee -a build.log
  log "DONE"

  # maven require JAVA_HOME
  if [[ -z "${JAVA_HOME}" ]]; then
    log "JAVA_HOME: not found !"
    exit 1
  else
    log "JAVA_HOME: ${JAVA_HOME}"
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
    java -version 2>&1 | head -n 1 | grep -q '"2[1567]'
  fi
  # Maven central need gpg sign and we store the release key encoded using openssl
  local OPENSSL_PRG=openssl
  if [[ -x $(command -v openssl11) ]]; then
    OPENSSL_PRG=openssl11
  fi
  echo "check ${OPENSSL_PRG}..."
  command -v "${OPENSSL_PRG}" | xargs echo "openssl: " | tee -a build.log
  echo "check gpg..."
  command -v gpg | xargs echo "gpg: " | tee -a build.log

  # Install Java GPG
  log -n "Install Java GPG..."
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
  log "DONE"

  log "Clear maven local package cache..."
  rm -rf ~/.m2/repository/com/google/ortools

  log "Install native jar packages..."
  for f in "${ROOT_DIR}"/export/ortools-*.jar; do
    case "$f" in
      *-sources.jar | *-javadoc.jar | *ortools-java-*) ;;
      *) mvn install:install-file -Dfile="$f" ;;
    esac
  done
  log "DONE"

  # Clean java
  log -n "Clean Java..."
  cd "${ROOT_DIR}" || exit 2
  rm -rf "${ROOT_DIR}/temp_meta_java"
  log "DONE"

  log "Build Java..."
  if [ -z "${GPG_ARGS}" ]; then
    GPG_EXTRA=""
  else
    GPG_EXTRA="-DGPG_ARGS=${GPG_ARGS}"
  fi
  log "  CMake configure..."
  # shellcheck disable=SC2086 # cmake fail to parse empty string ""
  cmake -S. -Btemp_meta_java -DBUILD_SAMPLES=OFF -DBUILD_EXAMPLES=OFF \
 -DBUILD_JAVA=ON -DUNIVERSAL_JAVA_PACKAGE=ON \
 -DSKIP_GPG=OFF ${GPG_EXTRA}
  log "  DONE"
  log "  CMake build..."
  cmake --build temp_meta_java --config Release --target java_package
  log "  DONE"
  log "DONE"
  # copy meta jar to export
  cp temp_meta_java/java/ortools-java/target/*.jar* export_meta/
  echo "${ORTOOLS_BRANCH} ${ORTOOLS_SHA1}" > "${ROOT_DIR}/export_meta/meta_java_build"
}

# Cleaning everything
function reset() {
  echo "Cleaning everything..."
  cd "${ROOT_DIR}" || exit 2
  rm -rf temp_meta_dotnet
  rm -rf temp_meta_java
  rm -rf export_meta
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

  mkdir -p "${ROOT_DIR}/export_meta"

  case ${1} in
    dotnet|java)
      "build_$1"
      exit ;;
    reset)
      reset
      exit ;;
    *)
      >&2 echo "Target '${1}' unknown"
      exit 1
  esac
}

main "${1:-help}"

