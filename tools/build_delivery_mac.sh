#!/bin/bash

echo Cleaning or-tools
make clean
echo Builing all libraries
make all fz -j 5 UNIX_PYTHON_VER=2.7
echo Running tests
make test UNIX_PYTHON_VER=2.7
echo Creating standard artifacts.
rm -rf temp-python*
make python_examples_archive archive fz_archive pypi_upload UNIX_PYTHON_VER=2.7
make clean_python UNIX_PYTHON_VER=2.7
echo Rebuilding for python 3.5
rm -rf temp-python*
make python UNIX_PYTHON_VER=3.5 -j 5
make test_python UNIX_PYTHON_VER=3.5
make pypi_upload UNIX_PYTHON_VER=3.5
make clean_python UNIX_PYTHON_VER=3.5
echo Rebuilding for python 3.6
rm -rf temp-python*
make python UNIX_PYTHON_VER=3.6 -j 5
make test_python UNIX_PYTHON_VER=3.6
make pypi_upload UNIX_PYTHON_VER=3.6
make clean_python UNIX_PYTHON_VER=3.6
