#!/usr/bin/env bash
set -euxo pipefail

if [[ -z "${DOTNET_SNK}" ]]; then
  echo "DOTNET_SNK: not found !" | tee build.log
  exit 1
fi

# Clean everything
make clean
make clean_third_party

# Print version
make print-OR_TOOLS_VERSION | tee build.log

# Check all prerequisite
# cc
command -v gcc | xargs echo "gcc: " | tee -a build.log
command -v cmake | xargs echo "cmake: " | tee -a build.log
command -v make | xargs echo "make: " | tee -a build.log
command -v swig | xargs echo "swig: " | tee -a build.log

# python
command -v python3 | xargs echo "python3: " | tee -a build.log

# java
# maven require JAVA_HOME
if [[ -z "${JAVA_HOME}" ]]; then
  echo "JAVA_HOME: not found !" | tee build.log
  exit 1
else
  echo "JAVA_HOME: ${JAVA_HOME}" | tee -a build.log
  command -v java | xargs echo "java: " | tee -a build.log
  command -v javac | xargs echo "javac: " | tee -a build.log
  command -v jar | xargs echo "jar: " | tee -a build.log
  command -v mvn | xargs echo "mvn: " | tee -a build.log
fi

# dotnet
command -v dotnet | xargs echo "dotnet: " | tee -a build.log

###############################
##  Build Examples Archives  ##
###############################
rm -rf temp ./*.tar.gz
echo -n "Build examples archives..." | tee -a build.log
echo -n "  C++ examples archive..." | tee -a build.log
make cc_examples_archive UNIX_PYTHON_VER=3
echo -n "  Python examples archive..." | tee -a build.log
make python_examples_archive UNIX_PYTHON_VER=3
echo -n "  Java examples archive..." | tee -a build.log
make java_examples_archive UNIX_PYTHON_VER=3
echo -n "  .Net examples archive..." | tee -a build.log
make dotnet_examples_archive UNIX_PYTHON_VER=3
echo "DONE" | tee -a build.log

#########################
##  Build Third Party  ##
#########################
echo -n "Build Third Party..." | tee -a build.log
make third_party UNIX_PYTHON_VER=3
echo "DONE" | tee -a build.log

#####################
##  C++/Java/.Net  ##
#####################
echo -n "Build C++..." | tee -a build.log
make cc -l 4 UNIX_PYTHON_VER=3
echo "DONE" | tee -a build.log
#make test_cc -l 4 UNIX_PYTHON_VER=3
#echo "make test_cc: DONE" | tee -a build.log

echo -n "Build flatzinc..." | tee -a build.log
make fz -l 4 UNIX_PYTHON_VER=3
echo "DONE" | tee -a build.log

echo -n "Build Java..." | tee -a build.log
make java -l 4 UNIX_PYTHON_VER=3
echo "DONE" | tee -a build.log
#make test_java -l 4 UNIX_PYTHON_VER=3
#echo "make test_java: DONE" | tee -a build.log

echo -n "Build .Net..." | tee -a build.log
make dotnet -l 4 UNIX_PYTHON_VER=3
echo "DONE" | tee -a build.log
#make test_dotnet -l 4 UNIX_PYTHON_VER=3
#echo "make test_dotnet: DONE" | tee -a build.log

# Create Archive
echo -n "Make archive..." | tee -a build.log
make archive UNIX_PYTHON_VER=3
echo "DONE" | tee -a build.log
echo -n "Test archive..." | tee -a build.log
make test_archive UNIX_PYTHON_VER=3
echo "DONE" | tee -a build.log

echo -n "Make flatzinc archive..." | tee -a build.log
make fz_archive UNIX_PYTHON_VER=3
echo "DONE" | tee -a build.log
echo -n "Test flatzinc archive..." | tee -a build.log
make test_fz_archive UNIX_PYTHON_VER=3
echo "DONE" | tee -a build.log

################
##  Python 3  ##
################
echo -n "Cleaning Python..." | tee -a build.log
make clean_python UNIX_PYTHON_VER=3
echo "DONE" | tee -a build.log

echo -n "Build Python 3..." | tee -a build.log
make python -l 4 UNIX_PYTHON_VER=3
echo "DONE" | tee -a build.log
#make test_python UNIX_PYTHON_VER=3
#echo "make test_python3: DONE" | tee -a build.log
echo -n "Build Python 3 wheel archive..." | tee -a build.log
make package_python UNIX_PYTHON_VER=3
echo "DONE" | tee -a build.log
echo -n "Test Python 3 wheel archive..." | tee -a build.log
make test_package_python UNIX_PYTHON_VER=3
echo "DONE" | tee -a build.log

cp temp_python3/ortools/dist/*.whl .
