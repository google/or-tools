#!/usr/bin/env bash
set -euxo pipefail

# Check all prerequisite
# cc
command -v cmake | xargs echo "cmake: " | tee test.log
command -v make | xargs echo "make: " | tee -a test.log
command -v swig | xargs echo "swig: " | tee -a test.log
# python
command -v python3 | xargs echo "python3: " | tee -a test.log

##################
##  PYTHON 3.X  ##
##################
echo Cleaning Python... | tee -a test.log
make clean_python
echo Cleaning Python...DONE | tee -a test.log

echo Rebuild Python3 pypi archive... | tee -a test.log
make package_python UNIX_PYTHON_VER=3
echo Rebuild Python3 pypi archive...DONE | tee -a test.log

echo Creating Python3 venv... | tee -a test.log
TEMP_DIR=temp_python3
VENV_DIR=${TEMP_DIR}/venv
python3 -m pip install --user virtualenv
python3 -m virtualenv ${VENV_DIR}
echo Creating Python3 venv...DONE | tee -a test.log

echo Installing ortools Python3 venv... | tee -a test.log
${VENV_DIR}/bin/python -m pip install ${TEMP_DIR}/ortools/dist/*.whl
echo Installing ortools Python3 venv...DONE | tee -a test.log

set +e
echo Testing ortools Python3... | tee -a test.log
(cd ${VENV_DIR}/bin && ./python -c "from ortools.linear_solver import pywraplp") 2>&1 | tee -a test.log
(cd ${VENV_DIR}/bin && ./python -c "from ortools.constraint_solver import pywrapcp") 2>&1 | tee -a test.log
(cd ${VENV_DIR}/bin && ./python -c "from ortools.sat import pywrapsat") 2>&1 | tee -a test.log
(cd ${VENV_DIR}/bin && ./python -c "from ortools.graph import pywrapgraph") 2>&1 | tee -a test.log
(cd ${VENV_DIR}/bin && ./python -c "from ortools.algorithms import pywrapknapsack_solver") 2>&1 | tee -a test.log
cp test.py.in ${VENV_DIR}/test.py
${VENV_DIR}/bin/python ${VENV_DIR}/test.py 2>&1 | tee -a test.log
echo Testing ortools Python3...DONE | tee -a test.log
set -e
