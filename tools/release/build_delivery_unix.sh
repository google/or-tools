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
\t$NAME [-h|--help|help] [cpp|dotnet|java|julia|python X.Y|reset]
${BOLD}DESCRIPTION${RESET}
\tBuild Google OR-Tools deliveries.
\tYou ${BOLD}MUST${RESET} define the following variables before running this script:
\t* ORTOOLS_TOKEN: secret use to decrypt keys to sign .Net and Java packages.

${BOLD}OPTIONS${RESET}
\t-h --help: display this help text (default)
\tcpp: build C++ (CMake based) prebuilt archive
\tdotnet: build all .Net packages
\tjava: build all Java packages
\tjulia: build Julia package
\tpython <X.Y>: build Pyhon X.Y package

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

# Create C++ export
# shellcheck disable=SC2329
function build_cpp() {
  if echo "${ORTOOLS_BRANCH} ${ORTOOLS_SHA1}" | cmp --silent "${ROOT_DIR}/export/cpp_build" -; then
    log "build C++ up to date!"
    return 0
  fi

  # Clean dotnet
  log -n "Clean C++..."
  cd "${ROOT_DIR}" || exit 2
  rm -rf "${ROOT_DIR}/temp_cpp"
  log "DONE"

  log -n "Build C++..."
  cmake -S. -Btemp_cpp -DBUILD_SAMPLES=OFF -DBUILD_EXAMPLES=OFF -DBUILD_DEPS=ON
  cmake --build temp_cpp --target all
  (cd temp_cpp && cpack -B pack)
  log "DONE"

  # move archive to export
  mv temp_cpp/pack/*.tar.gz export/
  echo "${ORTOOLS_BRANCH} ${ORTOOLS_SHA1}" > "${ROOT_DIR}/export/cpp_build"
}

# .Net build
# shellcheck disable=SC2329
function build_dotnet() {
  if echo "${ORTOOLS_BRANCH} ${ORTOOLS_SHA1}" | cmp --silent "${ROOT_DIR}/export/dotnet_build" -; then
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
    -out "${ROOT_DIR}/export/or-tools.snk" -d
  export DOTNET_SNK=export/or-tools.snk
  log "DONE"

  # Clean dotnet
  log -n "Clean .Net..."
  cd "${ROOT_DIR}" || exit 2
  rm -rf "${ROOT_DIR}/temp_dotnet"
  log "DONE"

  log -n "Build .Net..."
  cmake -S. -Btemp_dotnet -DBUILD_SAMPLES=OFF -DBUILD_EXAMPLES=OFF -DBUILD_DOTNET=ON
  cmake --build temp_dotnet
  log "DONE"

  if [[ "${OS}" == "Darwin" ]]; then
    log -n "  Check libortools.dylib..."
    otool -L temp_dotnet/lib/libortools.dylib | grep -vqz "/Users"
    log "DONE"
  fi
  #cmake --build temp_dotnet --target test
  #echo "cmake test: DONE" | tee -a build.log

  # copy nupkg to export
  cp temp_dotnet/dotnet/packages/*nupkg export/
  echo "${ORTOOLS_BRANCH} ${ORTOOLS_SHA1}" > "${ROOT_DIR}/export/dotnet_build"
}

# Java build
# shellcheck disable=SC2329
function build_java() {
  if echo "${ORTOOLS_BRANCH} ${ORTOOLS_SHA1}" | cmp --silent "${ROOT_DIR}/export/java_build" -; then
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

  # Install the maven settings.xml having the GPG passphrase
  mkdir -p ~/.m2
  $OPENSSL_PRG aes-256-cbc -iter 42 -pass pass:"$ORTOOLS_TOKEN" \
  -in tools/release/settings.xml.enc \
  -out ~/.m2/settings.xml -d
  log "DONE"

  # Clean java
  log -n "Clean Java..."
  cd "${ROOT_DIR}" || exit 2
  rm -rf "${ROOT_DIR}/temp_java"
  log "DONE"

  log "Build Java..."
  if [ -z "${GPG_ARGS}" ]; then
    GPG_EXTRA=""
  else
    GPG_EXTRA="-DGPG_ARGS=${GPG_ARGS}"
  fi
  log "  CMake configure..."
  # shellcheck disable=SC2086 # cmake fail to parse empty string ""
  cmake -S. -Btemp_java -DBUILD_SAMPLES=OFF -DBUILD_EXAMPLES=OFF \
 -DBUILD_JAVA=ON -DSKIP_GPG=OFF ${GPG_EXTRA}
  log "  DONE"
  log "  CMake build..."
  cmake --build temp_java --config Release --target java_native_package java_package
  log "  DONE"
  log "DONE"
  if [[ "${OS}" == "Darwin" ]]; then
    log -n "  Check libortools.dylib..."
    otool -L temp_java/lib/libortools.dylib | grep -vqz "/Users"
    log "DONE"
  fi
  # copy jar to export
  if [[ "${OS}" == "Darwin" ]]; then
    cp temp_java/java/ortools-darwin-*/target/*.jar* export/
  else
    cp temp_java/java/ortools-linux-*/target/*.jar* export/
  fi
  cp temp_java/java/ortools-java/target/*.jar* export/

  log "Generate native java bundle..."
  cmake --build temp_java --config Release --target java_native_deploy
  log "DONE"
  # copy native zip bundle to export
  if [[ "${OS}" == "Darwin" ]]; then
    cp temp_java/java/ortools-darwin-*/target/central-publishing/*.zip export/
  else
    cp temp_java/java/ortools-linux-*/target/central-publishing/*.zip export/
  fi

  echo "${ORTOOLS_BRANCH} ${ORTOOLS_SHA1}" > "${ROOT_DIR}/export/java_build"
}

# shellcheck disable=SC2329
function build_julia() {
  if echo "${ORTOOLS_BRANCH} ${ORTOOLS_SHA1}" | cmp --silent "${ROOT_DIR}/export/julia_build" -; then
    log "build Julia up to date!"
    return 0
  fi

  log -n "Cleaning Julia..."
  cd "${ROOT_DIR}" || exit 2
  rm -rf "temp_julia"
  log "DONE"

  # A Julia build is mostly a C++ build.
  # Many flags to disable features irrelevant for Julia.
  local -r JULIA_EXPORT_PKG_DIR="export/julia_pkg"
  log "Build Julia..."
  cmake -S. -Btemp_julia \
    -DBUILD_TESTING=OFF -DBUILD_SAMPLES=OFF -DBUILD_EXAMPLES=OFF \
    -DBUILD_PYTHON=OFF -DBUILD_JAVA=OFF -DBUILD_DOTNET=OFF \
    -DUSE_COINOR=OFF -DUSE_HIGHS=OFF -DUSE_SCIP=OFF -DUSE_XPRESS=OFF -DBUILD_FLATZINC=OFF \
    -DCMAKE_INSTALL_PREFIX="${JULIA_EXPORT_PKG_DIR}"
  cmake --build temp_julia
  log "Install Julia into ${JULIA_EXPORT_PKG_DIR}..."
  cmake --build temp_julia --target install
  log "DONE"

  # Make Julia package from scratch: the Julia packages and an installed
  # OR-Tools.
  cp -r ortools/julia/ORTools.jl "${JULIA_EXPORT_PKG_DIR}/"
  cp -r ortools/julia/ORToolsBinaries.jl "${JULIA_EXPORT_PKG_DIR}/"
  cp -r ortools/julia/ORToolsGenerated.jl "${JULIA_EXPORT_PKG_DIR}/"

  local -r ORTOOLS_VERSION="${OR_TOOLS_MAJOR}.${OR_TOOLS_MINOR}.0"
  local -r ARCHIVE_NAME="or-tools_julia_${ARCH}_v${ORTOOLS_VERSION}.tar.gz"
  tar --no-same-owner -czvf "export/${ARCHIVE_NAME}" -C "export" julia_pkg
  echo "${ORTOOLS_BRANCH} ${ORTOOLS_SHA1}" > "${ROOT_DIR}/export/julia_build"
}

# Python 3
# TODO(user) Use `make --directory tools/docker python` instead
# shellcheck disable=SC2317
# shellcheck disable=SC2329
function build_python() {
  if [ -z "$1" ]; then
    >&2 echo "No python version supplied"
    exit 1
  fi
  local -r PY_VERSION="3.$1"

  if echo "${ORTOOLS_BRANCH} ${ORTOOLS_SHA1}" | cmp --silent "${ROOT_DIR}/export/python${PY_VERSION}_build" -; then
    log "build python up to date!"
    return 0
  fi

  # Save PATH
  local -r PATH_BCKP=${PATH}

  cd "${ROOT_DIR}" || exit 2
  echo "check swig..."
  command -v swig | xargs echo "swig: " | tee -a build.log
  log "DONE"

  if [[ "${OS}" == "Darwin" ]]; then
    # Check python interpreter
    # Need the one form python.org not from homebrew to be compatible with older macOS
    PY_PATH="/Library/Frameworks/Python.framework/Versions/${PY_VERSION}"
    if [[ ! -d "$PY_PATH" ]]; then
      log "Error: Python ${PY_VERSION} is not found (${PY_PATH})."
      exit 1
    fi
    export PATH="${HOME}/Library/Python/${PY_VERSION}/bin:${PY_PATH}/bin:${PATH_BCKP}"
  fi

  # Check Python env
  echo "check python3..."
  command -v python3 | xargs echo "python3: " | tee -a build.log
  python3 --version | grep "${PY_VERSION}"
  command -v "python${PY_VERSION}" | xargs echo "python${PY_VERSION}: " | tee -a build.log
  python3 -c "import platform as p; print(p.platform())" | tee -a build.log
  if [[ "${OS}" == "Darwin" ]]; then
    python3 -m pip install --upgrade --user \
      pip wheel absl-py mypy mypy-protobuf protobuf virtualenv "typing-extensions>=4.12"
  else
    python3 -m pip install --upgrade --user --break-system-package \
      pip wheel absl-py mypy mypy-protobuf protobuf virtualenv "typing-extensions>=4.12"
  fi
  echo "check protoc-gen-mypy..."
  command -v protoc-gen-mypy | xargs echo "protoc-gen-mypy: " | tee -a build.log
  protoc-gen-mypy --version | xargs echo "protoc-gen-mypy version: " | tee -a build.log
  protoc-gen-mypy --version | grep "5\.1\.0"

  declare -a MYPY_FILES=(
    "ortools/algorithms/python/knapsack_solver.pyi"
    "ortools/graph/python/linear_sum_assignment.pyi"
    "ortools/graph/python/max_flow.pyi"
    "ortools/graph/python/min_cost_flow.pyi"
    "ortools/init/python/init.pyi"
    "ortools/linear_solver/python/model_builder_helper.pyi"
    "ortools/linear_solver/python/pywraplp.pyi"
    "ortools/pdlp/python/pdlp.pyi"
    "ortools/sat/python/cp_model_helper.pyi"
    "ortools/scheduling/python/rcpsp.pyi"
    "ortools/util/python/sorted_interval_list.pyi"
  )

  # Clean and build
  local -r TEMP_DIR="temp_python${PY_VERSION}"
  log -n "Cleaning Python3..."
  rm -rf "${TEMP_DIR}"
  log "DONE"
  log "Build Python3..."
  log -n "  CMake configure..."
  if [[ "${OS}" == "Darwin" ]]; then
    cmake -S. -B"${TEMP_DIR}" \
      -DBUILD_SAMPLES=OFF -DBUILD_EXAMPLES=OFF \
      -DBUILD_PYTHON=ON -DPython3_ROOT_DIR="$PY_PATH"
  else
    cmake -S. -B"${TEMP_DIR}" \
      -DBUILD_SAMPLES=OFF -DBUILD_EXAMPLES=OFF \
      -DBUILD_PYTHON=ON
  fi
  log "DONE"

  if [[ "${OS}" == "Darwin" ]]; then
    log -n "  CMake build ortools..."
    cmake --build "${TEMP_DIR}" --target ortools
    log "DONE"

    log -n "  Build all few times..."
    # on macos stubgen will timeout -> need to build few times
    cmake --build "${TEMP_DIR}" || true
    sleep 10
    cmake --build "${TEMP_DIR}" || true
    log "DONE"
    log -n "  ReBuild all..."
    cmake --build "${TEMP_DIR}"
    log "DONE"

    log -n "  Check libortools.dylib..."
    otool -L "${TEMP_DIR}/lib/libortools.dylib" | grep -vqz "/Users"
    log "DONE"
  else
    log -n "  CMake build..."
    cmake --build "${TEMP_DIR}"
    log "DONE"
  fi

  # Check mypy files
  for FILE in "${MYPY_FILES[@]}"; do
    if [[ ! -f "${TEMP_DIR}/python/${FILE}" ]]; then
      log "error: ${FILE} missing in the python project"
      exit 2
    fi
  done

  cp "${TEMP_DIR}"/python/dist/*.whl export/
  if [[ "${OS}" == "Darwin" ]]; then
    # Fix wheel naming
    pushd export
    for WHEEL_FILE in *_universal2.whl; do
      if [[ ${ARCH} == "arm64" ]]; then
        mv "${WHEEL_FILE}" "${WHEEL_FILE%_universal2.whl}_arm64.whl"
      else
        mv "${WHEEL_FILE}" "${WHEEL_FILE%_universal2.whl}_x86_64.whl" || true
      fi
    done
    popd
  fi
  # Reset PATH
  export PATH=${PATH_BCKP}

  echo "${ORTOOLS_BRANCH} ${ORTOOLS_SHA1}" > "${ROOT_DIR}/export/python${PY_VERSION}_build"
}

# Cleaning everything
function reset() {
  echo "Cleaning everything..."
  cd "${ROOT_DIR}" || exit 2
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
    cpp|dotnet|java|julia)
      "build_$1"
      exit ;;
    python)
      "build_$1" "$2"
      exit ;;
    reset)
      reset
      exit ;;
    *)
      >&2 echo "Target '${1}' unknown"
      exit 1
  esac
}

main "${1:-help}" "$2"

