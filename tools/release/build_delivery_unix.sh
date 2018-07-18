#!/bin/bash
set -x
set -e

# Check all prerequisite
# cc
which cmake | xargs echo "cmake: " | tee build.log
which make | xargs echo "make: " | tee -a build.log
which swig | xargs echo "swig: " | tee -a build.log
# python
which python2 | xargs echo "python2: " | tee -a build.log
which python3 | xargs echo "python3: " | tee -a build.log
# java
which java | xargs echo "java: " | tee -a build.log
which javac | xargs echo "javac: " | tee -a build.log
which jar | xargs echo "jar: " | tee -a build.log
# C#
which mcs | xargs echo "mcs: " | tee -a build.log
which mono | xargs echo "mono: " | tee -a build.log
# F#
which fsharpc | xargs echo "fsharpc: " | tee -a build.log

# Build Third Party
make clean_third_party
make third_party UNIX_PYTHON_VER=3
echo "make third_party: DONE" | tee -a build.log

# Building OR-Tools
make clean
make cc -l 4 UNIX_PYTHON_VER=3
echo "make cc: DONE" | tee -a build.log
make test_cc -l 4 UNIX_PYTHON_VER=3
echo "make test_cc: DONE" | tee -a build.log

make python -l 4 UNIX_PYTHON_VER=3
echo "make python3: DONE" | tee -a build.log
make test_python -l 4 UNIX_PYTHON_VER=3
echo "make test_python3: DONE" | tee -a build.log

make java -l 4 UNIX_PYTHON_VER=3
echo "make java: DONE" | tee -a build.log
make test_java -l 4 UNIX_PYTHON_VER=3
echo "make test_java: DONE" | tee -a build.log

make dotnet -l 4 UNIX_PYTHON_VER=3
echo "make dotnet: DONE" | tee -a build.log
make test_dotnet -l 4 UNIX_PYTHON_VER=3
echo "make test_dotnet: DONE" | tee -a build.log

make fz -l 4 UNIX_PYTHON_VER=3
echo "make fz: DONE" | tee -a build.log

# Create Archive
rm -rf temp ./*.tar.gz
make archive UNIX_PYTHON_VER=3
echo "make archive: DONE" | tee -a build.log
make fz_archive UNIX_PYTHON_VER=3
echo "make fz_archive: DONE" | tee -a build.log
make python_examples_archive UNIX_PYTHON_VER=3
echo "make python_examples_archive: DONE" | tee -a build.log


# Rebuilding for Python 2
make clean_python UNIX_PYTHON_VER=2
make python -l 4 UNIX_PYTHON_VER=2
echo "make python2: DONE" | tee -a build.log
make test_python UNIX_PYTHON_VER=2
echo "make test_python2: DONE" | tee -a build.log
make pypi_archive UNIX_PYTHON_VER=2
echo "make pypi_archive2: DONE" | tee -a build.log

# Rebuilding for Python 3...
make clean_python UNIX_PYTHON_VER=3
make python -l 4 UNIX_PYTHON_VER=3
echo "make python3: DONE" | tee -a build.log
make test_python UNIX_PYTHON_VER=3
echo "make test_python3: DONE" | tee -a build.log
make pypi_archive UNIX_PYTHON_VER=3
echo "make pypi_archive3: DONE" | tee -a build.log
