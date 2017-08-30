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

mkdir -p ${BUILD_ROOT}
mkdir -p ${EXPORT_ROOT}

echo "BUILD_ROOT=${BUILD_ROOT}"
echo "EXPORT_ROOT=${EXPORT_ROOT}"

################################################################################

function build_pypi_archives {
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
    make third_party
    ### Fix issue with or-tools makefile not looking for libraries where
    ### protobuffer has been installed.
    ### cp -prv dependencies/install/lib64/* dependencies/install/lib/
    make install_python_modules
    make python
    make pypi_archive
    # Build and repair wheels
    cd temp-python*/ortools
    python setup.py bdist_wheel
    cd dist
    auditwheel repair *.whl -w "$_EXPORT_ROOT"
    # Ensure everything is clean
    cd "$_SRC_ROOT"
    make clean
    make clean_third_party
}

###############################################################################

# Retrieve or-tools, we are building the master
cd "$BUILD_ROOT"
git clone https://github.com/google/or-tools

# TODO remove all patching, write Makefile targets for manylinux

# Patch makefile, remove offending command
# (the setup.py-e file doesn't exist)
cd "$BUILD_ROOT/or-tools"
sed -i -e 's=-rm $(PYPI_ARCHIVE_TEMP_DIR)/ortools/setup.py-e==g' makefiles/Makefile.python.mk

# Patch makefile, remove manual libortools.so grafting
# and RPATH setting. All of this stuff will be carried
# out by auditwheel, otherwise we would end up with a
# broken manylinux wheel
cd "$BUILD_ROOT/or-tools"
sed -i -e 's=cp lib/libortools=#=g' makefiles/Makefile.python.mk
sed -i -e 's=tools/fix_python_libraries_on_linux.sh=#=g' makefiles/Makefile.python.mk

# For each python platform provided by manylinux,
# build and export artifact using a virtualenv
cd "$BUILD_ROOT"
for PYROOT in /opt/python/*
do
    PYTAG=$(basename "$PYROOT")
    PYBIN="${PYROOT}/bin"
    "${PYBIN}/pip" install virtualenv
    "${PYBIN}/virtualenv" -p "${PYBIN}/python" ${BUILD_ROOT}/${PYTAG}
    source ${BUILD_ROOT}/${PYTAG}/bin/activate
    pip install -U pip setuptools
    build_pypi_archives "$BUILD_ROOT/or-tools" "$EXPORT_ROOT"
    deactivate
done
