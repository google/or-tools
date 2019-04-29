#!/usr/bin/env bash
set -x
set -e

# Check all prerequisite
# cc
command -v cmake | xargs echo "cmake: " | tee test.log
command -v make | xargs echo "make: " | tee -a test.log
command -v swig | xargs echo "swig: " | tee -a test.log
# python
command -v python2.7 | xargs echo "python2.7: " | tee -a test.log
command -v python3.7 | xargs echo "python3.7: " | tee -a test.log

##################
##  PYTHON 2.7  ##
##################
echo Cleaning Python... | tee -a test.log
make clean_python
echo Cleaning Python...DONE | tee -a test.log

echo Rebuild Python2.7 pypi archive... | tee -a test.log
make python_package UNIX_PYTHON_VER=2.7
echo Rebuild Python2.7 pypi archive...DONE | tee -a test.log

echo Creating Python2.7 venv... | tee -a test.log
TEMP_DIR=temp_python2.7
VENV_DIR=${TEMP_DIR}/venv
python2.7 -m pip install --user virtualenv
python2.7 -m virtualenv ${VENV_DIR}
echo Creating Python2.7 venv...DONE | tee -a test.log

echo Installing ortools Python2.7 venv... | tee -a test.log
${VENV_DIR}/bin/python -m pip install ${TEMP_DIR}/ortools/dist/*.whl
echo Installing ortools Python2.7 venv...DONE | tee -a test.log

set +e
echo Testing ortools Python2.7... | tee -a test.log
(cd ${VENV_DIR}/bin && ./python -c "from ortools.linear_solver import pywraplp") 2>&1 | tee -a test.log
(cd ${VENV_DIR}/bin && ./python -c "from ortools.constraint_solver import pywrapcp") 2>&1 | tee -a test.log
(cd ${VENV_DIR}/bin && ./python -c "from ortools.sat import pywrapsat") 2>&1 | tee -a test.log
(cd ${VENV_DIR}/bin && ./python -c "from ortools.graph import pywrapgraph") 2>&1 | tee -a test.log
(cd ${VENV_DIR}/bin && ./python -c "from ortools.algorithms import pywrapknapsack_solver") 2>&1 | tee -a test.log
cp test.py.in ${VENV_DIR}/test.py
${VENV_DIR}/bin/python ${VENV_DIR}/test.py 2>&1 | tee -a test.log
echo Testing ortools Python2.7...DONE | tee -a test.log
set -e

##################
##  PYTHON 3.7  ##
##################
echo Cleaning Python... | tee -a test.log
make clean_python
echo Cleaning Python...DONE | tee -a test.log

echo Rebuild Python3.7 pypi archive... | tee -a test.log
make python_package UNIX_PYTHON_VER=3.7
echo Rebuild Python3.7 pypi archive...DONE | tee -a test.log

echo Creating Python3.7 venv... | tee -a test.log
TEMP_DIR=temp_python3.7
VENV_DIR=${TEMP_DIR}/venv
python3.7 -m pip install --user virtualenv
python3.7 -m virtualenv ${VENV_DIR}
echo Creating Python3.7 venv...DONE | tee -a test.log

echo Installing ortools Python3.7 venv... | tee -a test.log
${VENV_DIR}/bin/python -m pip install ${TEMP_DIR}/ortools/dist/*.whl
echo Installing ortools Python3.7 venv...DONE | tee -a test.log

set +e
echo Testing ortools Python3.7... | tee -a test.log
(cd ${VENV_DIR}/bin && ./python -c "from ortools.linear_solver import pywraplp") 2>&1 | tee -a test.log
(cd ${VENV_DIR}/bin && ./python -c "from ortools.constraint_solver import pywrapcp") 2>&1 | tee -a test.log
(cd ${VENV_DIR}/bin && ./python -c "from ortools.sat import pywrapsat") 2>&1 | tee -a test.log
(cd ${VENV_DIR}/bin && ./python -c "from ortools.graph import pywrapgraph") 2>&1 | tee -a test.log
(cd ${VENV_DIR}/bin && ./python -c "from ortools.algorithms import pywrapknapsack_solver") 2>&1 | tee -a test.log
cp test.py.in ${VENV_DIR}/test.py
${VENV_DIR}/bin/python ${VENV_DIR}/test.py 2>&1 | tee -a test.log
echo Testing ortools Python3.7...DONE | tee -a test.log
set -e
