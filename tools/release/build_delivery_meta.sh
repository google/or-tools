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
\t$NAME [-h|--help|help] [dotnet|java|all|reset]
${BOLD}DESCRIPTION${RESET}
\tBuild Google OR-Tools meta-package deliveries.
\tYou ${BOLD}MUST${RESET} define the following variables before running this script:
\t* ORTOOLS_TOKEN: secret use to decrypt keys to sign .Net and Java packages.

${BOLD}OPTIONS${RESET}
\t-h --help: display this help text
\tdotnet: build the meta .Net package
\tjava: build the meta Java package
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
  if echo "${ORTOOLS_BRANCH} ${ORTOOLS_SHA1}" | cmp --silent "${ROOT_DIR}/export_meta/meta_dotnet_build" -; then
    echo "build .Net up to date!" | tee -a build.log
    return 0
  fi

  cd "${ROOT_DIR}" || exit 2
  command -v swig
  command -v swig | xargs echo "swig: " | tee -a build.log
  command -v dotnet
  command -v dotnet | xargs echo "dotnet: " | tee -a build.log

  # Install .Net SNK
  echo -n "Install .Net SNK..." | tee -a build.log
  local OPENSSL_PRG=openssl
  if [[ -x $(command -v openssl11) ]]; then
    OPENSSL_PRG=openssl11
  fi

  $OPENSSL_PRG aes-256-cbc -iter 42 -pass pass:"$ORTOOLS_TOKEN" \
    -in "${RELEASE_DIR}"/or-tools.snk.enc \
    -out "${ROOT_DIR}"/export_meta/or-tools.snk -d
  DOTNET_SNK=export_meta/or-tools.snk
  echo "DONE" | tee -a build.log

  # Clean dotnet
  echo -n "Clean .Net..." | tee -a build.log
  cd "${ROOT_DIR}" || exit 2
  rm -rf "${ROOT_DIR}/temp_meta_dotnet"
  echo "DONE" | tee -a build.log

  echo -n "Build .Net..." | tee -a build.log
  cmake -S. -Btemp_meta_dotnet -DBUILD_SAMPLES=OFF -DBUILD_EXAMPLES=OFF \
  -DBUILD_DOTNET=ON -DUSE_DOTNET_462=ON -DUNIVERSAL_DOTNET_PACKAGE=ON
  cmake --build temp_meta_dotnet -j8 -v
  echo "DONE" | tee -a build.log
  #cmake --build temp_meta_dotnet --target test
  #echo "cmake test: DONE" | tee -a build.log

  # copy nupkg to export
  cp temp_meta_dotnet/dotnet/packages/Google.OrTools.9.*nupkg export_meta/
  echo "${ORTOOLS_BRANCH} ${ORTOOLS_SHA1}" > "${ROOT_DIR}/export_meta/meta_dotnet_build"
}

# Java build
function build_java() {
  if echo "${ORTOOLS_BRANCH} ${ORTOOLS_SHA1}" | cmp --silent "${ROOT_DIR}/export_meta/meta_java_build" -; then
    echo "build Java up to date!" | tee -a build.log
    return 0
  fi

  cd "${ROOT_DIR}" || exit 2
  command -v swig
  command -v swig | xargs echo "swig: " | tee -a build.log
  # maven require JAVA_HOME
  if [[ -z "${JAVA_HOME}" ]]; then
    echo "JAVA_HOME: not found !" | tee -a build.log
    exit 1
  else
    echo "JAVA_HOME: ${JAVA_HOME}" | tee -a build.log
    command -v java | xargs echo "java: " | tee -a build.log
    command -v javac | xargs echo "javac: " | tee -a build.log
    command -v jar | xargs echo "jar: " | tee -a build.log
    command -v mvn | xargs echo "mvn: " | tee -a build.log
  fi
  # Maven central need gpg sign and we store the release key encoded using openssl
  local OPENSSL_PRG=openssl
  if [[ -x $(command -v openssl11) ]]; then
    OPENSSL_PRG=openssl11
  fi
  command -v $OPENSSL_PRG | xargs echo "openssl: " | tee -a build.log
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
  rm -rf "${ROOT_DIR}/temp_meta_java"
  echo "DONE" | tee -a build.log

  echo -n "Build Java..." | tee -a build.log

  if [[ ! -v GPG_ARGS ]]; then
    GPG_EXTRA=""
  else
    GPG_EXTRA="-DGPG_ARGS=${GPG_ARGS}"
  fi

  # shellcheck disable=SC2086: cmake fail to parse empty string ""
  cmake -S. -Btemp_meta_java -DBUILD_SAMPLES=OFF -DBUILD_EXAMPLES=OFF \
 -DBUILD_JAVA=ON -DUNIVERSAL_JAVA_PACKAGE=ON \
 -DSKIP_GPG=OFF ${GPG_EXTRA}
  cmake --build temp_meta_java -j8 -v
  echo "DONE" | tee -a build.log
  #cmake --build temp_meta_java --target test
  #echo "cmake test: DONE" | tee -a build.log

  # copy meta jar to export
  #if [ ${PLATFORM} == "aarch64" ]; then
  #  cp temp_meta_java/java/ortools-linux-aarch64/target/*.jar* export_meta/
  #else
  #  cp temp_meta_java/java/ortools-linux-x86-64/target/*.jar* export_meta/
  #fi
  cp temp_meta_java/java/ortools-java/target/*.jar* export_meta/
  echo "${ORTOOLS_BRANCH} ${ORTOOLS_SHA1}" > "${ROOT_DIR}/export_meta/meta_java_build"
}

# Cleaning everything
function reset() {
  echo "Cleaning everything..."

  cd "${ROOT_DIR}" || exit 2

  make clean
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

  assert_defined ORTOOLS_TOKEN
  echo "ORTOOLS_TOKEN: FOUND" | tee -a build.log

  local -r ROOT_DIR="$(cd -P -- "$(dirname -- "$0")/../.." && pwd -P)"
  echo "ROOT_DIR: '${ROOT_DIR}'" | tee -a build.log

  local -r RELEASE_DIR="$(cd -P -- "$(dirname -- "$0")" && pwd -P)"
  echo "RELEASE_DIR: '${RELEASE_DIR}'" | tee -a build.log

  #(cd "${ROOT_DIR}" && make print-OR_TOOLS_VERSION | tee -a build.log)

  local -r ORTOOLS_BRANCH=$(git rev-parse --abbrev-ref HEAD)
  local -r ORTOOLS_SHA1=$(git rev-parse --verify HEAD)
  local -r PLATFORM=$(uname -m)

  mkdir -p "${ROOT_DIR}/export_meta"

  case ${1} in
    dotnet|java)
      "build_$1"
      exit ;;
    reset)
      reset
      exit ;;
    all)
      build_dotnet
      build_java
      exit ;;
    *)
      >&2 echo "Target '${1}' unknown"
      exit 1
  esac
  exit 0
}

main "${1:-all}"

