#!/usr/bin/env bash
set -x
set -e

# Check all prerequisite
# cc
command -v cmake | xargs echo "cmake: " | tee build.log
command -v make | xargs echo "make: " | tee -a build.log
command -v swig | xargs echo "swig: " | tee -a build.log
# python
command -v python2 | xargs echo "python2: " | tee -a build.log
command -v python3 | xargs echo "python3: " | tee -a build.log

echo Creating Python 2 venv...
TEMP_DIR=temp_python2
VENV_DIR=${TEMP_DIR}/venv
python2 -m pip install --user virtualenv
python2 -m virtualenv -p python2 ${VENV_DIR}
# Bug: setup.py must be run in this directory !
(cd ${TEMP_DIR}/ortools && ../venv/bin/python setup.py install)
cp test.py.in ${TEMP_DIR}/venv/test.py
echo Creating Python 2 venv...DONE

echo Creating Python 3 venv...
TEMP_DIR=temp_python3
VENV_DIR=${TEMP_DIR}/venv
python3 -m pip install --user virtualenv
python3 -m virtualenv -p python3 ${VENV_DIR}
# Bug: setup.py must be run in this directory !
(cd ${TEMP_DIR}/ortools && ../venv/bin/python setup.py install)
cp test.py.in ${TEMP_DIR}/venv/test.py
echo Creating Python 3 venv...DONE

# To be sure to have sandboxed library (i.e. @loader_path)
make clean_cc

echo Testing in virtualenv...
temp_python2/venv/bin/python temp_python2/venv/test.py
temp_python3/venv/bin/python temp_python3/venv/test.py
echo Testing in virtualenv...DONE
