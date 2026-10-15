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
\t$NAME - Publish delivery using the ${BOLD}local host system${RESET}.
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

${BOLD}EXAMPLES${RESET}
Using export to define the ${BOLD}ORTOOLS_TOKEN${RESET} env and only publishing the Java packages:
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
  echo "$@" | tee -a "${ROOT_DIR}/publish.log"
}

# Java publish
# shellcheck disable=SC2329
function publish_java() {
  if echo "${ORTOOLS_BRANCH} ${ORTOOLS_SHA1}" | cmp --silent "${ROOT_DIR}/export/java_publish" -; then
    echo "publish Java up to date!"
    return 0
  fi

  # maven require JAVA_HOME
  if [[ -z "${JAVA_HOME}" ]]; then
    log "JAVA_HOME: not found !"
    exit 1
  else
    log "JAVA_HOME: ${JAVA_HOME}"
    echo "check java..."
    command -v java | xargs echo "java: " | tee -a publish.log
    echo "check javac..."
    command -v javac | xargs echo "javac: " | tee -a publish.log
    echo "check jar..."
    command -v jar | xargs echo "jar: " | tee -a publish.log
    echo "check mvn..."
    command -v mvn | xargs echo "mvn: " | tee -a publish.log
    echo "Check java version..."
    java -version 2>&1 | head -n 1 | xargs echo "java version: " | tee -a publish.log
    java -version 2>&1 | head -n 1 | grep -q '"2[1567]'
  fi
  # Maven central need gpg sign and we store the release key encoded using openssl
  local OPENSSL_PRG=openssl
  if [[ -x $(command -v openssl11) ]]; then
    OPENSSL_PRG=openssl11
  fi
  echo "check ${OPENSSL_PRG}..."
  command -v "$OPENSSL_PRG" | xargs echo "openssl: " | tee -a publish.log
  echo "check gpg..."
  command -v gpg
  command -v gpg | xargs echo "gpg: " | tee -a publish.log

  # Install Java GPG
  log -n "Install Java GPG..."
  $OPENSSL_PRG aes-256-cbc -iter 42 -pass pass:"$ORTOOLS_TOKEN" \
  -in tools/release/private-key.gpg.enc \
  -out private-key.gpg -d
  gpg --batch --import private-key.gpg

  # Install the maven settings.xml having the GPG passphrase
  mkdir -p ~/.m2
  $OPENSSL_PRG aes-256-cbc -iter 42 -pass pass:"$ORTOOLS_TOKEN" \
  -in tools/release/settings.xml.enc \
  -out ~/.m2/settings.xml -d
  log "DONE"

  log "Build Java..."
  if [ -z "${GPG_ARGS}" ]; then
    GPG_EXTRA=""
  else
    GPG_EXTRA="-DGPG_ARGS=${GPG_ARGS}"
  fi
  # shellcheck disable=SC2086 # cmake fail to parse empty string ""
  cmake -S. -Btemp_java -DBUILD_SAMPLES=OFF -DBUILD_EXAMPLES=OFF \
 -DBUILD_JAVA=ON -DSKIP_GPG=OFF ${GPG_EXTRA}
  cmake --build temp_java --target java_native_package
  log "DONE"

  log -n "  Check libortools.dylib..."
  otool -L temp_java/lib/libortools.dylib | grep -vqz "/Users"
  log "DONE"

  log -n "Publish native Java..."
  cmake --build temp_java --target java_native_deploy -v
  log "DONE"

  echo "${ORTOOLS_BRANCH} ${ORTOOLS_SHA1}" > "${ROOT_DIR}/export/java_publish"
}

# Python publish
# shellcheck disable=SC2329
function publish_python() {
  if echo "${ORTOOLS_BRANCH} ${ORTOOLS_SHA1}" | cmp --silent "${ROOT_DIR}/export/python_publish" -; then
    echo "publish Python up to date!"
    return 0
  fi

  command -v twine
  command -v twine | xargs echo "twine: " | tee -a publish.log

  log -n "Publish Python..."
  pushd export
  twine upload "*.whl"
  popd
  log "DONE"

  echo "${ORTOOLS_BRANCH} ${ORTOOLS_SHA1}" > "${ROOT_DIR}/export/python_publish"
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

  mkdir -p "${ROOT_DIR}/export"

  case ${1} in
    java|python)
      "publish_$1"
      exit ;;
    *)
      >&2 echo "Target '${1}' unknown"
      exit 1
  esac
}

main "${1:-help}"

