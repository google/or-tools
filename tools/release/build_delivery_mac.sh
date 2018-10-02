#!/usr/bin/env bash
set -x
set -e

# Clean everything
make clean
make clean_third_party

# Print version
make print-OR_TOOLS_VERSION | tee build.log

# Check all prerequisite
# cc
command -v clang
command -v clang | xargs echo "clang: " | tee -a build.log
command -v cmake
command -v cmake | xargs echo "cmake: " | tee -a build.log
command -v make
command -v make | xargs echo "make: " | tee -a build.log
command -v swig
command -v swig | xargs echo "swig: " | tee -a build.log
# python
command -v python2.7
command -v python2.7 | xargs echo "python2.7: " | tee -a build.log
command -v python3.7
command -v python3.7 | xargs echo "python3.7: " | tee -a build.log
# java
echo "JAVA_HOME: ${JAVA_HOME}" | tee -a build.log
command -v java
command -v java | xargs echo "java: " | tee -a build.log
command -v javac
command -v javac | xargs echo "javac: " | tee -a build.log
command -v jar
command -v jar | xargs echo "jar: " | tee -a build.log
# C#
command -v dotnet
command -v dotnet | xargs echo "dotnet: " | tee -a build.log

#########################
##  Build Third Party  ##
#########################
echo -n "Build Third Party..." | tee -a build.log
make third_party UNIX_PYTHON_VER=3.7
echo "DONE" | tee -a build.log

#####################
##  C++/Java/.Net  ##
#####################
echo -n "Build C++..." | tee -a build.log
make cc -l 4 UNIX_PYTHON_VER=3.7
echo "DONE" | tee -a build.log
#make test_cc -l 4 UNIX_PYTHON_VER=3.7
#echo "make test_cc: DONE" | tee -a build.log

echo -n "Build flatzinc..." | tee -a build.log
make fz -l 4 UNIX_PYTHON_VER=3.7
echo "DONE" | tee -a build.log

echo -n "Build Java..." | tee -a build.log
make java -l 4 UNIX_PYTHON_VER=3.7
echo "DONE" | tee -a build.log
#make test_java -l 4 UNIX_PYTHON_VER=3.7
#echo "make test_java: DONE" | tee -a build.log

echo -n "Build .Net..." | tee -a build.log
make dotnet -l 4 UNIX_PYTHON_VER=3.7
echo "DONE" | tee -a build.log
#make test_dotnet -l 4 UNIX_PYTHON_VER=3.7
#echo "make test_dotnet: DONE" | tee -a build.log

# Create Archive
rm -rf temp ./*.tar.gz
echo -n "Make archive..." | tee -a build.log
make archive UNIX_PYTHON_VER=3.7
echo "DONE" | tee -a build.log
echo -n "Test archive..." | tee -a build.log
make test_archive UNIX_PYTHON_VER=3.7
echo "DONE" | tee -a build.log

echo -n "Make flatzinc archive..." | tee -a build.log
make fz_archive UNIX_PYTHON_VER=3.7
echo "DONE" | tee -a build.log
echo -n "Test flatzinc archive..." | tee -a build.log
make test_fz_archive UNIX_PYTHON_VER=3.7
echo "DONE" | tee -a build.log

echo -n "Make Python examples archive..." | tee -a build.log
make python_examples_archive UNIX_PYTHON_VER=3.7
echo "DONE" | tee -a build.log

##################
##  Python 2.7  ##
##################
echo -n "Cleaning Python..." | tee -a build.log
make clean_python UNIX_PYTHON_VER=2.7
echo "DONE" | tee -a build.log

echo -n "Build Python 2.7..." | tee -a build.log
make python -l 4 UNIX_PYTHON_VER=2.7
echo "DONE" | tee -a build.log
#make test_python UNIX_PYTHON_VER=2.7
#echo "make test_python2.7: DONE" | tee -a build.log
echo -n "Build Python 2.7 wheel archive..." | tee -a build.log
make pypi_archive UNIX_PYTHON_VER=2.7
echo "DONE" | tee -a build.log
echo -n "Test Python 2.7 wheel archive..." | tee -a build.log
make test_pypi_archive UNIX_PYTHON_VER=2.7
echo "DONE" | tee -a build.log

cp temp_python2.7/ortools/dist/*.whl .

##################
##  Python 3.7  ##
##################
echo -n "Cleaning Python..." | tee -a build.log
make clean_python UNIX_PYTHON_VER=3.7
echo "DONE" | tee -a build.log

echo -n "Build Python 3.7..." | tee -a build.log
make python -l 4 UNIX_PYTHON_VER=3.7
echo "DONE" | tee -a build.log
#make test_python UNIX_PYTHON_VER=3.7
#echo "make test_python3.7: DONE" | tee -a build.log
echo -n "Build Python 3.7 wheel archive..." | tee -a build.log
make pypi_archive UNIX_PYTHON_VER=3.7
echo "DONE" | tee -a build.log
echo -n "Test Python 3.7 wheel archive..." | tee -a build.log
make test_pypi_archive UNIX_PYTHON_VER=3.7
echo "DONE" | tee -a build.log

cp temp_python3.7/ortools/dist/*.whl .
