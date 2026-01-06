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
  echo -n "check swig..."
  command -v swig
  command -v swig | xargs echo "swig: " | tee -a build.log
  echo "DONE" | tee -a build.log

  echo -n "check dotnet..."
  command -v dotnet
  command -v dotnet | xargs echo "dotnet: " | tee -a build.log
  echo "DONE" | tee -a build.log

  # Install .Net SNK
  echo -n "Install .Net SNK..." | tee -a build.log
  local OPENSSL_PRG=openssl
  if [[ -x $(command -v openssl11) ]]; then
    OPENSSL_PRG=openssl11
  fi
  echo "DONE" | tee -a build.log
  echo -n "check ${OPENSSL_PRG}..."
  command -v ${OPENSSL_PRG} | xargs echo "openssl: " | tee -a build.log

  $OPENSSL_PRG aes-256-cbc -iter 42 -pass pass:"$ORTOOLS_TOKEN" \
    -in "${RELEASE_DIR}/or-tools.snk.enc" \
    -out "${ROOT_DIR}/export/or-tools.snk" -d
  export DOTNET_SNK=export/or-tools.snk
  echo "DONE" | tee -a build.log

  # Clean dotnet
  echo -n "Clean .Net..." | tee -a build.log
  cd "${ROOT_DIR}" || exit 2
  rm -rf "${ROOT_DIR}/temp_dotnet"
  echo "DONE" | tee -a build.log

  echo "Build .Net..." | tee -a build.log
  cmake -S. -Btemp_dotnet -DBUILD_SAMPLES=OFF -DBUILD_EXAMPLES=OFF -DBUILD_DOTNET=ON
  cmake --build temp_dotnet -j8 -v
  echo -n "  Check libortools.dylib..." | tee -a build.log
  otool -L temp_dotnet/lib/libortools.dylib | grep -vqz "/Users"
  echo "DONE" | tee -a build.log
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
  echo -n "check swig..."
  command -v swig
  command -v swig | xargs echo "swig: " | tee -a build.log
  echo "DONE" | tee -a build.log

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
    if [[ ${PLATFORM} == "arm64" ]]; then
      java -version 2>&1 | head -n 1 | grep "11\.0"
    else
      java -version 2>&1 | head -n 1 | grep "1\.8"
    fi
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

  echo "Build Java..." | tee -a build.log
  if [[ ! -v GPG_ARGS ]]; then
    GPG_EXTRA=""
  else
    GPG_EXTRA="-DGPG_ARGS=${GPG_ARGS}"
  fi
  # shellcheck disable=SC2086 # cmake fail to parse empty string ""
  cmake -S. -Btemp_java -DBUILD_SAMPLES=OFF -DBUILD_EXAMPLES=OFF \
 -DBUILD_JAVA=ON -DSKIP_GPG=OFF ${GPG_EXTRA}
  cmake --build temp_java -j8 -v
  echo -n "  Check libortools.dylib..." | tee -a build.log
  otool -L temp_java/lib/libortools.dylib | grep -vqz "/Users"
  echo "DONE" | tee -a build.log
  echo "DONE" | tee -a build.log
  #cmake --build temp_java --target test
  #echo "cmake test: DONE" | tee -a build.log

  # copy jar to export
  if [[ ${PLATFORM} == "arm64" ]]; then
    cp temp_java/java/ortools-darwin-aarch64/target/*.jar* export/
  else
    cp temp_java/java/ortools-darwin-x86-64/target/*.jar* export/
  fi
  cp temp_java/java/ortools-java/target/*.jar* export/
  echo "${ORTOOLS_BRANCH} ${ORTOOLS_SHA1}" > "${ROOT_DIR}/export/java_build"
}

# Python 3
# TODO(user) Use `make --directory tools/docker python` instead
# shellcheck disable=2317
function build_python() {
  if echo "${ORTOOLS_BRANCH} ${ORTOOLS_SHA1}" | cmp --silent "${ROOT_DIR}/export/python_build" -; then
    echo "build python up to date!" | tee -a build.log
    return 0
  fi

  # Save PATH
  PATH_BCKP=${PATH}

  cd "${ROOT_DIR}" || exit 2
  echo -n "check swig..."
  command -v swig
  command -v swig | xargs echo "swig: " | tee -a build.log
  echo "DONE" | tee -a build.log

  if [[ ${PLATFORM} == "arm64" ]]; then
    local -r PY=(3.9 3.10 3.11 3.12 3.13 3.14)
  else
    local -r PY=(3.9 3.10 3.11 3.12 3.13 3.14)
  fi

  # Check Python env
  for PY_VERSION in "${PY[@]}"; do
    # Check python interpreter
    # Need the one form python.org not from homebrew to be compatible with older macOS
    PY_PATH="/Library/Frameworks/Python.framework/Versions/${PY_VERSION}"
    if [[ ! -d "$PY_PATH" ]]; then
      echo "Error: Python ${PY_VERSION} is not found (${PY_PATH})." | tee -a build.log
      exit 1
    fi
    export PATH="${HOME}/Library/Python/${PY_VERSION}/bin:${PY_PATH}/bin:${PATH_BCKP}"

    # Check Python packages mandatory are available and up to date
    echo "check python${PY_VERSION}..."
    command -v python3 | xargs echo "python${PY_VERSION}: " | tee -a build.log
    command -v "python${PY_VERSION}" | xargs echo "python${PY_VERSION}: " | tee -a build.log
    "python${PY_VERSION}" -c "import platform as p; print(p.platform())" | tee -a build.log
    "python${PY_VERSION}" -m pip install --upgrade --user pip
    "python${PY_VERSION}" -m pip install --upgrade --user wheel absl-py mypy mypy-protobuf protobuf virtualenv
    echo "check protoc-gen-mypy..."
    command -v protoc-gen-mypy | xargs echo "protoc-gen-mypy: " | tee -a build.log
    protoc-gen-mypy --version | xargs echo "protoc-gen-mypy version: " | tee -a build.log
    protoc-gen-mypy --version | grep "4\.0\.0"
  done

  declare -a MYPY_FILES=(
    "ortools/algorithms/python/knapsack_solver.pyi"
    "ortools/constraint_solver/pywrapcp.pyi"
    "ortools/graph/python/linear_sum_assignment.pyi"
    "ortools/graph/python/max_flow.pyi"
    "ortools/graph/python/min_cost_flow.pyi"
    "ortools/init/python/init.pyi"
    "ortools/linear_solver/python/model_builder_helper.pyi"
    "ortools/linear_solver/pywraplp.pyi"
    "ortools/pdlp/python/pdlp.pyi"
    "ortools/sat/python/cp_model_helper.pyi"
    "ortools/scheduling/python/rcpsp.pyi"
    "ortools/util/python/sorted_interval_list.pyi"
  )

  for PY_VERSION in "${PY[@]}"; do
    PY_PATH="/Library/Frameworks/Python.framework/Versions/${PY_VERSION}"
    if [[ ! -d "$PY_PATH" ]]; then
      echo "Error: Python ${PY_VERSION} is not found (${PY_PATH})." | tee -a build.log
      exit 1
    fi
    export PATH="${HOME}/Library/Python/${PY_VERSION}/bin:${PY_PATH}/bin:${PATH_BCKP}"

    # Clean and build
    echo -n "Cleaning Python ${PY_VERSION}..." | tee -a build.log
    rm -rf "temp_python${PY_VERSION}"
    echo "DONE" | tee -a build.log

    echo "Build Python ${PY_VERSION}..." | tee -a build.log
    echo -n "  CMake configure..." | tee -a build.log
    cmake -S. -B"temp_python${PY_VERSION}" -DBUILD_SAMPLES=OFF -DBUILD_EXAMPLES=OFF -DBUILD_PYTHON=ON -DPython3_ROOT_DIR="$PY_PATH"
    echo "DONE" | tee -a build.log

    echo -n "  Build libortools..." | tee -a build.log
    cmake --build "temp_python${PY_VERSION}" --target ortools -j8 -v
    echo "DONE" | tee -a build.log

    if [[ ${PLATFORM} == "x86_64" ]]; then
      echo -n "  Build all few times..." | tee -a build.log
      # on macos X86_64 stubgen will timeout -> need to build few times
      cmake --build "temp_python${PY_VERSION}" -j4 -v || true
      sleep 10
      cmake --build "temp_python${PY_VERSION}" -v || true
      echo "DONE" | tee -a build.log
      echo -n "  ReBuild all..." | tee -a build.log
      cmake --build "temp_python${PY_VERSION}" -j4 -v
      echo "DONE" | tee -a build.log
    else
      echo -n "  Build all..." | tee -a build.log
      cmake --build "temp_python${PY_VERSION}" -j8 -v
      echo "DONE" | tee -a build.log
    fi

    echo -n "  Check libortools.dylib..." | tee -a build.log
    otool -L "temp_python${PY_VERSION}/lib/libortools.dylib" | grep -vqz "/Users"
    echo "DONE" | tee -a build.log
    echo "Build Python ${PY_VERSION}...DONE" | tee -a build.log
    #cmake --build temp_python${PY_VERSION} --target test
    #echo "cmake test_python${PY_VERSION}: DONE" | tee -a build.log

    # Check mypy files
    for FILE in "${MYPY_FILES[@]}"; do
      if [[ ! -f "temp_python${PY_VERSION}/python/${FILE}" ]]; then
        echo "error: ${FILE} missing in the python project" | tee -a build.log
        exit 2
      fi
    done

    cp "temp_python${PY_VERSION}"/python/dist/*.whl export/
    pushd export
    for WHEEL_FILE in *_universal2.whl; do
      if [[ ${PLATFORM} == "arm64" ]]; then
        mv "${WHEEL_FILE}" "${WHEEL_FILE%_universal2.whl}_arm64.whl"
      else
        mv "${WHEEL_FILE}" "${WHEEL_FILE%_universal2.whl}_x86_64.whl" || true
      fi
    done
    popd
  done

  # Reset PATH
  export PATH=${PATH_BCKP}

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
  echo "Check Make version..."
  make --version 2>&1 | head -n 1 | grep "\b4\.4"
  echo "Check Sed version..."
  sed --version 2>&1 | head -n 1 | grep "GNU sed.*\b4"

  echo -n "Clean previous archive..." | tee -a build.log
  make clean_archive
  echo "DONE" | tee -a build.log

  echo "Make cpp archive..." | tee -a build.log
  make archive_cpp
  echo -n "  Check libortools.dylib..." | tee -a build.log
  otool -L "build_make/lib/libortools.dylib" | grep -vqz "/Users"
  echo "DONE" | tee -a build.log
  echo "DONE" | tee -a build.log

  echo "Make dotnet archive..." | tee -a build.log
  make archive_dotnet
  echo -n "  Check libortools.dylib..." | tee -a build.log
  otool -L "build_make/lib/libortools.dylib" | grep -vqz "/Users"
  echo "DONE" | tee -a build.log
  echo "DONE" | tee -a build.log

  echo "Make java archive..." | tee -a build.log
  make archive_java
  echo -n "  Check libortools.dylib..." | tee -a build.log
  otool -L "build_make/lib/libortools.dylib" | grep -vqz "/Users"
  echo "DONE" | tee -a build.log
  echo "DONE" | tee -a build.log

  # move archive to export
  mv or-tools_*.tar.gz export/
  echo "${ORTOOLS_BRANCH} ${ORTOOLS_SHA1}" > "${ROOT_DIR}/export/archive_build"
}

# Build Examples
# shellcheck disable=2317
function build_examples() {
  if echo "${ORTOOLS_BRANCH} ${ORTOOLS_SHA1}" | cmp --silent "${ROOT_DIR}/export/examples_build" -; then
    echo "build examples up to date!" | tee -a build.log
    return 0
  fi

  cd "${ROOT_DIR}" || exit 2
  echo "Check Make version..."
  make --version 2>&1 | head -n 1 | grep "\b4\.3"
  echo "Check Sed version..."
  sed --version 2>&1 | head -n 1 | grep "GNU sed.*\b4"

  echo -n "Clean previous example archives..." | tee -a build.log
  rm -rf temp ./*.tar.gz
  echo "DONE" | tee -a build.log

  echo "Build examples archives..." | tee -a build.log

  echo -n "  Python examples archive..." | tee -a build.log
  make python_examples_archive UNIX_PYTHON_VER=3
  echo "DONE" | tee -a build.log

  echo -n "  Java examples archive..." | tee -a build.log
  make java_examples_archive UNIX_PYTHON_VER=3
  echo "DONE" | tee -a build.log

  echo -n "  .Net examples archive..." | tee -a build.log
  make dotnet_examples_archive UNIX_PYTHON_VER=3
  echo "DONE" | tee -a build.log

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
  rm -rf temp_cpp
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
}

main "${1:-all}"

