#!/bin/bash
# Build all the wheel artifacts for the platforms supported by manylinux1
# and export them to the export location.
#
# Arguments:
#
#   $1 the build process root directory. If not specified as argument,
#      the env var BUILD_ROOT will be used. If not specified at all, a
#      default location will be used.
#
#   $2 the artifacts export directory. If not specified as argument,
#      the env var EXPORT_ROOT will be used. If not specified at all, a
#      default location will be used.
set -e

if [ -n "$1" ]; then BUILD_ROOT="$1"; fi
if [ -n "$2" ]; then EXPORT_ROOT="$2"; fi

if [ -z "$BUILD_ROOT" ]
then
    echo "\$BUILD_ROOT is not set, using default location: $HOME"
    BUILD_ROOT="$HOME"
fi

if [ -z "$EXPORT_ROOT" ]
then
    echo "\$EXPORT_ROOT is not set, using default location: ${HOME}/export"
    EXPORT_ROOT="${HOME}/export"
fi

SRC_ROOT="${BUILD_ROOT}/or-tools"

# Platforms to be ignored.
# The build of Python 2.6.x bindings is known to be broken
# (and 2.6 itself is deprecated).
SKIP_PLATFORMS=(
    cp26-cp26m
    cp26-cp26mu
)

# Python scripts to be used as tests for the installed wheel.
# This list of files has been taken from 'test_python' make target.
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

echo "BUILD_ROOT=${BUILD_ROOT}"
echo "SRC_ROOT=${SRC_ROOT}"
echo "EXPORT_ROOT=${EXPORT_ROOT}"
echo "SKIP_PLATFORMS=${SKIP_PLATFORMS[@]}"
echo "TESTS=${TESTS[@]}"

################################################################################

contains_element () {
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
    # Build all the wheel artifacts assuming that the current 'python'
    # command is the target interpreter. Please note that in order to
    # have or-tools correctly detect the target python version, this
    # function should be run in a virtualenv.
    # Arguments:
    #   $1 the or-tools sources root directory
    #   $2 the artifacts export directory
    if [ "$#" -ne 2 ]; then
        echo "build_pypi_archives called with an illegal number of parameters"
        exit 1  # TODO return error and check it outside
    fi
    _SRC_ROOT="$1"
    _EXPORT_ROOT="$2"
    # Build
    cd "$_SRC_ROOT"
    # We need to force this target, otherwise the protobuf stub will be missing
    # (for the makefile, it exists even if previously generated for another
    # platform)
    make -B install_python_modules  # regenerates Makefile.local
    make python
    make test_python
    make pypi_archive
    # Build and repair wheels
    cd temp-python*/ortools
    python setup.py bdist_wheel
    cd dist
    auditwheel repair *.whl -w "$_EXPORT_ROOT"
}

function test_installed {
    cd $(mktemp -d) # ensure we are not importing something from $PWD
    for testfile in "${TESTS[@]}"
    do
        python "$testfile"
    done
}

###############################################################################

mkdir -p "${BUILD_ROOT}"
mkdir -p "${EXPORT_ROOT}"

# Retrieve or-tools if needed
if [ ! -d "$SRC_ROOT" ]
then
  echo "SRC_ROOT doesn't exist, retrieving source from: https://github.com/google/or-tools"
  git clone https://github.com/google/or-tools "$SRC_ROOT"
fi

# Make third_party if needed
if [ ! -f "${SRC_ROOT}/Makefile.local" ]
then
  echo "\${SRC_ROOT}/Makefile.local doesn't exist, building third_party"
  cd "$SRC_ROOT"
  make third_party
fi

# TODO remove all patching, write Makefile targets for manylinux

# Patch makefile, remove offending command
# (the setup.py-e file doesn't exist)
cd "$SRC_ROOT"
sed -i -e 's=-rm $(PYPI_ARCHIVE_TEMP_DIR)/ortools/setup.py-e==g' makefiles/Makefile.python.mk

# Patch makefile, remove manual libortools.so grafting
# and RPATH setting. All of this stuff will be carried
# out by auditwheel, otherwise we would end up with a
# broken manylinux wheel file
cd "$SRC_ROOT"
sed -i -e 's=cp lib/libortools=#=g' makefiles/Makefile.python.mk
sed -i -e 's=tools/fix_python_libraries_on_linux.sh=#=g' makefiles/Makefile.python.mk

# For each python platform provided by manylinux,
# build, export and test artifacts.
for PYROOT in /opt/python/*
do
    PYTAG=$(basename "$PYROOT")
    # Check for platforms to be skipped
    SKIP=$(contains_element "$PYTAG" "${SKIP_PLATFORMS[@]}")
    if [ $SKIP -eq '0' ]
    then
        echo "skipping deprecated platform $PYTAG"
        continue
    fi
    ###
    ### Wheel build
    ###
    # Create and activate virtualenv
    PYBIN="${PYROOT}/bin"
    "${PYBIN}/pip" install virtualenv
    "${PYBIN}/virtualenv" -p "${PYBIN}/python" "${BUILD_ROOT}/${PYTAG}"
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
    source "${BUILD_ROOT}/${PYTAG}-test/bin/activate"
    pip install -U pip setuptools wheel six
    # Install wheel and run tests
    pip install --no-cache-dir $WHEEL_FILE
    test_installed
    # Restore environment
    deactivate
done
