#!/bin/bash
# Build all the wheel artifacts for the platforms supported by manylinux1 and
# export them to the specified location.
#
# Arguments:
#   $1 the path of the or-tools sources.
#   $2 the build process root directory. If not specified, a default location is
#      used.
#   $3 the artifacts export directory. If not specified, a default location is
#      used.
#
# Environment variables:
#   SKIP_PLATFORMS manylinux1 platforms (e.g.: cp26-cp26m) to be skipped.
#   EXPORT_ROOT    if not specified at command line, this value is used as the
#                  destination path for the wheels export.
#   BUILD_ROOT     if not specified at command line, this value is used as the
#                  root path for the build process.
set -e

DEFAULT_BUILD_ROOT="$HOME"
DEFAULT_EXPORT_ROOT="${HOME}/export"

function usage() {
  echo "Usage: build-manylinux1.sh SRC_ROOT <BUILD_ROOT> <EXPORT_ROOT>"
}

function contains_element () {
    # Look for the presence of an element in an array. Echoes '0' if found,
    # '1' otherwise.
    # Arguments:
    #   $1 the element to be searched
    #   $2 the array to search into
    local e match="$1"
    shift
    for e; do [[ "$e" == "$match" ]] && echo '0' && return; done
    echo '1' && return
}

function export_manylinux_wheel {
    # Build all the wheel artifacts assuming that the current 'python' command
    # is the target interpreter. Please note that in order to have or-tools
    # correctly detect the target python version, this function should be run in
    # a virtualenv.
    # Arguments:
    #   $1 the or-tools sources root directory
    #   $2 the artifacts export directory
    if [ "$#" -ne 2 ]; then
        echo "build_pypi_archives called with an illegal number of parameters"
        exit 1  # TODO return error and check it outside
    fi
    local src_root="$1"
    local export_root="$2"
    # Build python bindings
    cd "$src_root"
    # We need to force this target, otherwise the protobuf stub will be missing
    # (for the makefile, it exists even if previously generated for another
    # platform)
    git pull
    make -B install_python_modules  # regenerates Makefile.local
    make python
    make test_python
    make pypi_archive_dir
    # Build and repair wheels
    cd temp-python*/ortools
    python setup.py bdist_wheel
    cd dist
    auditwheel repair ./*.whl -w "$export_root"
}

function test_installed {
    # Run all the specified test scripts using the current environment.
    # Arguments:
    #   $@ the test files to be tested
    local testfiles=("${@}")
    cd "$(mktemp -d)" # ensure we are not importing something from $PWD
    for testfile in "${testfiles[@]}"
    do
        python "$testfile"
    done
}

###############################################################################
# Setup

if [ -z "$1" ]; then
  (>&2 usage)
  exit 1
fi

SRC_ROOT="$1";
if [ -n "$2" ]; then BUILD_ROOT="$2"; fi
if [ -n "$3" ]; then EXPORT_ROOT="$3"; fi

if [ ! -d "$SRC_ROOT" ]; then
    (>&2 echo "Can't find or-tools sources at the specified location: $SRC_ROOT")
    exit 1
fi

if [ -z "$BUILD_ROOT" ]; then
    (>&2 echo "\$BUILD_ROOT is not set, using default location: $DEFAULT_BUILD_ROOT")
    BUILD_ROOT="$DEFAULT_BUILD_ROOT"
fi

if [ -z "$EXPORT_ROOT" ]; then
    (>&2 echo "\$EXPORT_ROOT is not set, using default location: $DEFAULT_EXPORT_ROOT")
    EXPORT_ROOT="$DEFAULT_EXPORT_ROOT"
fi

# Split SKIP_PLATFORMS string and put values into an array
read -r -a SKIP <<< "$SKIP_PLATFORMS"

# Python scripts to be used as tests for the installed wheel. This list of files
# has been taken from the 'test_python' make target.
TESTS=(
    "${SRC_ROOT}/examples/python/hidato_table.py"
    "${SRC_ROOT}/examples/python/tsp.py"
    "${SRC_ROOT}/examples/python/pyflow_example.py"
    "${SRC_ROOT}/examples/python/knapsack.py"
    "${SRC_ROOT}/examples/python/linear_programming.py"
    "${SRC_ROOT}/examples/python/integer_programming.py"
    "${SRC_ROOT}/examples/tests/test_cp_api.py"
    "${SRC_ROOT}/examples/tests/test_lp_api.py"
)

(
    >&2 echo "BUILD_ROOT=${BUILD_ROOT}"
    >&2 echo "SRC_ROOT=${SRC_ROOT}"
    >&2 echo "EXPORT_ROOT=${EXPORT_ROOT}"
    >&2 echo "SKIP_PLATFORMS=( ${SKIP[*]} )"
    >&2 echo "TESTS=( ${TESTS[*]} )"
)

###############################################################################
# Main

mkdir -p "${BUILD_ROOT}"
mkdir -p "${EXPORT_ROOT}"

# Make third_party if needed
if [ ! -f "${SRC_ROOT}/Makefile.local" ]; then
  (>&2 echo "\${SRC_ROOT}/Makefile.local doesn't exist, building third_party")
  cd "$SRC_ROOT"
  make third_party
fi

# For each python platform provided by manylinux, build, export and test
# artifacts.
for PYROOT in /opt/python/*
do
    PYTAG=$(basename "$PYROOT")
    # Check for platforms to be skipped
    _skip=$(contains_element "$PYTAG" "${SKIP[@]}")
    if [ "$_skip" -eq '0' ]; then
        (>&2 echo "skipping deprecated platform $PYTAG")
        continue
    fi
    ###
    ### Wheel build
    ###
    # Create and activate virtualenv
    PYBIN="${PYROOT}/bin"
    "${PYBIN}/pip" install virtualenv
    "${PYBIN}/virtualenv" -p "${PYBIN}/python" "${BUILD_ROOT}/${PYTAG}"
    # shellcheck source=/dev/null
    source "${BUILD_ROOT}/${PYTAG}/bin/activate"
    pip install -U pip setuptools wheel six  # six is needed by make test_python
    # Build artifact
    export_manylinux_wheel "$SRC_ROOT" "$EXPORT_ROOT"
    # Ensure everything is clean (don't clean third_party anyway,
    # it has been built once)
    cd "$SRC_ROOT"
    make clean_python
    # Restore environment
    deactivate
    ###
    ### Wheel test
    ###
    WHEEL_FILE=$(echo "${EXPORT_ROOT}"/*-"${PYTAG}"-*.whl)
    # Create and activate a new virtualenv
    "${PYBIN}/virtualenv" -p "${PYBIN}/python" "${BUILD_ROOT}/${PYTAG}-test"
    # shellcheck source=/dev/null
    source "${BUILD_ROOT}/${PYTAG}-test/bin/activate"
    pip install -U pip setuptools wheel six
    # Install wheel and run tests
    pip install --no-cache-dir "$WHEEL_FILE"
    test_installed "${TESTS[@]}"
    # Restore environment
    deactivate
done
