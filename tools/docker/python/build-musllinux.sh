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

# Build all the wheel artifacts for the platforms supported by musllinux_1_2 and
# export them to the specified location.
set -exo pipefail

function assert_defined(){
  if [[ -z "${!1}" ]]; then
    >&2 echo "Variable '${1}' must be defined"
    exit 1
  fi
}

function log() {
  echo "$@" | tee -a "${ROOT_DIR}/build.log"
}

function usage() {
  local -r NAME=$(basename "$0")
  echo -e "$NAME - Build using a cross toolchain.

SYNOPSIS
\t$NAME [-h|--help|help] [build|test|all]

DESCRIPTION
\tBuild wheel artifacts.

\tYou MUST define the following variables before running this script:
\t* PLATFORM: x86_64 aarch64
\t* PYTHON_VERSION: 310 311 312 313 314 314t 315 315t

OPTIONS
\t-h --help: show this help text
\tbuild: build the project using each python (note: remove previous build dir)
\ttest: install each wheel in a venv then test it (note: don't build !)
\tall: build + test (default)

EXAMPLES
* Using export
export PLATFORM=x86_64
export PYTHON_VERSION=312
$0 build

* One-liner:
PLATFORM=x86_64 PYTHON_VERSION=312 $0 build"
}

function contains_element() {
  # Look for the presence of an element in an array. Echoes '0' if found,
  # '1' otherwise.
  # Arguments:
  #   $1 the element to be searched
  #   $2 the array to search into
  local e match="$1"
  shift
  for e; do
    [[ "$e" == "$match" ]] && return 0
  done
  return 1
}

function build_wheel() {
  assert_defined BUILD_DIR
  assert_defined VENV_DIR
  # Build the wheel artifact
  # Arguments:
  #   $1 the python root directory
  if [[ "$#" -ne 1 ]]; then
    log "$0 called with an illegal number of parameters"
    exit 1
  fi

  # Create and activate virtualenv
  # this is needed so protoc can call the correct python executable
  local -r PYBIN="$1/bin"
  "${PYBIN}/pip" install virtualenv
  "${PYBIN}/virtualenv" -p "${PYBIN}/python" "${VENV_DIR}"
  # shellcheck source=/dev/null
  source "${VENV_DIR}/bin/activate"
  pip install -U pip setuptools wheel absl-py  # absl-py is needed by make test_python
  pip install -U mypy mypy-protobuf  # need to generate protobuf mypy files

  log "current dir: $(pwd)"

  if [[ ! -e "CMakeLists.txt" ]] || [[ ! -d "cmake" ]]; then
    >&2 echo "Can't find project's CMakeLists.txt or cmake"
    exit 2
  fi
  log -n "CMake configure..."
  cmake -S. -B"${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_DEPS=ON -DBUILD_PYTHON=ON -DPython3_ROOT_DIR="$1" \
    -DBUILD_TESTING=OFF -DBUILD_SAMPLES=OFF -DBUILD_EXAMPLES=OFF #--debug-find
  log "DONE"

  log -n "CMake build..."
  cmake --build "${BUILD_DIR}" -v -j4
  log "DONE"

  # Restore environment
  deactivate
}

function check_wheel() {
  assert_defined BUILD_DIR
  # Check the wheel artifact
  # Arguments:
  #   $1 the python root directory
  if [[ "$#" -ne 1 ]]; then
    log "$0 called with an illegal number of parameters"
    exit 1
  fi

  # Check mypy files
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
    "ortools/set_cover/python/set_cover.pyi"
    "ortools/util/python/sorted_interval_list.pyi"
  )
  for FILE in "${MYPY_FILES[@]}"; do
    if [[ ! -f "${BUILD_DIR}/python/${FILE}" ]]; then
      log "error: ${FILE} missing in the python project"
      exit 1
    fi
  done

  # Check all generated wheel packages
  pushd "${BUILD_DIR}/python/dist"
  for FILE in *.whl; do
    # if no files found do nothing
    [[ -e "$FILE" ]] || continue
    auditwheel show "$FILE" || true
    auditwheel -v repair --plat "musllinux_1_2_$PLATFORM" "$FILE" -w "$export_root"
    #auditwheel -v repair --plat musllinux_1_2_x86_64 "$FILE" -w "$export_root"
    #auditwheel -v repair --plat musllinux_1_2_aarch64 "$FILE" -w "$export_root"
  done
  popd
}

function test_wheel() {
  assert_defined BUILD_DIR
  assert_defined TEST_DIR
  # Test the wheel artifacts
  # Arguments:
  #   $1 the python root directory
  if [[ "$#" -ne 1 ]]; then
    log "$0 called with an illegal number of parameters"
    exit 1
  fi

  # Create and activate virtualenv
  local -r PYBIN="$1/bin"
  "${PYBIN}/pip" install virtualenv
  "${PYBIN}/virtualenv" -p "${PYBIN}/python" "${TEST_DIR}"

  # shellcheck source=/dev/null
  source "${TEST_DIR}/bin/activate"
  pip install -U pip setuptools wheel

  # Install the wheel artifact
  #pwd
  local -r WHEEL_FILE=$(find "${BUILD_DIR}"/python/dist/*.whl | head -1)
  log "WHEEL file: ${WHEEL_FILE}"
  pip install --no-cache-dir "$WHEEL_FILE"
  pip show ortools

  # Python scripts to be used as tests for the installed wheel. This list of files
  # has been taken from the 'test_python' make target.
  declare -a TESTS=(
    "ortools/algorithms/samples/simple_knapsack_program.py"
    "ortools/graph/samples/simple_max_flow_program.py"
    "ortools/graph/samples/simple_min_cost_flow_program.py"
    "ortools/linear_solver/samples/simple_lp_program.py"
    "ortools/linear_solver/samples/simple_mip_program.py"
    "ortools/sat/samples/simple_sat_program.py"
    "ortools/routing/samples/tsp.py"
    "ortools/routing/samples/vrp.py"
    "ortools/routing/samples/cvrptw_break.py"
  )

  # Run all the specified test scripts using the current environment.
  pushd "$(mktemp -d)" # ensure we are not importing something from $PWD
  python --version
  for TEST in "${TESTS[@]}"; do
    python "${ROOT_DIR}/${TEST}"
  done
  popd

  # Restore environment
  deactivate
}

function build() {
  local -r PYROOT=$(echo /opt/python/cp3*-cp"${PYTHON_VERSION}")
  log "PYROOT: $PYROOT"
  local -r PYTAG=$(basename "$PYROOT")
  log "PYTAG: $PYTAG"

  # Check for platforms to be skipped
  if contains_element "$PYTAG" "${SKIPS[@]}"; then
    log "skipping deprecated platform $PYTAG"
    exit
  fi

  BUILD_DIR="build_${PYTAG}"
  VENV_DIR="env_${PYTAG}"
  build_wheel "$PYROOT"
  check_wheel "$PYROOT"
}

function tests() {
  local -r PYROOT=$(echo /opt/python/cp3*-cp"${PYTHON_VERSION}")
  log "PYROOT: $PYROOT"
  local -r PYTAG=$(basename "$PYROOT")
  log "PYTAG: $PYTAG"

  # Check for platforms to be skipped
  if contains_element "$PYTAG" "${SKIPS[@]}"; then
    log "skipping deprecated platform $PYTAG"
    continue
  fi

  BUILD_DIR="build_${PYTAG}"
  TEST_DIR="test_${PYTAG}"
  test_wheel "$PYROOT"
}

# Main
function main() {
  case ${1} in
    -h | --help | help)
      usage; exit ;;
  esac

  local -r ROOT_DIR="$(pwd)"
  log "ROOT_DIR: '${ROOT_DIR}'"

  assert_defined PLATFORM
  log "PLATFORM: '${PLATFORM}'"
  assert_defined PYTHON_VERSION
  log "PYTHON_VERSION: '${PYTHON_VERSION}'"

  # shellcheck source=/dev/null
  source "${ROOT_DIR}/Version.txt"
  assert_defined OR_TOOLS_MAJOR
  assert_defined OR_TOOLS_MINOR
  log "ORTOOLS_VERSION: '${OR_TOOLS_MAJOR}.${OR_TOOLS_MINOR}'"

  local -r ORTOOLS_BRANCH=$(git rev-parse --abbrev-ref HEAD)
  log "ORTOOLS_BRANCH: '${ORTOOLS_BRANCH}'"
  local -r ORTOOLS_SHA1=$(git rev-parse --verify HEAD)
  log "ORTOOLS_SHA1: '${ORTOOLS_SHA1}'"

  # Setup
  declare -a SKIPS=( "cp38-cp38" "cp39-cp39" )

  case ${1} in
    build)
      build
      exit ;;
    test)
      tests
      exit ;;
    *)
      >&2 echo "Target '${1}' unknown"
      exit 1
  esac
}

main "${1:-help}"
